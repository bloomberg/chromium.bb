// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/styled_text_field_cell.h"

// Subclass of StyledTextFieldCell that allows you to slice off sections on the
// left and right of the cell.
@interface StyledTextFieldTestCell : StyledTextFieldCell {
  CGFloat leftMargin_;
  CGFloat rightMargin_;
}
@property(assign) CGFloat leftMargin;
@property(assign) CGFloat rightMargin;
@end
