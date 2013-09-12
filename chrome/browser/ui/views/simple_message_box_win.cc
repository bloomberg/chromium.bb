// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#include "components/startup_metric_utils/startup_metric_utils.h"
#include "ui/base/win/message_box_win.h"
#include "ui/gfx/win/hwnd_util.h"

namespace chrome {

MessageBoxResult NativeShowMessageBox(HWND parent,
                                      const string16& title,
                                      const string16& message,
                                      MessageBoxType type) {
  UINT flags = MB_SETFOREGROUND;
  if (type == MESSAGE_BOX_TYPE_QUESTION) {
    flags |= MB_YESNO;
  } else if (type == MESSAGE_BOX_TYPE_OK_CANCEL) {
    flags |= MB_OKCANCEL;
  } else {
    flags |= MB_OK;
  }
  flags |= ((type == MESSAGE_BOX_TYPE_INFORMATION) ?
      MB_ICONINFORMATION : MB_ICONWARNING);
  int result = ui::MessageBox(parent, message, title, flags);
  return (result == IDYES || result == IDOK) ?
      MESSAGE_BOX_RESULT_YES : MESSAGE_BOX_RESULT_NO;
}

#if !defined(USE_AURA)
MessageBoxResult ShowMessageBox(gfx::NativeWindow parent,
                                const string16& title,
                                const string16& message,
                                MessageBoxType type) {
  startup_metric_utils::SetNonBrowserUIDisplayed();

  if (!parent)
    parent = gfx::GetWindowToParentTo(true);

  return NativeShowMessageBox(parent, title, message, type);
}
#endif

}  // namespace chrome
