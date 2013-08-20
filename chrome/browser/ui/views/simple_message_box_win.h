// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIMPLE_MESSAGE_BOX_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_SIMPLE_MESSAGE_BOX_WIN_H_

#include "chrome/browser/ui/simple_message_box.h"

namespace chrome {

MessageBoxResult NativeShowMessageBox(HWND parent,
                                      const string16& title,
                                      const string16& message,
                                      MessageBoxType type);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_SIMPLE_MESSAGE_BOX_WIN_H_
