// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/confirm_bubble_controller.h"

#include <utility>

#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/confirm_bubble_cocoa.h"
#import "chrome/browser/ui/confirm_bubble_model.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image.h"

@implementation ConfirmBubbleController

- (id)initWithParent:(NSView*)parent
              origin:(CGPoint)origin
               model:(std::unique_ptr<ConfirmBubbleModel>)model {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    parent_ = parent;
    origin_ = origin;
    model_ = std::move(model);
  }
  return self;
}

- (void)loadView {
  [self setView:[[[ConfirmBubbleCocoa alloc] initWithParent:parent_
                                                 controller:self] autorelease]];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self autorelease];
}

// Accessors. This functions converts the C++ types retrieved from the
// ConfirmBubbleModel object to Objective-C types, and return them.
- (NSPoint)origin {
  return NSPointFromCGPoint(origin_);
}

- (NSString*)title {
  return base::SysUTF16ToNSString(model_->GetTitle());
}

- (NSString*)messageText {
  return base::SysUTF16ToNSString(model_->GetMessageText());
}

- (NSString*)linkText {
  return base::SysUTF16ToNSString(model_->GetLinkText());
}

- (NSString*)helpPageURL {
  return base::SysUTF8ToNSString(model_->GetHelpPageURL().spec());
}

- (NSString*)okButtonText {
  return base::SysUTF16ToNSString(
      model_->GetButtonLabel(ConfirmBubbleModel::BUTTON_OK));
}

- (NSString*)cancelButtonText {
  return base::SysUTF16ToNSString(
      model_->GetButtonLabel(ConfirmBubbleModel::BUTTON_CANCEL));
}

- (BOOL)hasOkButton {
  return (model_->GetButtons() & ConfirmBubbleModel::BUTTON_OK) ? YES : NO;
}

- (BOOL)hasCancelButton {
  return (model_->GetButtons() & ConfirmBubbleModel::BUTTON_CANCEL) ? YES : NO;
}

// Action handlers.
- (void)accept {
  model_->Accept();
}

- (void)cancel {
  model_->Cancel();
}

- (void)openHelpPage {
  model_->OpenHelpPage();
}

@end
