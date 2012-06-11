// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
#pragma once

#include <stddef.h>
#include <string>
#include <vector>

#include "base/string16.h"
#include "ui/gfx/size.h"

class TabContents;
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
  static WebIntentPicker* Create(TabContents* tab_contents,
                                 WebIntentPickerDelegate* delegate,
                                 WebIntentPickerModel* model);

  // Hides the UI for this picker, and destroys its UI.
  virtual void Close() = 0;

  // Sets the action string of the picker, e.g.,
  // "Which service should be used for sharing?".
  virtual void SetActionString(const string16& action) = 0;

  // Called when an extension is successfully installed via the picker.
  virtual void OnExtensionInstallSuccess(const std::string& id) {}

  // Called when an extension installation started via the picker has failed.
  virtual void OnExtensionInstallFailure(const std::string& id) {}

  // Called when the inline disposition experiences an auto-resize.
  virtual void OnInlineDispositionAutoResize(const gfx::Size& size) {}

  // Called when the controller has finished all pending asynchronous
  // activities.
  virtual void OnPendingAsyncCompleted() {}

  // Called when the inline disposition's web contents have been loaded.
  virtual void OnInlineDispositionWebContentsLoaded(
      content::WebContents* web_contents) {}

  // Get the default size of the inline disposition tab container.
  static gfx::Size GetDefaultInlineDispositionSize(
      content::WebContents* web_contents);

  // Get the minimum size of the inline disposition content container.
  static gfx::Size GetMinInlineDispositionSize();

  // Get the maximum size of the inline disposition content container.
  static gfx::Size GetMaxInlineDispositionSize();

  // Get the star image IDs to use for the nth star (out of 5), given a
  // |rating| in the range [0, 5].
  static int GetNthStarImageIdFromCWSRating(double rating, int index);

 protected:
  virtual ~WebIntentPicker() {}
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_H_
