// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"

#include "app/gfx/font.h"
#include "app/resource_bundle.h"
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

// The amount of padding on either side reserved for drawing an icon.
const NSInteger kIconHorizontalPad = 3;

// How far to shift bounding box of hint icon label down from top of field.
const NSInteger kIconLabelYOffset = 7;

// How far the editor insets itself, for purposes of determining if
// decorations need to be trimmed.
const CGFloat kEditorHorizontalInset = 3.0;

// Conveniences to centralize width+offset calculations.
CGFloat WidthForHint(NSAttributedString* hintString) {
  return kHintXOffset + ceil([hintString size].width);
}
CGFloat WidthForKeyword(NSAttributedString* keywordString) {
  return kKeywordXOffset + ceil([keywordString size].width) +
      2 * kKeywordTokenInset;
}

}  // namespace

@implementation AutocompleteTextFieldCell

// @synthesize doesn't seem to compile for this transition.
- (NSAttributedString*)keywordString {
  return keywordString_.get();
}
- (NSAttributedString*)hintString {
  return hintString_.get();
}

- (void)setKeywordString:(NSString*)fullString
           partialString:(NSString*)partialString
          availableWidth:(CGFloat)width {
  DCHECK(fullString != nil);

  hintString_.reset();

  // Adjust for space between editor and decorations.
  width -= 2 * kEditorHorizontalInset;

  // If |fullString| won't fit, choose |partialString|.
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:[self font]
                                  forKey:NSFontAttributeName];
  NSString* s = fullString;
  if ([s sizeWithAttributes:attributes].width > width) {
    if (partialString) {
      s = partialString;
    }
  }
  keywordString_.reset(
      [[NSAttributedString alloc] initWithString:s attributes:attributes]);

}

// Convenience for the attributes used in the right-justified info
// cells.
- (NSDictionary*)hintAttributes {
  scoped_nsobject<NSMutableParagraphStyle> style(
      [[NSMutableParagraphStyle alloc] init]);
  [style setAlignment:NSRightTextAlignment];

  return [NSDictionary dictionaryWithObjectsAndKeys:
              [self font], NSFontAttributeName,
              [NSColor lightGrayColor], NSForegroundColorAttributeName,
              style.get(), NSParagraphStyleAttributeName,
              nil];
}

- (void)setKeywordHintPrefix:(NSString*)prefixString
                       image:(NSImage*)anImage
                      suffix:(NSString*)suffixString
              availableWidth:(CGFloat)width {
  DCHECK(prefixString != nil);
  DCHECK(anImage != nil);
  DCHECK(suffixString != nil);

  keywordString_.reset();

  // Adjust for space between editor and decorations.
  width -= 2 * kEditorHorizontalInset;

  // If |hintString_| is now too wide, clear it so that we don't pass
  // the equality tests.
  if (hintString_ && WidthForHint(hintString_) > width) {
    [self clearKeywordAndHint];
  }

  // TODO(shess): Also check the length?
  if (![[hintString_ string] hasPrefix:prefixString] ||
      ![[hintString_ string] hasSuffix:suffixString]) {

    // Build an attributed string with the concatenation of the prefix
    // and suffix.
    NSString* s = [prefixString stringByAppendingString:suffixString];
    scoped_nsobject<NSMutableAttributedString> as(
        [[NSMutableAttributedString alloc]
          initWithString:s attributes:[self hintAttributes]]);

    // Build an attachment containing the hint image.
    scoped_nsobject<NSTextAttachmentCell> attachmentCell(
        [[NSTextAttachmentCell alloc] initImageCell:anImage]);
    scoped_nsobject<NSTextAttachment> attachment(
        [[NSTextAttachment alloc] init]);
    [attachment setAttachmentCell:attachmentCell];

    // The attachment's baseline needs to be adjusted so the image
    // doesn't sit on the same baseline as the text and make
    // everything too tall.
    scoped_nsobject<NSMutableAttributedString> is(
        [[NSAttributedString attributedStringWithAttachment:attachment]
          mutableCopy]);
    [is addAttribute:NSBaselineOffsetAttributeName
        value:[NSNumber numberWithFloat:kKeywordHintImageBaseline]
        range:NSMakeRange(0, [is length])];

    // Stuff the image attachment between the prefix and suffix.
    [as insertAttributedString:is atIndex:[prefixString length]];

    // If too wide, just show the image.
    hintString_.reset(WidthForHint(as) > width ? [is copy] : [as copy]);
  }
}

- (void)setSearchHintString:(NSString*)aString
             availableWidth:(CGFloat)width {
  DCHECK(aString != nil);

  // Adjust for space between editor and decorations.
  width -= 2 * kEditorHorizontalInset;

  keywordString_.reset();

  // If |hintString_| is now too wide, clear it so that we don't pass
  // the equality tests.
  if (hintString_ && WidthForHint(hintString_) > width) {
    [self clearKeywordAndHint];
  }

  if (![[hintString_ string] isEqualToString:aString]) {
    scoped_nsobject<NSAttributedString> as(
        [[NSAttributedString alloc] initWithString:aString
                                        attributes:[self hintAttributes]]);

    // If too wide, don't keep the hint.
    hintString_.reset(WidthForHint(as) > width ? nil : [as copy]);
  }
}

- (void)clearKeywordAndHint {
  keywordString_.reset();
  hintString_.reset();
}

- (void)setSecurityImageView:(LocationBarViewMac::SecurityImageView*)view {
  security_image_view_ = view;
}

- (void)onSecurityIconMousePressed {
  security_image_view_->OnMousePressed();
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

- (NSRect)textCursorFrameForFrame:(NSRect)cellFrame {
  return NSInsetRect(cellFrame, 0, kBaselineAdjust);
}

- (NSRect)textFrameForFrame:(NSRect)cellFrame {
  NSRect textFrame([self textCursorFrameForFrame:cellFrame]);

  if (hintString_) {
    DCHECK(!keywordString_);
    const CGFloat hintWidth(WidthForHint(hintString_));

    // TODO(shess): This could be better.  Show the hint until the
    // non-hint text bumps against it?
    if (hintWidth < NSWidth(cellFrame)) {
      textFrame.size.width -= hintWidth;
    }
  } else if (keywordString_) {
    DCHECK(!hintString_);
    const CGFloat keywordWidth(WidthForKeyword(keywordString_));

    // TODO(shess): This could be better.  There's support for a
    // "short" version of the keyword string, work that in in a
    // follow-on pass.
    if (keywordWidth < NSWidth(cellFrame)) {
      textFrame.origin.x += keywordWidth;
      textFrame.size.width = NSMaxX(cellFrame) - NSMinX(textFrame);
    }
  } else if (security_image_view_ && security_image_view_->IsVisible()) {
    NSImage* image = security_image_view_->GetImage();
    CGFloat width = [image size].width;
    width += kIconHorizontalPad * 2;
    NSAttributedString* label = security_image_view_->GetLabel();
    if (label) {
      width += ceil([label size].width) + kHintXOffset;
    }
    if (width < NSWidth(cellFrame)) {
      textFrame.size.width -= width;
    }
  }

  return textFrame;
}

- (NSRect)imageFrameForFrame:(NSRect)cellFrame
      withImageView:(LocationBarViewMac::LocationBarImageView*)image_view {
  if (!image_view->IsVisible()) {
    return NSZeroRect;
  }
  const NSSize imageRect = [image_view->GetImage() size];
  CGFloat labelWidth = 0;
  NSAttributedString* label = image_view->GetLabel();
  if (label) {
    labelWidth = ceil([label size].width) + kHintXOffset;
  }

  // Move the rect that we're drawing into to the far right, minus
  // enough space for the label (if present).
  cellFrame.origin.x += cellFrame.size.width - imageRect.width;
  cellFrame.origin.x -= labelWidth;
  // Add back the padding
  cellFrame.origin.x -= kIconHorizontalPad;

  // Center the image vertically in the frame
  cellFrame.origin.y +=
      floor((cellFrame.size.height - imageRect.height) / 2);

  // Set the drawing size to the image size
  cellFrame.size = imageRect;

  return cellFrame;
}

- (NSRect)securityImageFrameForFrame:(NSRect)cellFrame {
  if (!security_image_view_) {
    return NSZeroRect;
  }
  return [self imageFrameForFrame:cellFrame withImageView:security_image_view_];
}

// For NSTextFieldCell this is the area within the borders.  For our
// purposes, we count the info decorations as being part of the
// border.
- (NSRect)drawingRectForBounds:(NSRect)theRect {
  return [super drawingRectForBounds:[self textFrameForFrame:theRect]];
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

  // Draw the token rectangle with rounded corners.
  NSRect frame(NSInsetRect(infoFrame, 0.5, 0.5));
  NSBezierPath* path =
      [NSBezierPath bezierPathWithRoundedRect:frame xRadius:4.0 yRadius:4.0];

  // Matches the color of the highlighted line in the popup.
  [[NSColor selectedControlColor] set];
  [path fill];

  // Border around token rectangle, match focus ring's inner color.
  [[[NSColor keyboardFocusIndicatorColor] colorWithAlphaComponent:0.5] set];
  [path setLineWidth:1.0];
  [path stroke];

  // Draw text w/in the rectangle.
  infoFrame.origin.x += 4.0;
  infoFrame.origin.y += 1.0;
  [keywordString_.get() drawInRect:infoFrame];
}

- (void)drawImageView:(LocationBarViewMac::LocationBarImageView*)image_view
            withFrame:(NSRect)cellFrame
               inView:(NSView*)controlView {
  // If there's a label, draw it to the right of the icon.
  CGFloat labelWidth = 0;
  NSAttributedString* label = image_view->GetLabel();
  if (label) {
    labelWidth = ceil([label size].width) + kHintXOffset;
    NSRect textFrame(NSMakeRect(NSMaxX(cellFrame) - labelWidth,
                                cellFrame.origin.y + kIconLabelYOffset,
                                labelWidth,
                                cellFrame.size.height - kIconLabelYOffset));
    [label drawInRect:textFrame];
  }

  // Draw the entire image.
  NSRect imageRect = NSZeroRect;
  NSImage* image = image_view->GetImage();
  image.size = [image size];
  NSRect imageFrame([self imageFrameForFrame:cellFrame
                               withImageView:image_view]);
  [image setFlipped:[controlView isFlipped]];
  [image drawInRect:imageFrame
           fromRect:imageRect
          operation:NSCompositeSourceOver
           fraction:1.0];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  if (hintString_) {
    [self drawHintWithFrame:cellFrame inView:controlView];
  } else if (keywordString_) {
    [self drawKeywordWithFrame:cellFrame inView:controlView];
  } else if (security_image_view_ && security_image_view_->IsVisible()) {
    [self drawImageView:security_image_view_
              withFrame:cellFrame
                 inView:controlView];
  }

  [super drawInteriorWithFrame:cellFrame inView:controlView];
}

- (void)resetCursorRect:(NSRect)cellFrame inView:(NSView *)controlView {
  [super resetCursorRect:[self textCursorFrameForFrame:cellFrame]
                  inView:controlView];
}

@end
