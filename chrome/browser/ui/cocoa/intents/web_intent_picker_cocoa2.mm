// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/intents/web_intent_picker_cocoa2.h"

#include <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/message_loop.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_picker_view_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "content/public/browser/native_web_keyboard_event.h"

WebIntentPickerCocoa2::WebIntentPickerCocoa2(content::WebContents* web_contents,
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

  constrained_window_.reset(new ConstrainedWindowMac2(
      this, web_contents, window));

  [view_controller_ update];
}

WebIntentPickerCocoa2::~WebIntentPickerCocoa2() {
}

void WebIntentPickerCocoa2::Close() {
  constrained_window_->CloseConstrainedWindow();
}

void WebIntentPickerCocoa2::SetActionString(const string16& action) {
  // Ignored. Action string is retrieved from the model.
}

void WebIntentPickerCocoa2::OnExtensionInstallSuccess(const std::string& id) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa2::OnExtensionInstallFailure(const std::string& id) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa2::OnShowExtensionInstallDialog(
      content::WebContents* parent_web_contents,
      ExtensionInstallPrompt::Delegate* delegate,
      const ExtensionInstallPrompt::Prompt& prompt) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa2::OnInlineDispositionAutoResize(
    const gfx::Size& size) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa2::OnInlineDispositionHandleKeyboardEvent(
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

void WebIntentPickerCocoa2::OnPendingAsyncCompleted() {
  ScheduleUpdate();
}

void WebIntentPickerCocoa2::InvalidateDelegate() {
  delegate_ = NULL;
}

void WebIntentPickerCocoa2::OnInlineDispositionWebContentsLoaded(
    content::WebContents* web_contents) {
  ScheduleUpdate();
}

gfx::Size WebIntentPickerCocoa2::GetMinInlineDispositionSize() {
  return [view_controller_ minimumInlineWebViewSize];
}

void WebIntentPickerCocoa2::OnModelChanged(WebIntentPickerModel* model) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa2::OnFaviconChanged(WebIntentPickerModel* model,
                                             size_t index) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa2::OnExtensionIconChanged(
    WebIntentPickerModel* model,
    const std::string& extension_id) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa2::OnInlineDisposition(const string16& title,
                                                const GURL& url) {
  ScheduleUpdate();
}

void WebIntentPickerCocoa2::OnConstrainedWindowClosed(
    ConstrainedWindowMac2* window) {
  // After the OnClosing call the model may be deleted so unset this reference.
  model_->set_observer(NULL);
  model_ = NULL;
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (delegate_)
    delegate_->OnClosing();
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void WebIntentPickerCocoa2::ScheduleUpdate() {
  if (update_pending_)
    return;
  update_pending_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&WebIntentPickerCocoa2::PerformUpdate,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebIntentPickerCocoa2::PerformUpdate() {
  update_pending_ = false;
  [view_controller_ update];
}
