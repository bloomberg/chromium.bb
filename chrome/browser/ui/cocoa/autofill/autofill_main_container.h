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
@class AutofillSectionContainer;
@class GTMWidthBasedTweaker;

namespace autofill {
  class AutofillDialogController;
}

// NSViewController for the main portion of the autofill dialog. Contains
// account chooser, details for current payment instruments, OK/Cancel.
// Might dynamically add and remove other elements.
@interface AutofillMainContainer : NSViewController<AutofillLayout> {
 @private
  base::scoped_nsobject<GTMWidthBasedTweaker> buttonContainer_;
  base::scoped_nsobject<AutofillDetailsContainer> detailsContainer_;
  AutofillDialogWindowController* target_;
  autofill::AutofillDialogController* controller_;  // Not owned.
}

@property(assign, nonatomic) AutofillDialogWindowController* target;

// Designated initializer.
- (id)initWithController:(autofill::AutofillDialogController*)controller;

// Returns the view controller responsible for |section|.
- (AutofillSectionContainer*)sectionForId:(autofill::DialogSection)section;

// Called when the controller-maintained suggestions model has changed.
- (void)modelChanged;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_MAIN_CONTAINER_H_
