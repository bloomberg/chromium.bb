// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_POP_UP_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_POP_UP_BUTTON_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/autofill/autofill_input_field.h"

@interface AutofillPopUpButton : NSPopUpButton<AutofillInputField>
@end

@interface AutofillPopUpCell : NSPopUpButtonCell<AutofillInputField> {
 @private
  BOOL invalid_;
}

@end

#endif // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_POP_UP_BUTTON_H_
