// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#include "ui/base/win/message_box_win.h"

namespace browser {

void ShowMessageBox(gfx::NativeWindow parent,
                    const string16& title,
                    const string16& message,
                    MessageBoxType type) {
  UINT flags = MB_OK | MB_SETFOREGROUND | MB_TOPMOST;
  if (type == MESSAGE_BOX_TYPE_WARNING)
    flags |= MB_ICONWARNING;
  else
    flags |= MB_ICONINFORMATION;
  ui::MessageBox(parent, message, title, flags);
}

bool ShowQuestionMessageBox(gfx::NativeWindow parent,
                            const string16& title,
                            const string16& message) {
  const UINT flags = MB_YESNO | MB_ICONWARNING | MB_SETFOREGROUND;
  return ui::MessageBox(parent, message, title, flags) == IDYES;
}

}  // namespace browser
