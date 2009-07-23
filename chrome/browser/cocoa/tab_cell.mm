// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_cell.h"
#import "third_party/GTM/AppKit/GTMTheme.h"
#import "third_party/GTM/AppKit/GTMNSColor+Luminance.h"

@implementation TabCell

- (id)initTextCell:(NSString *)aString {
  self = [super initTextCell:aString];
  if (self != nil) {
    // nothing for now...
  }
  return self;
}

- (NSBackgroundStyle)interiorBackgroundStyle {
  return [[[self controlView] gtm_theme]
          interiorBackgroundStyleForStyle:GTMThemeStyleTabBarSelected
          state:GTMThemeStateActiveWindow];
}

// Override drawing the button so that it looks like a Chromium tab instead
// of just a normal MacOS button.
- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
  // Inset where the text is drawn to keep it away from the sloping edges of the
  // tab, the close box, and the icon view. These constants are derived
  // empirically as the cell doesn't know about the surrounding view objects.
  // TODO(pinkerton/alcor): Fix this somehow?
  const int kIconXOffset = 28;
  const int kCloseBoxXOffset = 21;
  NSRect frame = NSOffsetRect(cellFrame, kIconXOffset, 0);
  frame.size.width -= kCloseBoxXOffset + kIconXOffset;
  [self drawInteriorWithFrame:frame
                       inView:controlView];
}

- (void)highlight:(BOOL)flag
        withFrame:(NSRect)cellFrame
           inView:(NSView *)controlView {
  // Don't do normal highlighting
}

@end
