// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/simple_message_box.h"

#include "ui/base/message_box_win.h"

namespace browser {

void ShowErrorBox(gfx::NativeWindow parent,
                  const string16& title,
                  const string16& message) {
  UINT flags = MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST;
  ui::MessageBox(parent, message, title, flags);
}

bool ShowYesNoBox(gfx::NativeWindow parent,
                  const string16& title,
                  const string16& message) {
  UINT flags = MB_YESNO | MB_ICONWARNING | MB_SETFOREGROUND;
  int result = ui::MessageBox(parent, message, title, flags);
  return result == IDYES;
}

}  // namespace browser
