// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_TEXTFIELD_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_TEXTFIELD_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_input_field.h"

// Text field used for text inputs inside Autofill.
// Provide both dog ear and red outline when the contents are marked invalid.
@interface AutofillTextField : NSTextField<AutofillInputField,
                                           NSTextFieldDelegate> {
 @private
  id<AutofillInputDelegate> delegate_;
  base::scoped_nsobject<NSString> validityMessage_;

  // |shouldFilterFirstClick_| ensures only the very first click after
  // -becomeFirstResponder: is treated specially.
  BOOL shouldFilterClick_;

  // YES if the field is currently handling a click that caused the field to
  // become first responder.
  BOOL handlingFirstClick_;
}

// Can be invoked by field editor to forward mouseDown messages from the field
// editor to the AutofillTextField.
- (void)onEditorMouseDown:(id)sender;

@end

@interface AutofillTextFieldCell : NSTextFieldCell<AutofillInputCell> {
 @private
  BOOL invalid_;
  NSString* defaultValue_;
  base::scoped_nsobject<NSImage> icon_;
}

@property(nonatomic, retain) NSImage* icon;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_TEXTFIELD_H_
