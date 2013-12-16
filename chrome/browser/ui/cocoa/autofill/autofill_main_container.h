// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_MAIN_CONTAINER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_MAIN_CONTAINER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"

@class AutofillDetailsContainer;
@class AutofillDialogWindowController;
@class AutofillNotificationContainer;
@class AutofillSectionContainer;
@class AutofillTooltipController;
@class GTMWidthBasedTweaker;
@class HyperlinkTextView;

namespace autofill {
  class AutofillDialogViewDelegate;
}

// NSViewController for the main portion of the autofill dialog. Contains
// account chooser, details for current payment instruments, OK/Cancel.
// Might dynamically add and remove other elements.
@interface AutofillMainContainer : NSViewController<AutofillLayout,
                                                    NSTextViewDelegate> {
 @private
  base::scoped_nsobject<GTMWidthBasedTweaker> buttonContainer_;
  base::scoped_nsobject<NSImageView> buttonStripImage_;
  base::scoped_nsobject<NSButton> saveInChromeCheckbox_;
  base::scoped_nsobject<AutofillTooltipController> saveInChromeTooltip_;
  base::scoped_nsobject<AutofillDetailsContainer> detailsContainer_;
  base::scoped_nsobject<HyperlinkTextView> legalDocumentsView_;
  base::scoped_nsobject<AutofillNotificationContainer> notificationContainer_;
  AutofillDialogWindowController* target_;

  // Weak. Owns the dialog.
  autofill::AutofillDialogViewDelegate* delegate_;

  // Preferred size for legal documents.
  NSSize legalDocumentsSize_;

  // Dirty marker for preferred size.
  BOOL legalDocumentsSizeDirty_;
}

@property(assign, nonatomic) AutofillDialogWindowController* target;

// Designated initializer.
- (id)initWithDelegate:(autofill::AutofillDialogViewDelegate*)delegate;

// Returns the preferred size for the footer and notifications at the specfied
// |width|.
- (NSSize)decorationSizeForWidth:(CGFloat)width;

// Sets the anchor point for the notificationView_.
- (void)setAnchorView:(NSView*)anchorView;

// Returns the view delegate responsible for |section|.
- (AutofillSectionContainer*)sectionForId:(autofill::DialogSection)section;

// Called when the delegate-maintained suggestions model has changed.
- (void)modelChanged;

// Get status of "Save in Chrome" checkbox.
- (BOOL)saveDetailsLocally;

// Called when the legal documents text might need to be refreshed.
- (void)updateLegalDocuments;

// Called when there are changes to the notification area.
- (void)updateNotificationArea;

// Called when the error bubble needs to be updated.
- (void)updateErrorBubble;

// Validates form input data.
- (BOOL)validate;

// Updates status of "save in Chrome" checkbox.
- (void)updateSaveInChrome;

// Makes the first invalid input first responder.
- (void)makeFirstInvalidInputFirstResponder;

// Called when the main container becomes visible. Ensures the right input field
// becomes first responder, and positions the scrollview correctly. This MUST be
// called after layout on the main container is complete, since it depends on
// the size of the contained views to be correct.
- (void)scrollInitialEditorIntoViewAndMakeFirstResponder;

@end


// AutofillMainContainer helper functions, for testing purposes only.
@interface AutofillMainContainer (Testing)

@property(readonly, nonatomic) NSButton* saveInChromeCheckboxForTesting;
@property(readonly, nonatomic) NSImageView* buttonStripImageForTesting;
@property(readonly, nonatomic) NSImageView* saveInChromeTooltipForTesting;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_MAIN_CONTAINER_H_
