// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

const NSInteger kBaselineOffset = 2;

@implementation AutocompleteTextFieldCell

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  [[NSColor colorWithCalibratedWhite:1.0 alpha:0.25] set];
  NSFrameRectWithWidthUsingOperation(cellFrame,  1, NSCompositeSourceOver);

  NSRect frame = NSInsetRect(cellFrame, 0, 1);
  [[NSColor whiteColor] setFill];
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

  [self drawInteriorWithFrame:cellFrame
                        inView:controlView];

}

- (void)drawInteriorWithFrame:(NSRect)cellFrame
                       inView:(NSView*)controlView {
  [super drawInteriorWithFrame:NSInsetRect(cellFrame, 0, kBaselineOffset)
                        inView:controlView];
}

// Override these methods so that the field editor shows up in the right place
- (void)editWithFrame:(NSRect)cellFrame
               inView:(NSView*)controlView
               editor:(NSText*)textObj
             delegate:(id)anObject
                event:(NSEvent*)theEvent {
  [super editWithFrame:NSInsetRect(cellFrame, 0, kBaselineOffset)
                inView:controlView
                editor:textObj
              delegate:anObject
                 event:theEvent];
}


// Override these methods so that the field editor shows up in the right place
- (void)selectWithFrame:(NSRect)cellFrame
                 inView:(NSView*)controlView
                 editor:(NSText*)textObj
               delegate:(id)anObject
                  start:(NSInteger)selStart
                 length:(NSInteger)selLength {
  [super selectWithFrame:NSInsetRect(cellFrame, 0, kBaselineOffset)
                  inView:controlView editor:textObj
                delegate:anObject
                   start:selStart
                  length:selLength];
}

@end
