// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_control_utils.h"

#include "base/mac/scoped_nsobject.h"
#include "skia/ext/skia_utils_mac.h"

namespace constrained_window {

NSTextField* CreateLabel() {
  NSTextField* label =
      [[[NSTextField alloc] initWithFrame:NSZeroRect] autorelease];
  [label setEditable:NO];
  [label setSelectable:NO];
  [label setBezeled:NO];
  [label setDrawsBackground:NO];
  return label;
}

NSAttributedString* GetAttributedLabelString(
    NSString* string,
    ui::ResourceBundle::FontStyle fontStyle,
    NSTextAlignment alignment,
    NSLineBreakMode lineBreakMode) {
  if (!string)
    return nil;

  const gfx::Font& font =
      ui::ResourceBundle::GetSharedInstance().GetFont(fontStyle);
  base::scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setAlignment:alignment];
  [paragraphStyle setLineBreakMode:lineBreakMode];

  NSDictionary* attributes = @{
      NSFontAttributeName:            font.GetNativeFont(),
      NSParagraphStyleAttributeName:  paragraphStyle.get()
  };
  return [[[NSAttributedString alloc] initWithString:string
                                          attributes:attributes] autorelease];
}

}  // namespace constrained_window
