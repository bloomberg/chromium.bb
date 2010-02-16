// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_text_field_mac.h"

@implementation AutoFillTextField

// Override NSResponder method for when the text field may gain focus.  We
// call |scrollRectToVisible| to ensure that this text field is visible within
// the scrolling area.
- (BOOL)becomeFirstResponder {
  // Vertical inset is negative to indicate "outset".  Provides some visual
  // space above and below when tabbing between fields.
  const CGFloat kVerticalInset = -40.0;
  BOOL becoming = [super becomeFirstResponder];
  if (becoming) {
    [self scrollRectToVisible:NSInsetRect([self bounds], 0.0, kVerticalInset)];
  }
  return becoming;
}

@end
