// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
#pragma once

#include <stddef.h>
#include <vector>

#include "ui/gfx/native_widget_types.h"

class GURL;
class SkBitmap;
class TabContentsWrapper;
class WebIntentPickerDelegate;

// Base class for the web intent picker dialog.
class WebIntentPicker {
 public:
  class Delegate;

  // Platform specific factory function. This function will automatically show
  // the picker.
  static WebIntentPicker* Create(gfx::NativeWindow parent,
                                 TabContentsWrapper* wrapper,
                                 WebIntentPickerDelegate* delegate);

  // Initalizes this picker with the |urls|.
  virtual void SetServiceURLs(const std::vector<GURL>& urls) = 0;

  // Sets the icon for a service at |index|.
  virtual void SetServiceIcon(size_t index, const SkBitmap& icon) = 0;

  // Sets the icon for a service at |index| to be the default favicon.
  virtual void SetDefaultServiceIcon(size_t index) = 0;

  // Hides the UI for this picker, and destroys its UI.
  virtual void Close() = 0;

 protected:
  virtual ~WebIntentPicker() {}
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
