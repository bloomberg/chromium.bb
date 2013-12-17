// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SUGGESTION_CONTAINER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SUGGESTION_CONTAINER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"

namespace autofill {
  class AutofillDialogViewDelegate;
}

@class AutofillTextField;

// Container for the data suggested for a particular input section.
@interface AutofillSuggestionContainer : NSViewController<AutofillLayout> {
 @private
  // The spacer at the top of the suggestion.
  base::scoped_nsobject<NSBox> spacer_;

  // The label that holds the suggestion description text.
  base::scoped_nsobject<NSTextView> label_;

  // The input set by ShowTextfield.
  base::scoped_nsobject<AutofillTextField> inputField_;

  autofill::AutofillDialogViewDelegate* delegate_;  // Not owned.
}

// Auxiliary textfield. See showInputField: for details.
@property (readonly, nonatomic) AutofillTextField* inputField;

// Set the main suggestion text and the corresponding |icon|. The text is set to
// |verticallyCompactText| if that can fit without wrapping. Otherwise, the text
// is set to |horizontallyCompactText|, with possibly additional wrapping
// imposed by the dialog's size constraints.
// NOTE: The implementation assumes that all other elements' sizes are already
// known. Hence, -showInputField:withIcon: should be called prior to calling
// this method, if it is going to be called at all.
- (void)
    setSuggestionWithVerticallyCompactText:(NSString*)verticallyCompactText
                   horizontallyCompactText:(NSString*)horizontallyCompactText
                                      icon:(NSImage*)icon
                                  maxWidth:(CGFloat)maxWidth;

// Shows an auxiliary textfield to the right of the suggestion icon and
// text. This is currently only used to show a CVC field for the CC section.
- (void)showInputField:(NSString*)text withIcon:(NSImage*)icon;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SUGGESTION_CONTAINER_H_
