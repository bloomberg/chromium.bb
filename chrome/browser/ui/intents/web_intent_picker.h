// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
#pragma once

#include <vector>

class GURL;
class SkBitmap;
class TabContents;
class WebIntentPickerDelegate;

// Base class for the web intent picker dialog.
class WebIntentPicker {
 public:
  class Delegate;

  // Platform specific factory function.
  static WebIntentPicker* Create(TabContents* tab_contents,
                                 WebIntentPickerDelegate* delegate);

  // Initalizes this picker with the |urls|.
  virtual void SetServiceURLs(const std::vector<GURL>& urls) = 0;

  // Sets the icon for a service at |index|.
  virtual void SetServiceIcon(size_t index, const SkBitmap& icon) = 0;

  // Sets the icon for a service at |index| to be the default favicon.
  virtual void SetDefaultServiceIcon(size_t index) = 0;

  // Shows the UI for this picker.
  virtual void Show() = 0;

  // Hides the UI for this picker, and destroys its UI.
  virtual void Close() = 0;

 protected:
  virtual ~WebIntentPicker() {}
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
