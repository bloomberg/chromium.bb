// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SUGGESTION_CONTAINER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SUGGESTION_CONTAINER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"

namespace autofill {
  class AutofillDialogController;
}

@class AutofillTextField;

// Container for the data suggested for a particular input section.
@interface AutofillSuggestionContainer : NSViewController<AutofillLayout> {
 @private
  // The label that holds the suggestion description text.
  base::scoped_nsobject<NSTextField> label_;

  // The second (and longer) line of text that describes the suggestion.
  base::scoped_nsobject<NSTextField> label2_;

  // The icon that comes just before |label_|.
  base::scoped_nsobject<NSImageView> iconImageView_;

  // The input set by ShowTextfield.
  base::scoped_nsobject<AutofillTextField> inputField_;

  autofill::AutofillDialogController* controller_;  // Not owned.
}

// Set the icon for the suggestion.
- (void)setIcon:(NSImage*)iconImage;

// Set the main suggestion text and the font used to render that text.
- (void)setSuggestionText:(NSString*)line1
                    line2:(NSString*)line2
                 withFont:(NSFont*)font;

// Turns editable textfield on, setting the field's placeholder text and icon.
- (void)showTextfield:(NSString*)text withIcon:(NSImage*)icon;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SUGGESTION_CONTAINER_H_
