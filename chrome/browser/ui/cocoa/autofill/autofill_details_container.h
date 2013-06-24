// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DETAILS_CONTAINER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DETAILS_CONTAINER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"

namespace autofill {
class AutofillDialogController;
}

@class AutofillSectionContainer;

// UI controller for details for current payment instrument.
@interface AutofillDetailsContainer : NSViewController<AutofillLayout> {
 @private
  base::scoped_nsobject<NSMutableArray> details_;   // The individual detail
                                                    // sections.
  autofill::AutofillDialogController* controller_;  // Not owned.
}

// Designated initializer.
- (id)initWithController:(autofill::AutofillDialogController*)controller;

// Retrieve the container for the specified |section|.
- (AutofillSectionContainer*)sectionForId:(autofill::DialogSection)section;

// Called when the controller-maintained suggestions model has changed.
- (void)modelChanged;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DETAILS_CONTAINER_H_
