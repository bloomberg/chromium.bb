// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SECTION_CONTAINER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SECTION_CONTAINER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"

namespace autofill {
  class AutofillDialogController;
}

@interface AutofillSectionContainer : NSViewController {
 @private
  autofill::DialogSection section_;
  autofill::AutofillDialogController* controller_;  // Not owned.
}

@property(readonly, nonatomic) autofill::DialogSection section;

- (id)initWithController:(autofill::AutofillDialogController*)controller
              forSection:(autofill::DialogSection)section;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SECTION_CONTAINER_H_
