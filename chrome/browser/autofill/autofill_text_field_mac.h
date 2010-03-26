// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_TEXT_FIELD_MAC_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_TEXT_FIELD_MAC_

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"

#define AUTOFILL_CC_TAG 22

// Subclass of NSTextField with special abilities:
// - automatically scrolls containing NSScrollView to visually reveal itself
//   on focus
// - properly obfuscates credit card numbers

@interface AutoFillTextField : NSTextField {
  BOOL isCreditCardField_;
  BOOL isObfuscated_;
  BOOL isBeingSelected_;

  scoped_nsobject<NSString> obfuscatedValue_;
}
@end

#endif // CHROME_BROWSER_AUTOFILL_AUTOFILL_TEXT_FIELD_MAC_
