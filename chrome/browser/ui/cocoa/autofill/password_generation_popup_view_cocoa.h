// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_PASSWORD_GENERATION_POPUP_VIEW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_PASSWORD_GENERATION_POPUP_VIEW_COCOA_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/autofill/password_generation_popup_controller.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_popup_base_view_cocoa.h"

namespace autofill {
class AutofillPopupController;
}  // namespace autofill

@class HyperlinkTextView;

// Draws the native password generation popup view on Mac.
@interface PasswordGenerationPopupViewCocoa
    : AutofillPopupBaseViewCocoa <NSTextViewDelegate> {
 @private
  // The cross-platform controller for this view.
  __weak autofill::PasswordGenerationPopupController* controller_;

  __weak NSTextField* passwordField_;
  __weak NSTextField* passwordSubtextField_;
  __weak HyperlinkTextView* helpTextView_;
}

// Designated initializer.
- (id)initWithController:
    (autofill::PasswordGenerationPopupController*)controller
                   frame:(NSRect)frame;

// Informs the view that its controller has been (or will imminently be)
// destroyed.
- (void)controllerDestroyed;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_PASSWORD_GENERATION_POPUP_VIEW_COCOA_H_
