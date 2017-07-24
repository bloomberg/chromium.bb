// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/web_textfield_touch_bar_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_popup_view_cocoa.h"
#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"
#import "ui/base/cocoa/touch_bar_util.h"

@implementation WebTextfieldTouchBarController

- (instancetype)initWithTabContentsController:(TabContentsController*)owner {
  if ((self = [self init])) {
    owner_ = owner;
  }

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)showCreditCardAutofillForPopupView:(AutofillPopupViewCocoa*)popupView {
  DCHECK(popupView);
  DCHECK([popupView window]);

  window_ = [popupView window];

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(popupWindowWillClose:)
                 name:NSWindowWillCloseNotification
               object:window_];
  popupView_ = popupView;

  if ([owner_ respondsToSelector:@selector(setTouchBar:)])
    [owner_ performSelector:@selector(setTouchBar:) withObject:nil];
}

- (void)popupWindowWillClose:(NSNotification*)notif {
  popupView_ = nil;

  if ([owner_ respondsToSelector:@selector(setTouchBar:)])
    [owner_ performSelector:@selector(setTouchBar:) withObject:nil];

  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowWillCloseNotification
              object:window_];
}

- (NSTouchBar*)makeTouchBar {
  if (popupView_)
    return [popupView_ makeTouchBar];

  return nil;
}

@end
