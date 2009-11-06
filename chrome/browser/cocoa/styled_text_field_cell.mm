// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/styled_text_field_cell.h"

#include "app/gfx/font.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

@implementation StyledTextFieldCell

- (CGFloat)baselineAdjust {
  return 0.0;
}

// Returns the same value as textCursorFrameForFrame, but does not call it
// directly to avoid potential infinite loops.
- (NSRect)textFrameForFrame:(NSRect)cellFrame {
  return NSInsetRect(cellFrame, 0, [self baselineAdjust]);
}

// Returns the same value as textFrameForFrame, but does not call it directly to
// avoid potential infinite loops.
- (NSRect)textCursorFrameForFrame:(NSRect)cellFrame {
  return NSInsetRect(cellFrame, 0, [self baselineAdjust]);
}

// Override to show the I-beam cursor only in the area given by
// |textCursorFrameForFrame:|.
- (void)resetCursorRect:(NSRect)cellFrame inView:(NSView *)controlView {
  [super resetCursorRect:[self textCursorFrameForFrame:cellFrame]
                  inView:controlView];
}

// For NSTextFieldCell this is the area within the borders.  For our
// purposes, we count the info decorations as being part of the
// border.
- (NSRect)drawingRectForBounds:(NSRect)theRect {
  return [super drawingRectForBounds:[self textFrameForFrame:theRect]];
}

// TODO(shess): This code is manually drawing the cell's border area,
// but otherwise the cell assumes -setBordered:YES for purposes of
// calculating things like the editing area.  This is probably
// incorrect.  I know that this affects -drawingRectForBounds:.
- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  DCHECK([controlView isFlipped]);
  [[NSColor colorWithCalibratedWhite:1.0 alpha:0.25] set];
  NSFrameRectWithWidthUsingOperation(cellFrame,  1, NSCompositeSourceOver);

  // TODO(shess): This inset is also reflected in ToolbarController
  // -autocompletePopupPosition.
  NSRect frame = NSInsetRect(cellFrame, 0, 1);
  [[self backgroundColor] setFill];
  NSRect innerFrame = NSInsetRect(frame, 1, 1);
  NSRectFill(innerFrame);

  NSRect shadowFrame, restFrame;
  NSDivideRect(innerFrame, &shadowFrame, &restFrame, 1, NSMinYEdge);

  BOOL isMainWindow = [[controlView window] isMainWindow];
  GTMTheme *theme = [controlView gtm_theme];
  NSColor* stroke = [theme strokeColorForStyle:GTMThemeStyleToolBarButton
                                         state:isMainWindow];
  [stroke set];
  NSFrameRectWithWidthUsingOperation(frame, 1.0, NSCompositeSourceOver);

  // Draw the shadow.
  [[NSColor colorWithCalibratedWhite:0.0 alpha:0.05] setFill];
  NSRectFillUsingOperation(shadowFrame, NSCompositeSourceOver);

  if ([self showsFirstResponder]) {
    [[[NSColor keyboardFocusIndicatorColor] colorWithAlphaComponent:0.5] set];
    NSFrameRectWithWidthUsingOperation(NSInsetRect(frame, 0, 0), 2,
                                       NSCompositeSourceOver);
  }

  [self drawInteriorWithFrame:cellFrame inView:controlView];
}

@end
