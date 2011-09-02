// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"

#include "base/memory/scoped_nsobject.h"

// The baseline shift for text in the NSTextView.
const float kTextBaselineShift = -1.0;

@interface HyperlinkTextView(Private)
// Initialize the NSTextView properties for this subclass.
- (void)configureTextView;

// Change the current IBeamCursor to an arrowCursor.
- (void)fixupCursor;
@end

@implementation HyperlinkTextView

- (id)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder]))
    [self configureTextView];
  return self;
}

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect]))
    [self configureTextView];
  return self;
}

// Never draw the insertion point (otherwise, it shows up without any user
// action if full keyboard accessibility is enabled).
- (BOOL)shouldDrawInsertionPoint {
  return NO;
}

- (NSRange)selectionRangeForProposedRange:(NSRange)proposedSelRange
                              granularity:(NSSelectionGranularity)granularity {
  // Do not allow selections.
  return NSMakeRange(0, 0);
}

// Convince NSTextView to not show an I-Beam cursor when the cursor is over the
// text view but not over actual text.
//
// http://www.mail-archive.com/cocoa-dev@lists.apple.com/msg10791.html
// "NSTextView sets the cursor over itself dynamically, based on considerations
// including the text under the cursor. It does so in -mouseEntered:,
// -mouseMoved:, and -cursorUpdate:, so those would be points to consider
// overriding."
- (void)mouseMoved:(NSEvent*)e {
  [super mouseMoved:e];
  [self fixupCursor];
}

- (void)mouseEntered:(NSEvent*)e {
  [super mouseEntered:e];
  [self fixupCursor];
}

- (void)cursorUpdate:(NSEvent*)e {
  [super cursorUpdate:e];
  [self fixupCursor];
}

- (void)configureTextView {
  [self setEditable:NO];
  [self setDrawsBackground:NO];
  [self setHorizontallyResizable:NO];
  [self setVerticallyResizable:NO];
}

- (void)fixupCursor {
  if ([[NSCursor currentCursor] isEqual:[NSCursor IBeamCursor]])
    [[NSCursor arrowCursor] set];
}

- (void)setMessageAndLink:(NSString*)message
                 withLink:(NSString*)link
                 atOffset:(NSUInteger)linkOffset
                     font:(NSFont*)font
             messageColor:(NSColor*)messageColor
                linkColor:(NSColor*)linkColor {
  // Create an attributes dictionary for the message and link.
  NSMutableDictionary* attributes = [NSMutableDictionary dictionary];
  [attributes setObject:messageColor
                 forKey:NSForegroundColorAttributeName];
  [attributes setObject:[NSCursor arrowCursor]
                 forKey:NSCursorAttributeName];
  [attributes setObject:font
                 forKey:NSFontAttributeName];
  [attributes setObject:[NSNumber numberWithFloat:kTextBaselineShift]
                 forKey:NSBaselineOffsetAttributeName];

  // Create the attributed string for the message.
  scoped_nsobject<NSMutableAttributedString> attributedMessage(
      [[NSMutableAttributedString alloc] initWithString:message
                                             attributes:attributes]);

  if ([link length] != 0) {
    // Add additional attributes to style the link text appropriately as
    // well as linkify it.
    [attributes setObject:linkColor
                   forKey:NSForegroundColorAttributeName];
    [attributes setObject:[NSNumber numberWithBool:YES]
                   forKey:NSUnderlineStyleAttributeName];
    [attributes setObject:[NSCursor pointingHandCursor]
                   forKey:NSCursorAttributeName];
    [attributes setObject:[NSNumber numberWithInt:NSSingleUnderlineStyle]
                   forKey:NSUnderlineStyleAttributeName];
    [attributes setObject:[NSString string]  // dummy value
                   forKey:NSLinkAttributeName];

    // Insert the link into the message at the appropriate offset.
    scoped_nsobject<NSAttributedString> attributedLink(
        [[NSAttributedString alloc] initWithString:link
                                        attributes:attributes]);
    [attributedMessage.get() insertAttributedString:attributedLink.get()
                                            atIndex:linkOffset];
    // Ensure the TextView doesn't override the link style.
    [self setLinkTextAttributes:attributes];
  }

  // Update the text view with the new text.
  [[self textStorage] setAttributedString:attributedMessage];
}

@end
