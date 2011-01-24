// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include "base/scoped_nsobject.h"

// A HyperlinkButtonCell is used to create an NSButton that looks and acts
// like a hyperlink. The default styling is to look like blue, underlined text
// and to have the pointingHand cursor on mouse over.
//
// To use in Interface Builder:
//  1. Drag out an NSButton.
//  2. Double click on the button so you have the cell component selected.
//  3. In the Identity panel of the inspector, set the custom class to this.
//  4. In the Attributes panel, change the Bezel to Square.
//  5. In the Size panel, set the Height to 16.
@interface HyperlinkButtonCell : NSButtonCell {
  scoped_nsobject<NSColor> textColor_;
}
@property(nonatomic, retain) NSColor* textColor;

+ (NSColor*)defaultTextColor;

@end
