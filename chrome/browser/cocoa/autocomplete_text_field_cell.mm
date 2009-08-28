// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"

#import "base/logging.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

namespace {

const NSInteger kBaselineAdjust = 2;

// How far to offset the keyword token into the field.
const NSInteger kKeywordXOffset = 3;

// How much width (beyond text) to add to the keyword token on each
// side.
const NSInteger kKeywordTokenInset = 3;

// Gap to leave between hint and right-hand-side of cell.
const NSInteger kHintXOffset = 4;

// How far to shift bounding box of hint down from top of field.
// Assumes -setFlipped:YES.
const NSInteger kHintYOffset = 4;

// How far to inset the keywork token from sides.
const NSInteger kKeywordYInset = 4;

// TODO(shess): The keyword hint image wants to sit on the baseline.
// This moves it down so that there is approximately as much image
// above the lowercase ascender as below the baseline.  A better
// technique would be nice to have, though.
const NSInteger kKeywordHintImageBaseline = -6;

// Offset from the bottom of the field for drawing decoration text.
// TODO(shess): Somehow determine the baseline for the text field and
// use that.
const NSInteger kBaselineOffset = 4;

}  // namespace

@implementation AutocompleteTextFieldCell

@synthesize fieldEditorNeedsReset = fieldEditorNeedsReset_;

// @synthesize doesn't seem to compile for this transition.
- (NSAttributedString*)keywordString {
  return keywordString_.get();
}
- (NSAttributedString*)hintString {
  return hintString_.get();
}

- (void)setKeywordString:(NSString*)aString {
  DCHECK(aString != nil);
  if (hintString_ || ![[keywordString_ string] isEqualToString:aString]) {
    NSDictionary* attributes =
        [NSDictionary dictionaryWithObjectsAndKeys:
             [self font], NSFontAttributeName,
              nil];

    keywordString_.reset(
        [[NSAttributedString alloc] initWithString:aString
                                        attributes:attributes]);
    hintString_.reset();

    fieldEditorNeedsReset_ = YES;
  }
}

- (void)setHintString:(NSAttributedString*)aString {
  keywordString_.reset();
  hintString_.reset([aString copy]);

  fieldEditorNeedsReset_ = YES;
}

// Convenience for the attributes used in the right-justified info
// cells.
- (NSDictionary*)hintAttributes {
  NSMutableParagraphStyle* style =
      [[[NSMutableParagraphStyle alloc] init] autorelease];
  [style setAlignment:NSRightTextAlignment];

  return [NSDictionary dictionaryWithObjectsAndKeys:
              [self font], NSFontAttributeName,
              [NSColor lightGrayColor], NSForegroundColorAttributeName,
              style, NSParagraphStyleAttributeName,
              nil];
}

- (void)setKeywordHintPrefix:(NSString*)prefixString
                       image:(NSImage*)anImage
                      suffix:(NSString*)suffixString {
  DCHECK(prefixString != nil);
  DCHECK(anImage != nil);
  DCHECK(suffixString != nil);

  // TODO(shess): Also check the length?
  if (keywordString_ ||
      ![[hintString_ string] hasPrefix:prefixString] ||
      ![[hintString_ string] hasSuffix:suffixString]) {

    // Build an attributed string with the concatenation of the prefix
    // and suffix.
    NSString* s = [prefixString stringByAppendingString:suffixString];
    NSMutableAttributedString* as =
        [[[NSMutableAttributedString alloc]
           initWithString:s attributes:[self hintAttributes]] autorelease];

    // Build an attachment containing the hint image.
    NSTextAttachmentCell* attachmentCell =
        [[[NSTextAttachmentCell alloc] initImageCell:anImage] autorelease];
    NSTextAttachment* attachment =
        [[[NSTextAttachment alloc] init] autorelease];
    [attachment setAttachmentCell:attachmentCell];

    // The attachment's baseline needs to be adjusted so the image
    // doesn't sit on the same baseline as the text and make
    // everything too tall.
    NSMutableAttributedString* is =
        [[[NSAttributedString attributedStringWithAttachment:attachment]
           mutableCopy] autorelease];
    [is addAttribute:NSBaselineOffsetAttributeName
        value:[NSNumber numberWithFloat:kKeywordHintImageBaseline]
        range:NSMakeRange(0, [is length])];

    // Stuff the image attachment between the prefix and suffix.
    [as insertAttributedString:is atIndex:[prefixString length]];

    [self setHintString:as];
  }
}

- (void)setSearchHintString:(NSString*)aString {
  DCHECK(aString != nil);

  if (keywordString_ || ![[hintString_ string] isEqualToString:aString]) {
    NSAttributedString* as =
        [[[NSAttributedString alloc] initWithString:aString
                                         attributes:[self hintAttributes]]
          autorelease];

    [self setHintString:as];
  }
}

- (void)clearKeywordAndHint {
  if (keywordString_ || hintString_) {
    keywordString_.reset();
    hintString_.reset();

    fieldEditorNeedsReset_ = YES;
  }
}

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

- (NSRect)textFrameForFrame:(NSRect)cellFrame {
  NSRect textFrame(cellFrame);

  if (hintString_) {
    DCHECK(!keywordString_);
    const CGFloat hintWidth = kHintXOffset + ceil([hintString_ size].width);

    // TODO(shess): This could be better.  Show the hint until the
    // non-hint text bumps against it?
    if (hintWidth < NSWidth(cellFrame)) {
      textFrame.size.width -= hintWidth;
    }
  } else if (keywordString_) {
    DCHECK(!hintString_);
    const CGFloat keywordWidth = kKeywordXOffset +
        ceil([keywordString_ size].width) + 2 * kKeywordTokenInset;

    // TODO(shess): This could be better.  There's support for a
    // "short" version of the keyword string, work that in in a
    // follow-on pass.
    if (keywordWidth < NSWidth(cellFrame)) {
      textFrame.origin.x += keywordWidth;
      textFrame.size.width = NSMaxX(cellFrame) - NSMinX(textFrame);
    }
  }

  return NSInsetRect(textFrame, 0, kBaselineAdjust);
}

- (void)drawHintWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  DCHECK(hintString_);

  NSRect textFrame = [self textFrameForFrame:cellFrame];
  NSRect infoFrame(NSMakeRect(NSMaxX(textFrame),
                              cellFrame.origin.y + kHintYOffset,
                              ceil([hintString_ size].width),
                              cellFrame.size.height - kHintYOffset));
  [hintString_.get() drawInRect:infoFrame];
}

- (void)drawKeywordWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  DCHECK(keywordString_);

  NSRect textFrame = [self textFrameForFrame:cellFrame];
  const CGFloat x = NSMinX(cellFrame) + kKeywordXOffset;
  NSRect infoFrame(NSMakeRect(x,
                              cellFrame.origin.y + kKeywordYInset,
                              NSMinX(textFrame) - x,
                              cellFrame.size.height - 2 * kKeywordYInset));

  // Draw a token rectangle with rounded corners.
  NSRect frame(NSInsetRect(infoFrame, 0.5, 0.5));
  NSBezierPath* path =
      [NSBezierPath bezierPathWithRoundedRect:frame xRadius:4.0 yRadius:4.0];

  [[NSColor controlColor] set];
  [path fill];

  GTMTheme *theme = [controlView gtm_theme];
  NSColor* stroke = [theme strokeColorForStyle:GTMThemeStyleToolBarButton
                                         state:YES];
  [stroke setStroke];
  [path setLineWidth:1.0];
  [path stroke];

  // Draw text w/in the rectangle.
  infoFrame.origin.x += 4.0;
  infoFrame.origin.y += 1.0;
  [keywordString_.get() drawInRect:infoFrame];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  if (hintString_) {
    [self drawHintWithFrame:cellFrame inView:controlView];
  } else if (keywordString_) {
    [self drawKeywordWithFrame:cellFrame inView:controlView];
  }

  [super drawInteriorWithFrame:[self textFrameForFrame:cellFrame]
                        inView:controlView];
}

// Override these methods so that the field editor shows up in the right place
- (void)editWithFrame:(NSRect)cellFrame
               inView:(NSView*)controlView
               editor:(NSText*)textObj
             delegate:(id)anObject
                event:(NSEvent*)theEvent {
  [super editWithFrame:[self textFrameForFrame:cellFrame]
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
  [super selectWithFrame:[self textFrameForFrame:cellFrame]
                  inView:controlView editor:textObj
                delegate:anObject
                   start:selStart
                  length:selLength];
}

@end
