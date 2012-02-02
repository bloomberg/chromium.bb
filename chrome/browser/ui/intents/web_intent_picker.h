// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
#pragma once

#include <stddef.h>
#include <vector>
#include "base/string16.h"

class Browser;
class TabContentsWrapper;
class WebIntentPickerDelegate;
class WebIntentPickerModel;

namespace content {
class WebContents;
}

// Base class for the web intent picker dialog.
class WebIntentPicker {
 public:
  // Platform specific factory function. This function will automatically show
  // the picker.
  static WebIntentPicker* Create(Browser* browser,
                                 TabContentsWrapper* wrapper,
                                 WebIntentPickerDelegate* delegate,
                                 WebIntentPickerModel* model);

  // Hides the UI for this picker, and destroys its UI.
  virtual void Close() = 0;

  // Called when the controller has finished all pending asynchronous
  // activities.
  virtual void OnPendingAsyncCompleted() {}

 protected:
  virtual ~WebIntentPicker() {}
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
