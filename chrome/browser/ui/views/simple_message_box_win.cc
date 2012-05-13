// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#include "ui/base/win/message_box_win.h"

namespace browser {

void ShowWarningMessageBox(gfx::NativeWindow parent,
                           const string16& title,
                           const string16& message) {
  const UINT flags = MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST;
  ui::MessageBox(parent, message, title, flags);
}

bool ShowQuestionMessageBox(gfx::NativeWindow parent,
                            const string16& title,
                            const string16& message) {
  const UINT flags = MB_YESNO | MB_ICONWARNING | MB_SETFOREGROUND;
  return ui::MessageBox(parent, message, title, flags) == IDYES;
}

}  // namespace browser
