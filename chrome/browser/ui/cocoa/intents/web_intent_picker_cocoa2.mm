// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/intents/web_intent_picker_cocoa2.h"

#include <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"
#include "base/message_loop.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_picker_view_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "content/public/browser/native_web_keyboard_event.h"

WebIntentPickerCocoa2::WebIntentPickerCocoa2(content::WebContents* web_contents,
                                             WebIntentPickerDelegate* delegate,
                                             WebIntentPickerModel* model)
    : web_contents_(web_contents),
      delegate_(delegate),
      model_(model) {
  model_->set_observer(this);

  view_controller_.reset(
      [[WebIntentPickerViewController alloc] initWithPicker:this]);
  // TODO(sail) Support bubble window as well.
  window_controller_.reset([[ConstrainedWindowController alloc]
      initWithParentWebContents:web_contents
                   embeddedView:[view_controller_ view]]);
  [view_controller_ update];
}

WebIntentPickerCocoa2::~WebIntentPickerCocoa2() {
}

void WebIntentPickerCocoa2::Close() {
  // After the OnClosing call the model may be deleted so unset this reference.
  model_->set_observer(NULL);
  model_ = NULL;

  [window_controller_ close];
  delegate_->OnClosing();
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void WebIntentPickerCocoa2::SetActionString(const string16& action) {
  // Ignored. Action string it retrieved from the model.
}

void WebIntentPickerCocoa2::OnExtensionInstallSuccess(const std::string& id) {
  [view_controller_ update];
}

void WebIntentPickerCocoa2::OnExtensionInstallFailure(const std::string& id) {
  [view_controller_ update];
}

void WebIntentPickerCocoa2::OnShowExtensionInstallDialog(
      gfx::NativeWindow parent,
      content::PageNavigator* navigator,
      ExtensionInstallPrompt::Delegate* delegate,
      const ExtensionInstallPrompt::Prompt& prompt) {
  [view_controller_ update];
}

void WebIntentPickerCocoa2::OnInlineDispositionAutoResize(
    const gfx::Size& size) {
  [view_controller_ update];
}

void WebIntentPickerCocoa2::OnInlineDispositionHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser ||
      event.type == content::NativeWebKeyboardEvent::Char) {
    return;
  }
  ChromeEventProcessingWindow* window =
      base::mac::ObjCCastStrict<ChromeEventProcessingWindow>(
          [window_controller_ window]);
  [window redispatchKeyEvent:event.os_event];
}

void WebIntentPickerCocoa2::OnPendingAsyncCompleted() {
  [view_controller_ update];
}

void WebIntentPickerCocoa2::OnInlineDispositionWebContentsLoaded(
    content::WebContents* web_contents) {
  [view_controller_ update];
}

gfx::Size WebIntentPickerCocoa2::GetMinInlineDispositionSize() {
  return [view_controller_ minimumInlineWebViewSize];
}

void WebIntentPickerCocoa2::OnModelChanged(WebIntentPickerModel* model) {
  [view_controller_ update];
}

void WebIntentPickerCocoa2::OnFaviconChanged(WebIntentPickerModel* model,
                                             size_t index) {
  [view_controller_ update];
}

void WebIntentPickerCocoa2::OnExtensionIconChanged(
    WebIntentPickerModel* model,
    const std::string& extension_id) {
  [view_controller_ update];
}

void WebIntentPickerCocoa2::OnInlineDisposition(const string16& title,
                                                const GURL& url) {
  [view_controller_ update];
}
