// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PICKER_COCOA2_H_
#define CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PICKER_COCOA2_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"

@class WebIntentPickerViewController;

// An implementation of WebIntentPicker for Cocoa. Bridges between the C++ and
// the Cocoa WebIntentPickerViewController object.  Note, this will eventually
// replace WebIntentPickerCocoa, see http://crbug.com/152010.
class WebIntentPickerCocoa2 : public WebIntentPicker,
                              public WebIntentPickerModelObserver {
 public:
  WebIntentPickerCocoa2(content::WebContents* web_contents,
                        WebIntentPickerDelegate* delegate,
                        WebIntentPickerModel* model);
  virtual ~WebIntentPickerCocoa2();

  WebIntentPickerDelegate* delegate() const { return delegate_; }
  WebIntentPickerModel* model() const { return model_; }
  content::WebContents* web_contents() const { return web_contents_; }
  NSWindowController* window_controller() const { return window_controller_; }
  WebIntentPickerViewController* view_controller() const {
    return view_controller_;
  }

  // WebIntentPicker implementation.
  virtual void Close() OVERRIDE;
  virtual void SetActionString(const string16& action) OVERRIDE;
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(const std::string& id) OVERRIDE;
  virtual void OnInlineDispositionAutoResize(const gfx::Size& size) OVERRIDE;
  virtual void OnInlineDispositionHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void OnPendingAsyncCompleted() OVERRIDE;
  virtual void OnInlineDispositionWebContentsLoaded(
      content::WebContents* web_contents) OVERRIDE;
  virtual gfx::Size GetMinInlineDispositionSize() OVERRIDE;

  // WebIntentPickerModelObserver implementation.
  virtual void OnModelChanged(WebIntentPickerModel* model) OVERRIDE;
  virtual void OnFaviconChanged(WebIntentPickerModel* model,
                                size_t index) OVERRIDE;
  virtual void OnExtensionIconChanged(WebIntentPickerModel* model,
                                      const std::string& extension_id) OVERRIDE;
  virtual void OnInlineDisposition(const string16& title,
                                   const GURL& url) OVERRIDE;

 private:
  content::WebContents* const web_contents_;
  WebIntentPickerDelegate* const delegate_;
  WebIntentPickerModel* const model_;
  scoped_nsobject<WebIntentPickerViewController> view_controller_;
  scoped_nsobject<NSWindowController> window_controller_;
};

#endif  // CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PICKER_COCOA2_H_
