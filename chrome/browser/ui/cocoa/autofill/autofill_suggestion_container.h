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
  base::scoped_nsobject<NSTextField> label_;

  // The second (and longer) line of text that describes the suggestion.
  base::scoped_nsobject<NSTextField> label2_;

  // The icon that comes just before |label_|.
  base::scoped_nsobject<NSImageView> iconImageView_;

  // The input set by ShowTextfield.
  base::scoped_nsobject<AutofillTextField> inputField_;

  autofill::AutofillDialogViewDelegate* delegate_;  // Not owned.
}

// Auxiliary textfield. See showTextfield: for details.
@property (readonly, nonatomic) AutofillTextField* inputField;

// Set the icon for the suggestion.
- (void)setIcon:(NSImage*)iconImage;

// Set the main suggestion text and the font used to render that text.
- (void)setSuggestionText:(NSString*)line1
                    line2:(NSString*)line2;

// Shows an auxiliary textfield to the right of the suggestion icon and
// text. This is currently only used to show a CVC field for the CC section.
- (void)showInputField:(NSString*)text withIcon:(NSImage*)icon;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SUGGESTION_CONTAINER_H_
