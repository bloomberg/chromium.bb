// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_TEXT_FIELD_MAC_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_TEXT_FIELD_MAC_

#import <Cocoa/Cocoa.h>

// Subclass of NSTextField that automatically scrolls containing NSScrollView
// to visually reveal the text field.  When the user tabs between text fields
// embedded in a scrolling view they'd like the "first responder" to be visible.
// This class helps achieve that.
@interface AutoFillTextField : NSTextField {
}
@end

#endif // CHROME_BROWSER_AUTOFILL_AUTOFILL_TEXT_FIELD_MAC_
