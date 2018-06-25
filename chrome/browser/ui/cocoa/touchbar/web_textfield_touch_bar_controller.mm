// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"

#include "base/debug/stack_trace.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"
#import "chrome/browser/ui/cocoa/touchbar/credit_card_autofill_touch_bar_controller.h"
#import "chrome/browser/ui/cocoa/touchbar/suggested_text_touch_bar_controller.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/touch_bar_util.h"

@implementation WebTextfieldTouchBarController

- (instancetype)initWithTabContentsController:(TabContentsController*)owner {
  if ((self = [super init])) {
    owner_ = owner;

    if (IsSuggestedTextTouchBarEnabled()) {
      suggestedTextTouchBarController_.reset(
          [[SuggestedTextTouchBarController alloc]
              initWithWebContents:[owner_ webContents]
                       controller:self]);
      [suggestedTextTouchBarController_ initObserver];
    }
  }

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)showCreditCardAutofillWithController:
    (autofill::AutofillPopupController*)controller {
  autofillTouchBarController_.reset(
      [[CreditCardAutofillTouchBarController alloc]
          initWithController:controller]);
  [self invalidateTouchBar];
}

- (void)hideCreditCardAutofillTouchBar {
  autofillTouchBarController_.reset();
  [self invalidateTouchBar];
}

bool IsSuggestedTextTouchBarEnabled() {
  return base::FeatureList::IsEnabled(features::kSuggestedTextTouchBar);
}

- (void)invalidateTouchBar {
  if ([owner_ respondsToSelector:@selector(setTouchBar:)])
    [owner_ performSelector:@selector(setTouchBar:) withObject:nil];
}

- (NSTouchBar*)makeTouchBar {
  if (autofillTouchBarController_)
    return [autofillTouchBarController_ makeTouchBar];

  if (suggestedTextTouchBarController_)
    return [suggestedTextTouchBarController_ makeTouchBar];

  return nil;
}

@end
