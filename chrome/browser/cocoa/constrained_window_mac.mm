// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/constrained_window_mac.h"

#import "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#import "third_party/GTM/AppKit/GTMWindowSheetController.h"

ConstrainedWindowMacDelegate::~ConstrainedWindowMacDelegate() {}

void ConstrainedWindowMacDelegateSystemSheet::RunSheet(
    GTMWindowSheetController* sheetController,
    NSView* view) {
  NSArray* params = [NSArray arrayWithObjects:
      [NSNull null],  // window, must be [NSNull null]
      delegate_.get(),
      [NSValue valueWithPointer:didEndSelector_],
      [NSValue valueWithPointer:NULL],  // context info for didEndSelector_.
      nil];
  [sheetController beginSystemSheet:systemSheet_
                       modalForView:view
                     withParameters:params];
}

void ConstrainedWindowMacDelegateCustomSheet::RunSheet(
    GTMWindowSheetController* sheetController,
    NSView* view) {
  [sheetController beginSheet:customSheet_.get()
                 modalForView:view
                modalDelegate:delegate_.get()
               didEndSelector:didEndSelector_
                  contextInfo:NULL];
}

// static
ConstrainedWindow* ConstrainedWindow::CreateConstrainedDialog(
    TabContents* parent,
    ConstrainedWindowMacDelegate* delegate) {
  return new ConstrainedWindowMac(parent, delegate);
}

ConstrainedWindowMac::ConstrainedWindowMac(
    TabContents* owner, ConstrainedWindowMacDelegate* delegate)
    : owner_(owner),
      delegate_(delegate),
      controller_(nil),
      should_be_visible_(false) {
  DCHECK(owner);
  DCHECK(delegate);
}

ConstrainedWindowMac::~ConstrainedWindowMac() {}

void ConstrainedWindowMac::ShowConstrainedWindow() {
  should_be_visible_ = true;
  // The TabContents only has a native window if it is currently visible. In
  // this case, open the sheet now. Else, Realize() will be called later, when
  // our tab becomes visible.
  NSWindow* browserWindow = owner_->view()->GetTopLevelNativeWindow();
  NSWindowController* controller = [browserWindow windowController];
  if (controller != nil) {
    DCHECK([controller isKindOfClass:[BrowserWindowController class]]);
    Realize(static_cast<BrowserWindowController*>(controller));
  }
}

void ConstrainedWindowMac::CloseConstrainedWindow() {
  // Note: controller_ can be `nil` here if the sheet was never realized. That's
  // ok.
  [controller_ removeConstrainedWindow:this];
  delegate_->DeleteDelegate();
  owner_->WillClose(this);

  delete this;
}

void ConstrainedWindowMac::Realize(BrowserWindowController* controller) {
  if (!should_be_visible_)
    return;

  if (controller_ != nil) {
    DCHECK(controller_ == controller);
    return;
  }
  DCHECK(controller != nil);

  // Remember the controller we're adding ourselves to, so that we can later
  // remove us from it.
  controller_ = controller;
  [controller_ attachConstrainedWindow:this];
  delegate_->set_sheet_open(true);
}
