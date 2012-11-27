// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PICKER_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PICKER_COCOA_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac2.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"

@class WebIntentPickerViewController;

// An implementation of WebIntentPicker for Cocoa. Bridges between the C++ and
// the Cocoa WebIntentPickerViewController object.
class WebIntentPickerCocoa : public WebIntentPicker,
                             public WebIntentPickerModelObserver,
                             public ConstrainedWindowMacDelegate2 {
 public:
  WebIntentPickerCocoa(content::WebContents* web_contents,
                       WebIntentPickerDelegate* delegate,
                       WebIntentPickerModel* model);
  virtual ~WebIntentPickerCocoa();

  WebIntentPickerDelegate* delegate() const {
    DCHECK(delegate_);
    return delegate_;
  }

  WebIntentPickerModel* model() const { return model_; }
  content::WebContents* web_contents() const { return web_contents_; }
  ConstrainedWindowMac2* constrained_window() const {
    return constrained_window_.get();
  }
  WebIntentPickerViewController* view_controller() const {
    return view_controller_;
  }

  // WebIntentPicker implementation.
  virtual void Close() OVERRIDE;
  virtual void SetActionString(const string16& action) OVERRIDE;
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(const std::string& id) OVERRIDE;
  virtual void OnShowExtensionInstallDialog(
      content::WebContents* parent_web_contents,
      ExtensionInstallPrompt::Delegate* delegate,
      const ExtensionInstallPrompt::Prompt& prompt) OVERRIDE;
  virtual void OnInlineDispositionAutoResize(const gfx::Size& size) OVERRIDE;
  virtual void OnInlineDispositionHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void OnPendingAsyncCompleted() OVERRIDE;
  virtual void InvalidateDelegate() OVERRIDE;
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

  // ConstrainedWindowMacDelegate2 implementation.
  virtual void OnConstrainedWindowClosed(
      ConstrainedWindowMac2* window) OVERRIDE;

 private:
  void ScheduleUpdate();
  void PerformUpdate();

  content::WebContents* const web_contents_;
  WebIntentPickerDelegate* delegate_;
  WebIntentPickerModel* model_;
  scoped_nsobject<WebIntentPickerViewController> view_controller_;
  scoped_ptr<ConstrainedWindowMac2> constrained_window_;
  bool update_pending_;
  base::WeakPtrFactory<WebIntentPickerCocoa> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_PICKER_COCOA_H_
