// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/hyperlink_button_cell.h"

@interface HyperlinkButtonCell (Private)
- (NSDictionary*)linkAttributres;
- (void)customizeButtonCell;
@end

@implementation HyperlinkButtonCell
@dynamic textColor;

+ (NSColor*)defaultTextColor {
  return [NSColor blueColor];
}

// Designated initializer.
- (id)init {
  if ((self = [super init])) {
    [self customizeButtonCell];
  }
  return self;
}

// Initializer called when the cell is loaded from the NIB.
- (id)initWithCoder:(NSCoder*)aDecoder {
  if ((self = [super initWithCoder:aDecoder])) {
    [self customizeButtonCell];
  }
  return self;
}

// Initializer for code-based creation.
- (id)initTextCell:(NSString*)title {
  if ((self = [super initTextCell:title])) {
    [self customizeButtonCell];
  }
  return self;
}

// Because an NSButtonCell has multiple initializers, this method performs the
// common cell customization code.
- (void)customizeButtonCell {
  [self setBordered:NO];
  [self setTextColor:[HyperlinkButtonCell defaultTextColor]];

  CGFloat fontSize = [NSFont systemFontSizeForControlSize:[self controlSize]];
  NSFont* font = [NSFont controlContentFontOfSize:fontSize];
  [self setFont:font];

  // Do not change button appearance when we are pushed.
  // TODO(rsesek): Change text color to red?
  [self setHighlightsBy:NSNoCellMask];

  // We need to set this so that we can override |-mouseEntered:| and
  // |-mouseExited:| to change the cursor style on hover states.
  [self setShowsBorderOnlyWhileMouseInside:YES];
}

// Creates the NSDictionary of attributes for the attributed string.
- (NSDictionary*)linkAttributes {
  NSUInteger underlineMask = NSUnderlinePatternSolid | NSUnderlineStyleSingle;
  NSMutableParagraphStyle* paragraphStyle =
    [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  [paragraphStyle setAlignment:[self alignment]];

  return [NSDictionary dictionaryWithObjectsAndKeys:
      [self textColor], NSForegroundColorAttributeName,
      [NSNumber numberWithInt:underlineMask], NSUnderlineStyleAttributeName,
      [self font], NSFontAttributeName,
      [NSCursor pointingHandCursor], NSCursorAttributeName,
      paragraphStyle, NSParagraphStyleAttributeName,
      nil
  ];
}

// Override the drawing point for the cell so that the custom style attributes
// can always be applied.
- (NSRect)drawTitle:(NSAttributedString*)title
          withFrame:(NSRect)frame
             inView:(NSView*)controlView {
  NSAttributedString* attrString =
      [[NSAttributedString alloc] initWithString:[title string]
                                      attributes:[self linkAttributes]];
  [attrString autorelease];
  return [super drawTitle:attrString withFrame:frame inView:controlView];
}

// Override the default behavior to draw the border. Instead, change the cursor.
- (void)mouseEntered:(NSEvent*)event {
  [[NSCursor pointingHandCursor] push];
}

- (void)mouseExited:(NSEvent*)event {
  [NSCursor pop];
}

// Setters and getters.
- (NSColor*)textColor {
  return textColor_.get();
}

- (void)setTextColor:(NSColor*)color {
  textColor_.reset([color retain]);
}

@end
