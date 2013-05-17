// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_MAIN_CONTAINER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_MAIN_CONTAINER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"

@class AutofillAccountChooser;
@class AutofillDetailsContainer;
@class AutofillDialogWindowController;
@class AutofillSectionContainer;
@class GTMWidthBasedTweaker;

namespace autofill {
  class AutofillDialogController;
}

// NSViewController for the main portion of the autofill dialog. Contains
// account chooser, details for current payment instruments, OK/Cancel.
// Might dynamically add and remove other elements.
@interface AutofillMainContainer : NSViewController {
 @private
  scoped_nsobject<AutofillAccountChooser> accountChooser_;
  scoped_nsobject<GTMWidthBasedTweaker> buttonContainer_;
  scoped_nsobject<AutofillDetailsContainer> detailsContainer_;
  AutofillDialogWindowController* target_;
  autofill::AutofillDialogController* controller_;  // Not owned.
}

@property(assign, nonatomic) AutofillDialogWindowController* target;

// Designated initializer.
- (id)initWithController:(autofill::AutofillDialogController*)controller;

// Returns the account chooser.
- (AutofillAccountChooser*)accountChooser;

// Returns the view controller responsible for |section|.
- (AutofillSectionContainer*)sectionForId:(autofill::DialogSection)section;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_MAIN_CONTAINER_H_
