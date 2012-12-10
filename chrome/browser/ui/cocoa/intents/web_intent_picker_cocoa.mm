// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/intents/web_intent_picker_cocoa.h"

#include <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/message_loop.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_picker_view_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "content/public/browser/native_web_keyboard_event.h"

// static
WebIntentPicker* WebIntentPicker::Create(content::WebContents* web_contents,
                                         WebIntentPickerDelegate* delegate,
                                         WebIntentPickerModel* model) {
  return new WebIntentPickerCocoa(web_contents, delegate, model);
}

WebIntentPickerCocoa::WebIntentPickerCocoa(content::WebContents* web_contents,
                                           WebIntentPickerDelegate* delegate,
                                           WebIntentPickerModel* model)
    : web_contents_(web_contents),
      delegate_(delegate),
      model_(model),
      update_pending_(false),
      weak_ptr_factory_(this) {
  model_->set_observer(this);

  view_controller_.reset(
      [[WebIntentPickerViewController alloc] initWithPicker:this]);

  scoped_nsobject<NSWindow> window([[ConstrainedWindowCustomWindow alloc]
      initWithContentRect:[[view_controller_ view] bounds]]);
  [[window contentView] addSubview:[view_controller_ view]];
  [view_controller_ update];

  constrained_window_.reset(new ConstrainedWindowMac2(
      this, web_contents, window));
}

WebIntentPickerCocoa::~WebIntentPickerCocoa() {
}

void WebIntentPickerCocoa::Close() {
  constrained_window_->CloseConstrainedWindow();
}

void WebIntentPickerCocoa::SetActionString(const string16& action) {
  // Ignored. Action string is retrieved from the model.
}

void WebIntentPickerCocoa::OnExtensionInstallSuccess(const std::string& id) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa::OnExtensionInstallFailure(const std::string& id) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa::OnShowExtensionInstallDialog(
      content::WebContents* parent_web_contents,
      ExtensionInstallPrompt::Delegate* delegate,
      const ExtensionInstallPrompt::Prompt& prompt) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa::OnInlineDispositionAutoResize(
    const gfx::Size& size) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa::OnInlineDispositionHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser ||
      event.type == content::NativeWebKeyboardEvent::Char) {
    return;
  }
  ChromeEventProcessingWindow* window =
      base::mac::ObjCCastStrict<ChromeEventProcessingWindow>(
          constrained_window_->GetNativeWindow());
  [window redispatchKeyEvent:event.os_event];
}

void WebIntentPickerCocoa::OnPendingAsyncCompleted() {
  ScheduleUpdate();
}

void WebIntentPickerCocoa::InvalidateDelegate() {
  delegate_ = NULL;
}

void WebIntentPickerCocoa::OnInlineDispositionWebContentsLoaded(
    content::WebContents* web_contents) {
  ScheduleUpdate();
}

gfx::Size WebIntentPickerCocoa::GetMinInlineDispositionSize() {
  return [view_controller_ minimumInlineWebViewSize];
}

void WebIntentPickerCocoa::OnModelChanged(WebIntentPickerModel* model) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa::OnFaviconChanged(WebIntentPickerModel* model,
                                             size_t index) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa::OnExtensionIconChanged(
    WebIntentPickerModel* model,
    const std::string& extension_id) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa::OnInlineDisposition(const string16& title,
                                                const GURL& url) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa::OnConstrainedWindowClosed(
    ConstrainedWindowMac2* window) {
  // After the OnClosing call the model may be deleted so unset this reference.
  model_->set_observer(NULL);
  model_ = NULL;
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (delegate_)
    delegate_->OnClosing();
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void WebIntentPickerCocoa::ScheduleUpdate() {
  if (update_pending_)
    return;
  update_pending_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&WebIntentPickerCocoa::PerformUpdate,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebIntentPickerCocoa::PerformUpdate() {
  update_pending_ = false;
  [view_controller_ update];
}
