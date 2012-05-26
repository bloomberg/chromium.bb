// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIMPLE_MESSAGE_BOX_H_
#define CHROME_BROWSER_UI_SIMPLE_MESSAGE_BOX_H_
#pragma once

#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

namespace browser {

enum MessageBoxResult {
  MESSAGE_BOX_RESULT_NO = 0,
  MESSAGE_BOX_RESULT_YES = 1,
};

enum MessageBoxType {
  MESSAGE_BOX_TYPE_INFORMATION,
  MESSAGE_BOX_TYPE_WARNING,
  MESSAGE_BOX_TYPE_QUESTION,
};

// Shows a dialog box with the given |title| and |message|. If |parent| is
// non-NULL, the box will be made modal to the |parent|, except on Mac, where it
// is always app-modal. If |type| is MESSAGE_BOX_TYPE_QUESTION, the box will
// have YES and NO buttons; otherwise it will have an OK button.
//
// NOTE: In general, you should avoid this since it's usually poor UI.
// We have a variety of other surfaces such as wrench menu notifications and
// infobars; consult the UI leads for a recommendation.
MessageBoxResult ShowMessageBox(gfx::NativeWindow parent,
                                const string16& title,
                                const string16& message,
                                MessageBoxType type);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_SIMPLE_MESSAGE_BOX_H_
