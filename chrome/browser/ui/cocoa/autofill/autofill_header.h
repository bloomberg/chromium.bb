// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_HEADER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_HEADER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"

namespace autofill {
class AutofillDialogViewDelegate;
}  // autofill

@class AutofillAccountChooser;

@interface AutofillHeader : NSViewController<AutofillLayout> {
 @private
  base::scoped_nsobject<AutofillAccountChooser> accountChooser_;
  base::scoped_nsobject<NSTextField> title_;

  autofill::AutofillDialogViewDelegate* delegate_;  // not owned, owns dialog.
}

// Designated initializer.
- (id)initWithDelegate:(autofill::AutofillDialogViewDelegate*)delegate;

// Returns the anchor view for notifications, which is the middle of the account
// chooser view.
- (NSView*)anchorView;

// Updates the header's state from the dialog controller.
- (void)update;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_HEADER_H_
