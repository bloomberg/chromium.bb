// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/web_intent_picker_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/web_intent_bubble_controller.h"
#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/gfx/image/image.h"

using content::WebContents;

// static
WebIntentPicker* WebIntentPicker::Create(Browser* browser,
                                         TabContentsWrapper* wrapper,
                                         WebIntentPickerDelegate* delegate,
                                         WebIntentPickerModel* model) {
  return new WebIntentPickerCocoa(browser, wrapper, delegate, model);
}

WebIntentPickerCocoa::WebIntentPickerCocoa()
    : delegate_(NULL),
      model_(NULL),
      browser_(NULL),
      controller_(nil),
      weak_ptr_factory_(this),
      service_invoked(false) {
}


WebIntentPickerCocoa::WebIntentPickerCocoa(Browser* browser,
                                           TabContentsWrapper* wrapper,
                                           WebIntentPickerDelegate* delegate,
                                           WebIntentPickerModel* model)
   : delegate_(delegate),
     model_(model),
     browser_(browser),
     controller_(nil),
     weak_ptr_factory_(this),
     service_invoked(false) {
  model_->set_observer(this);

  DCHECK(browser);
  DCHECK(delegate);
  NSWindow* parentWindow = browser->window()->GetNativeHandle();

  // Compute the anchor point, relative to location bar.
  BrowserWindowController* controller = [parentWindow windowController];
  LocationBarViewMac* locationBar = [controller locationBarBridge];
  NSPoint anchor = locationBar->GetPageInfoBubblePoint();
  anchor = [browser->window()->GetNativeHandle() convertBaseToScreen:anchor];

  // The controller is deallocated when the window is closed, so no need to
  // worry about it here.
  [[WebIntentBubbleController alloc] initWithPicker:this
                                       parentWindow:parentWindow
                                         anchoredAt:anchor];
}

WebIntentPickerCocoa::~WebIntentPickerCocoa() {
  if (model_ != NULL)
    model_->set_observer(NULL);
}

void WebIntentPickerCocoa::Close() {
  DCHECK(controller_);
  [controller_ close];
  if (inline_disposition_tab_contents_.get())
    inline_disposition_tab_contents_->web_contents()->OnCloseStarted();
}

void WebIntentPickerCocoa::PerformDelayedLayout() {
  // Check to see if a layout has already been scheduled.
  if (weak_ptr_factory_.HasWeakPtrs())
    return;

  // Delay performing layout by a second so that all the animations from
  // InfoBubbleWindow and origin updates from BaseBubbleController finish, so
  // that we don't all race trying to change the frame's origin.
  //
  // Using MessageLoop is superior here to |-performSelector:| because it will
  // not retain its target; if the child outlives its parent, zombies get left
  // behind (http://crbug.com/59619). This will cancel the scheduled task if
  // the controller get destroyed before the message
  // can be delivered.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&WebIntentPickerCocoa::PerformLayout,
                 weak_ptr_factory_.GetWeakPtr()),
      100 /* milliseconds */);
}

void WebIntentPickerCocoa::PerformLayout() {
  DCHECK(controller_);
  // If the window is animating closed when this is called, the
  // animation could be holding the last reference to |controller_|
  // (and thus |this|).  Pin it until the task is completed.
  scoped_nsobject<WebIntentBubbleController> keep_alive([controller_ retain]);
  [controller_ performLayoutWithModel:model_];
}

void WebIntentPickerCocoa::OnModelChanged(WebIntentPickerModel* model) {
  PerformDelayedLayout();
}

void WebIntentPickerCocoa::OnFaviconChanged(WebIntentPickerModel* model,
                                            size_t index) {
  // We don't handle individual icon changes - just redo the whole model.
  PerformDelayedLayout();
}

void WebIntentPickerCocoa::OnInlineDisposition(WebIntentPickerModel* model) {
  const WebIntentPickerModel::Item& item = model->GetItemAt(
      model->inline_disposition_index());

  content::WebContents* web_contents = content::WebContents::Create(
      browser_->profile(), NULL, MSG_ROUTING_NONE, NULL, NULL);
  inline_disposition_tab_contents_.reset(new TabContentsWrapper(web_contents));
  inline_disposition_delegate_.reset(new WebIntentInlineDispositionDelegate);
  web_contents->SetDelegate(inline_disposition_delegate_.get());

  // Must call this immediately after WebContents creation to avoid race
  // with load.
  delegate_->OnInlineDispositionWebContentsCreated(web_contents);

  inline_disposition_tab_contents_->web_contents()->GetController().LoadURL(
      item.url,
      content::Referrer(),
      content::PAGE_TRANSITION_START_PAGE,
      std::string());

  [controller_ setInlineDispositionTabContents:
      inline_disposition_tab_contents_.get()];
  PerformDelayedLayout();
}

void WebIntentPickerCocoa::OnCancelled() {
  DCHECK(delegate_);
  if (!service_invoked)
    delegate_->OnCancelled();
  delegate_->OnClosing();
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void WebIntentPickerCocoa::OnServiceChosen(size_t index) {
  DCHECK(delegate_);
  const WebIntentPickerModel::Item& item = model_->GetItemAt(index);
  service_invoked = true;
  delegate_->OnServiceChosen(index, item.disposition);
}

void WebIntentPickerCocoa::set_controller(
    WebIntentBubbleController* controller) {
  controller_ = controller;
}
