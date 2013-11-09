// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_POP_UP_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_POP_UP_BUTTON_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_input_field.h"

@interface AutofillPopUpButton : NSPopUpButton<AutofillInputField> {
 @private
  id<AutofillInputDelegate> inputDelegate_;
  base::scoped_nsobject<NSString> validityMessage_;
}

@end

@interface AutofillPopUpCell : NSPopUpButtonCell<AutofillInputCell> {
 @private
  BOOL invalid_;
  NSString* defaultValue_;
}

@end

#endif // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_POP_UP_BUTTON_H_
