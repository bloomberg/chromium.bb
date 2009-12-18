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

  // TODO(shess): This inset is also reflected in ToolbarController
  // -autocompletePopupPosition.
  NSRect frame = NSInsetRect(cellFrame, 0, 1);
  NSRect midFrame = NSInsetRect(frame, 0.5, 0.5);
  NSRect innerFrame = NSInsetRect(frame, 1, 1);

  // Paint button background image if there is one (otherwise the border won't
  // look right).
  GTMTheme* theme = [controlView gtm_theme];
  NSImage* backgroundImage =
      [theme backgroundImageForStyle:GTMThemeStyleToolBarButton state:YES];
  if (backgroundImage) {
    NSColor* patternColor = [NSColor colorWithPatternImage:backgroundImage];
    [patternColor set];
    // Set the phase to match window.
    NSRect trueRect = [controlView convertRectToBase:cellFrame];
    [[NSGraphicsContext currentContext]
        setPatternPhase:NSMakePoint(NSMinX(trueRect), NSMaxY(trueRect))];
    NSRectFillUsingOperation(midFrame, NSCompositeCopy);
  }

  // Draw the outer stroke (over the background).
  BOOL active = [[controlView window] isMainWindow];
  [[theme strokeColorForStyle:GTMThemeStyleToolBarButton state:active] set];
  NSFrameRectWithWidthUsingOperation(frame, 1, NSCompositeSourceOver);

  // Draw the background for the interior.
  [[self backgroundColor] setFill];
  NSRectFill(innerFrame);

  // Draw the shadow.
  NSRect topShadowFrame, leftShadowFrame, restFrame;
  NSDivideRect(innerFrame, &topShadowFrame, &restFrame, 1, NSMinYEdge);
  NSDivideRect(restFrame, &leftShadowFrame, &restFrame, 1, NSMinXEdge);
  [[NSColor colorWithCalibratedWhite:0.0 alpha:0.05] setFill];
  NSRectFillUsingOperation(topShadowFrame, NSCompositeSourceOver);
  NSRectFillUsingOperation(leftShadowFrame, NSCompositeSourceOver);

  // Draw the focus ring if needed.
  if ([self showsFirstResponder]) {
    [[[NSColor keyboardFocusIndicatorColor] colorWithAlphaComponent:0.5] set];
    NSFrameRectWithWidthUsingOperation(NSInsetRect(frame, 0, 0), 2,
                                       NSCompositeSourceOver);
  }

  [self drawInteriorWithFrame:cellFrame inView:controlView];
}

@end
