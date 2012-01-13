// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_WEB_INTENT_PICKER_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_WEB_INTENT_PICKER_COCOA_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"

class InlineHtmlContentDelegate;
class TabContentsWrapper;
@class WebIntentBubbleController;

// A bridge class that enables communication between ObjectiveC and C++.
class WebIntentPickerCocoa : public WebIntentPicker {
 public:
  // |browser| and |delegate| cannot be NULL.
  // |wrapper| is unused.
  WebIntentPickerCocoa(Browser* browser,
                       TabContentsWrapper* wrapper,
                       WebIntentPickerDelegate* delegate);
  virtual ~WebIntentPickerCocoa();

  // WebIntentPickerDelegate forwarding API.
  void OnCancelled();
  void OnServiceChosen(size_t index);

  void set_controller(WebIntentBubbleController* controller);

  // WebIntentPicker:
  virtual void SetServiceURLs(const std::vector<GURL>& urls) OVERRIDE;
  virtual void SetServiceIcon(size_t index, const SkBitmap& icon) OVERRIDE;
  virtual void SetDefaultServiceIcon(size_t index) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual content::WebContents* SetInlineDisposition(const GURL& url) OVERRIDE;

 private:
  // Weak pointer to the |delegate_| to notify about user choice/cancellation.
  WebIntentPickerDelegate* delegate_;

  Browser* browser_;  // The browser we're in. Weak Reference.

  WebIntentBubbleController* controller_;  // Weak reference.

  // Tab contents wrapper to hold intent page if inline disposition is used.
  scoped_ptr<TabContentsWrapper> inline_disposition_tab_contents_;

  // Delegate for inline disposition tab contents.
  scoped_ptr<InlineHtmlContentDelegate> inline_disposition_delegate_;

  // Default constructor, for testing only.
  WebIntentPickerCocoa();

  // For testing access.
  friend class WebIntentBubbleControllerTest;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_WEB_INTENT_PICKER_COCOA_H_
