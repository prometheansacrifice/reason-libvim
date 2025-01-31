#include "libvim.h"
#include <caml/alloc.h>
#include <caml/bigarray.h>
#include <caml/callback.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <caml/threads.h>

#define Val_none Val_int(0)

static value Val_some(value v) {
  CAMLparam1(v);
  CAMLlocal1(some);
  some = caml_alloc(1, 0);
  Store_field(some, 0, v);
  CAMLreturn(some);
}

static value Val_highlight(searchHighlight_T hl) {
  CAMLparam0();
  CAMLlocal1(ret);

  ret = caml_alloc(4, 0);
  Store_field(ret, 0, Val_int(hl.start.lnum));
  Store_field(ret, 1, Val_int(hl.start.col));
  Store_field(ret, 2, Val_int(hl.end.lnum));
  Store_field(ret, 3, Val_int(hl.end.col));

  CAMLreturn(ret);
}

void onBufferChanged(bufferUpdate_T bu) {
  static value *lv_onBufferChanged = NULL;

  if (lv_onBufferChanged == NULL) {
    lv_onBufferChanged = caml_named_value("lv_onBufferChanged");
  }

  value *pArgs = (value *)malloc(sizeof(value) * 4);
  pArgs[0] = (value)bu.buf;
  pArgs[1] = Val_int(bu.lnum);
  pArgs[2] = Val_int(bu.lnume);
  pArgs[3] = Val_long(bu.xtra);

  caml_callbackN(*lv_onBufferChanged, 4, pArgs);

  free(pArgs);
}

void onAutocommand(event_T event, buf_T *buf) {
  static value *lv_onAutocmd = NULL;

  if (lv_onAutocmd == NULL) {
    lv_onAutocmd = caml_named_value("lv_onAutocommand");
  }

  caml_callback2(*lv_onAutocmd, Val_int(event), (value)buf);
}

void onDirectoryChanged(char_u *path) {
  CAMLparam0();
  CAMLlocal1(pathString);
  static value *lv_onDirectoryChanged = NULL;

  if (lv_onDirectoryChanged == NULL) {
    lv_onDirectoryChanged = caml_named_value("lv_onDirectoryChanged");
  }

  caml_acquire_runtime_system();
  pathString = caml_copy_string(path);
  caml_callback(*lv_onDirectoryChanged, pathString);
  caml_release_runtime_system();
  CAMLreturn0;
}

void onMessage(char_u *title, char_u *contents, msgPriority_T priority) {
  CAMLparam0();
  CAMLlocal2(titleString, contentsString);

  static value *lv_onMessage = NULL;

  if (lv_onMessage == NULL) {
    lv_onMessage = caml_named_value("lv_onMessage");
  }

  caml_acquire_runtime_system();
  titleString = caml_copy_string(title);
  contentsString = caml_copy_string(contents);
  caml_callback3(*lv_onMessage, Val_int(priority), titleString, contentsString);
  caml_release_runtime_system();
  CAMLreturn0;
}

void onQuit(buf_T *buf, int isForced) {
  CAMLparam0();
  CAMLlocal1(quitResult);

  static value *lv_onQuit = NULL;

  if (lv_onQuit == NULL) {
    lv_onQuit = caml_named_value("lv_onQuit");
  }

  caml_acquire_runtime_system();
  if (buf == NULL) {
    quitResult = Val_none;
  } else {
    quitResult = Val_some((value)buf);
  }
  caml_callback2(*lv_onQuit, quitResult,
                 isForced == TRUE ? Val_true : Val_false);
  caml_release_runtime_system();

  CAMLreturn0;
}

void onWindowMovement(windowMovement_T movementType, int count) {
  CAMLparam0();

  static value *lv_onWindowMovement = NULL;

  if (lv_onWindowMovement == NULL) {
    lv_onWindowMovement = caml_named_value("lv_onWindowMovement");
  }

  caml_acquire_runtime_system();
  caml_callback2(*lv_onWindowMovement, Val_int(movementType), Val_int(count));
  caml_release_runtime_system();
  CAMLreturn0;
}

void onWindowSplit(windowSplit_T splitType, char_u *path) {
  CAMLparam0();
  CAMLlocal1(pathString);

  static value *lv_onWindowSplit = NULL;

  if (lv_onWindowSplit == NULL) {
    lv_onWindowSplit = caml_named_value("lv_onWindowSplit");
  }

  caml_acquire_runtime_system();
  pathString = caml_copy_string(path);
  caml_callback2(*lv_onWindowSplit, Val_int(splitType), pathString);
  caml_release_runtime_system();
  CAMLreturn0;
}

CAMLprim value libvim_vimAutoClosingPairsSet(value acp) {
  CAMLparam1(acp);
  CAMLlocal1(val);

  int len = Wosize_val(acp);

  autoClosingPair_T *pAcp =
      (autoClosingPair_T *)malloc(sizeof(autoClosingPair_T) * len);
  if (pAcp != NULL) {

    for (int i = 0; i < len; i++) {
      val = Field(acp, i);
      int opening = Int_val(Field(val, 0));
      int closing = Int_val(Field(val, 1));
      pAcp[i].open = opening;
      pAcp[i].close = closing;
    };

    acp_set_pairs(pAcp, len);
    free(pAcp);
  }
  CAMLreturn(Val_unit);
}

CAMLprim value libvim_vimInit(value unit) {
  vimSetBufferUpdateCallback(&onBufferChanged);
  vimSetAutoCommandCallback(&onAutocommand);
  vimSetDirectoryChangedCallback(&onDirectoryChanged);
  vimSetMessageCallback(&onMessage);
  vimSetQuitCallback(&onQuit);
  vimSetWindowMovementCallback(&onWindowMovement);
  vimSetWindowSplitCallback(&onWindowSplit);

  char *args[0];
  vimInit(0, args);
  return Val_unit;
}

CAMLprim value libvim_vimInput(value v) {
  char_u *s;
  s = (char_u *)String_val(v);
  vimInput(s);
  return Val_unit;
}

CAMLprim value libvim_vimCommand(value v) {
  char_u *s;
  s = (char_u *)String_val(v);
  vimExecute(s);
  return Val_unit;
}

CAMLprim value libvim_vimGetMode(value unit) {
  int mode = vimGetMode();

  int val = 0;

  if ((mode & INSERT) == INSERT) {
    val = 1;
  } else if ((mode & CMDLINE) == CMDLINE) {
    val = 2;
  } else if ((mode & REPLACE_FLAG) == REPLACE_FLAG) {
    val = 3;
  } else if ((mode & VISUAL) == VISUAL) {
    val = 4;
  } else if ((mode & OP_PENDING) == OP_PENDING) {
    val = 5;
  }

  return Val_int(val);
}

CAMLprim value libvim_vimBufferGetId(value v) {
  buf_T *buf = (buf_T *)v;
  int id = vimBufferGetId(buf);
  return Val_int(id);
}

CAMLprim value libvim_vimBufferOpen(value v) {
  CAMLparam1(v);
  char_u *s;
  s = (char_u *)String_val(v);
  buf_T *buf = vimBufferOpen(s, 1, 0);
  value vbuf = (value)buf;
  CAMLreturn(vbuf);
}

CAMLprim value libvim_vimBufferGetById(value v) {
  CAMLparam1(v);
  CAMLlocal1(ret);
  buf_T *buf = vimBufferGetById(Int_val(v));

  if (!buf) {
    ret = Val_none;
  } else {
    value b = (value)buf;
    ret = Val_some(b);
  }

  CAMLreturn(ret);
}

CAMLprim value libvim_vimBufferGetFilename(value v) {
  CAMLparam1(v);
  CAMLlocal1(ret);
  buf_T *buf = (buf_T *)v;

  if (buf == NULL) {
    ret = Val_none;
  } else {
    char_u *fname = vimBufferGetFilename(buf);
    if (fname == NULL) {
      ret = Val_none;
    } else {
      ret = Val_some(caml_copy_string(fname));
    }
  }

  CAMLreturn(ret);
}

CAMLprim value libvim_vimBufferGetModified(value v) {
  buf_T *buf = (buf_T *)v;

  if (vimBufferGetModified(buf) == TRUE) {
    return Val_true;
  } else {
    return Val_false;
  }
}

CAMLprim value libvim_vimBufferGetChangedTick(value v) {
  buf_T *buf = (buf_T *)v;

  long tick = vimBufferGetLastChangedTick(buf);
  return Val_long(tick);
}

CAMLprim value libvim_vimBufferGetFiletype(value v) {
  CAMLparam1(v);
  CAMLlocal1(ret);
  buf_T *buf = (buf_T *)v;

  if (buf == NULL) {
    ret = Val_none;
  } else {
    char_u *ftype = vimBufferGetFiletype(buf);
    if (ftype == NULL) {
      ret = Val_none;
    } else {
      ret = Val_some(caml_copy_string(ftype));
    }
  }

  CAMLreturn(ret);
}

CAMLprim value libvim_vimSearchGetHighlights(value startLine, value endLine) {
  CAMLparam2(startLine, endLine);
  CAMLlocal1(ret);

  int start = Int_val(startLine);
  int end = Int_val(endLine);

  int num_highlights;
  searchHighlight_T *highlights;

  vimSearchGetHighlights(start, end, &num_highlights, &highlights);

  ret = caml_alloc(num_highlights, 0);

  for (int i = 0; i < num_highlights; i++) {
    Store_field(ret, i, Val_highlight(highlights[i]));
  }

  vim_free(highlights);
  CAMLreturn(ret);
}

CAMLprim value libvim_vimSearchGetMatchingPair(value unit) {
  CAMLparam0();
  CAMLlocal2(ret, v);

  v = caml_alloc(2, 0);

  pos_T *result = vimSearchGetMatchingPair(0);
  if (result == NULL) {
    ret = Val_none;
  } else {
    Store_field(v, 0, Val_int(result->lnum));
    Store_field(v, 1, Val_int(result->col));
    ret = Val_some(v);
  }

  CAMLreturn(ret);
}

CAMLprim value libvim_vimBufferGetCurrent(value unit) {
  return (value)vimBufferGetCurrent();
}

CAMLprim value libvim_vimBufferGetLineCount(value v) {
  buf_T *buf = (buf_T *)v;
  size_t count = vimBufferGetLineCount(buf);
  return Val_long(count);
}

CAMLprim value libvim_vimBufferGetLine(value vBuf, value vLine) {
  CAMLparam2(vBuf, vLine);
  CAMLlocal1(ret);
  buf_T *buf = (buf_T *)vBuf;
  int line = Int_val(vLine);

  char_u *c = vimBufferGetLine(buf, line);
  ret = caml_copy_string(c);

  CAMLreturn(ret);
}

CAMLprim value libvim_vimBufferSetCurrent(value v) {
  buf_T *buf = (buf_T *)v;

  vimBufferSetCurrent(buf);
  return Val_unit;
}

CAMLprim value libvim_vimCommandLineGetCompletions(value unit) {
  CAMLparam0();
  CAMLlocal1(ret);

  char **completions;
  int count;

  vimCommandLineGetCompletions(&completions, &count);

  ret = caml_alloc(count, 0);
  for (int i = 0; i < count; i++) {
    Store_field(ret, i, caml_copy_string(completions[i]));
  }

  if (completions != NULL) {
    vim_free(completions);
  }

  CAMLreturn(ret);
}

CAMLprim value libvim_vimCommandLineGetPosition(value unit) {
  CAMLparam0();

  int pos = vimCommandLineGetPosition();
  CAMLreturn(Val_int(pos));
}

CAMLprim value libvim_vimCommandLineGetText(value unit) {
  CAMLparam0();
  CAMLlocal1(ret);

  char_u *c = vimCommandLineGetText();
  if (c == NULL) {
    ret = Val_none;
  } else {
    ret = Val_some(caml_copy_string(c));
  }

  CAMLreturn(ret);
}

CAMLprim value libvim_vimCommandLineGetType(value unit) {
  CAMLparam0();
  int type = vimCommandLineGetType();

  int ret;
  switch (type) {
  case ':':
    ret = 0;
    break;
  case '/':
    ret = 1;
    break;
  case '?':
    ret = 2;
    break;
  default:
    ret = 3;
  }
  CAMLreturn(Val_int(ret));
}

CAMLprim value libvim_vimCursorGetLine(value unit) {
  int line = vimCursorGetLine();
  return Val_int(line);
}

CAMLprim value libvim_vimCursorGetColumn(value unit) {
  int column = vimCursorGetColumn();
  return Val_int(column);
}

CAMLprim value libvim_vimCursorSetPosition(value l, value c) {
  int line = Int_val(l);
  int column = Int_val(c);

  pos_T pos;
  pos.lnum = line;
  pos.col = column;

  vimCursorSetPosition(pos);

  return Val_unit;
}

CAMLprim value libvim_vimOptionSetAutoClosingPairs(value ts) {
  int tabSize = Int_val(ts);
  p_acp = tabSize;
}

CAMLprim value libvim_vimOptionSetTabSize(value ts) {
  int tabSize = Int_val(ts);
  vimOptionSetTabSize(tabSize);
}

CAMLprim value libvim_vimOptionSetInsertSpaces(value v) {
  int insertSpaces = Bool_val(v);
  vimOptionSetInsertSpaces(insertSpaces);
  return Val_unit;
}

CAMLprim value libvim_vimOptionGetAutoClosingPairs(value unit) {
  return Val_bool(p_acp);
}
CAMLprim value libvim_vimOptionGetInsertSpaces(value unit) {
  int insertSpaces = vimOptionGetInsertSpaces();
  return Val_bool(insertSpaces);
}

CAMLprim value libvim_vimOptionGetTabSize(value unit) {
  int tabSize = vimOptionGetTabSize();
  return Val_int(tabSize);
}

CAMLprim value libvim_vimVisualGetRange(value unit) {
  CAMLparam0();
  CAMLlocal1(ret);

  pos_T start;
  pos_T end;

  vimVisualGetRange(&start, &end);

  ret = caml_alloc(4, 0);
  Store_field(ret, 0, Val_int(start.lnum));
  Store_field(ret, 1, Val_int(start.col));
  Store_field(ret, 2, Val_int(end.lnum));
  Store_field(ret, 3, Val_int(end.col));

  CAMLreturn(ret);
}

CAMLprim value libvim_vimWindowGetWidth(value unit) {
  int width = vimWindowGetWidth();
  return Val_int(width);
}

CAMLprim value libvim_vimWindowGetHeight(value unit) {
  int height = vimWindowGetHeight();
  return Val_int(height);
}

CAMLprim value libvim_vimWindowGetTopLine(value unit) {
  int topline = vimWindowGetTopLine();
  return Val_int(topline);
}

CAMLprim value libvim_vimWindowGetLeftColumn(value unit) {
  int left = vimWindowGetLeftColumn();
  return Val_int(left);
}

CAMLprim value libvim_vimWindowSetWidth(value width) {
  int w = Int_val(width);
  vimWindowSetWidth(w);
  return Val_unit;
}

CAMLprim value libvim_vimWindowSetHeight(value height) {
  int h = Int_val(height);
  vimWindowSetHeight(h);
  return Val_unit;
}

CAMLprim value libvim_vimWindowSetTopLeft(value top, value left) {
  int t = Int_val(top);
  int l = Int_val(left);
  vimWindowSetTopLeft(t, l);
  return Val_unit;
}

CAMLprim value libvim_vimVisualGetType(value unit) {
  int ret;
  char v = vimVisualGetType();

  if (vimVisualIsActive() == FALSE) {
    ret = 3;
  } else {
    switch (v) {
    case 'v':
      ret = 0;
      break;
    case 'V':
      ret = 1;
      break;
    case Ctrl_V:
      ret = 2;
      break;
    default:
      ret = 3;
      break;
    };
  }

  return Val_int(ret);
}
