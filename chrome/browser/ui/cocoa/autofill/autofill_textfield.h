// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_TEXTFIELD_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_TEXTFIELD_H_

#import <Cocoa/Cocoa.h>

#include <base/memory/scoped_nsobject.h>

// Text field used for text inputs inside Autofill.
// Provide both dog ear and red outline when the contents are marked invalid.
@interface AutofillTextField : NSTextField
@end

@interface AutofillTextFieldCell : NSTextFieldCell {
 @private
  BOOL invalid_;
  scoped_nsobject<NSImage> icon_;
}

@property(assign, nonatomic) BOOL invalid;
@property(nonatomic, retain, getter=icon, setter=setIcon:) NSImage* icon;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_TEXTFIELD_H_
