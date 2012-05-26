// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#include "ui/base/win/message_box_win.h"

namespace browser {

MessageBoxResult ShowMessageBox(gfx::NativeWindow parent,
                                const string16& title,
                                const string16& message,
                                MessageBoxType type) {
  UINT flags = MB_SETFOREGROUND;
  flags |= ((type == MESSAGE_BOX_TYPE_QUESTION) ? MB_YESNO : MB_OK);
  flags |= ((type == MESSAGE_BOX_TYPE_INFORMATION) ?
      MB_ICONINFORMATION : MB_ICONWARNING);
  return (ui::MessageBox(parent, message, title, flags) == IDNO) ?
      MESSAGE_BOX_RESULT_NO : MESSAGE_BOX_RESULT_YES;
}

}  // namespace browser
