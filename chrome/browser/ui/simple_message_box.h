// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIMPLE_MESSAGE_BOX_H_
#define CHROME_BROWSER_UI_SIMPLE_MESSAGE_BOX_H_
#pragma once

#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

namespace browser {

enum MessageBoxType {
  MESSAGE_BOX_TYPE_INFORMATION,
  MESSAGE_BOX_TYPE_WARNING,
};

// Shows a message box with an OK button. If |parent| is non-null,
// the box will be modal on it. (On Mac, it is always app-modal.) Generally
// speaking, this function should not be used for much. Infobars are preferred.
void ShowMessageBox(gfx::NativeWindow parent,
                    const string16& title,
                    const string16& message,
                    MessageBoxType type);

// Shows a question message box with two buttons (Yes/No), with the default
// button of Yes. If |parent| is non-null, the box will be modal on it. (On Mac,
// it is always app-modal.) Returns true if the Yes button was chosen.
bool ShowQuestionMessageBox(gfx::NativeWindow parent,
                            const string16& title,
                            const string16& message);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_SIMPLE_MESSAGE_BOX_H_
