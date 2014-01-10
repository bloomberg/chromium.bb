// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DETAILS_CONTAINER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DETAILS_CONTAINER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_section_container.h"


namespace autofill {
class AutofillDialogViewDelegate;
}

@class InfoBubbleView;
@class AutofillBubbleController;

// UI controller for details for current payment instrument.
@interface AutofillDetailsContainer
    : NSViewController<AutofillLayout,
                       AutofillValidationDisplay> {
 @private
  // Scroll view containing all detail sections.
  base::scoped_nsobject<NSScrollView> scrollView_;

  // The individual detail sections.
  base::scoped_nsobject<NSMutableArray> details_;

  // An info bubble to display validation errors.
  base::scoped_nsobject<InfoBubbleView> errorBubble_;

  AutofillBubbleController* errorBubbleController_;

  // The view the current error bubble is anchored to.
  NSView* errorBubbleAnchorView_;

  autofill::AutofillDialogViewDelegate* delegate_;  // Not owned.
}

// Designated initializer.
- (id)initWithDelegate:(autofill::AutofillDialogViewDelegate*)delegate;

// Retrieve the container for the specified |section|.
- (AutofillSectionContainer*)sectionForId:(autofill::DialogSection)section;

// Called when |errorBubble_| needs to be updated.
- (void)updateErrorBubble;

// Called when the delegate-maintained suggestions model has changed.
- (void)modelChanged;

// Validate every visible details section.
- (BOOL)validate;

// Find the first visible and invalid user input field. Returns nil if no field
// is found. Looks at both direct input fields and input fields in suggestions.
- (NSControl*)firstInvalidField;

// Finds the first visible user input field. Returns nil if no field is found.
// Looks at both direct input fields and input fields in suggestions.
- (NSControl*)firstVisibleField;

// Positions the scrollview so that given |field| is visible.
- (void)scrollToView:(NSView*)field;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DETAILS_CONTAINER_H_
