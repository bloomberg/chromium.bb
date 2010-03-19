// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"

#include "app/gfx/font.h"
#include "app/resource_bundle.h"
#include "base/logging.h"

namespace {

const CGFloat kBaselineAdjust = 2.0;

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

// The amount of padding on either side reserved for drawing an icon.
const NSInteger kIconHorizontalPad = 3;

// How far to shift bounding box of hint icon label down from top of field.
const NSInteger kIconLabelYOffset = 5;

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

@implementation AutocompleteTextFieldIcon

@synthesize rect = rect_;
@synthesize view = view_;

+ (AutocompleteTextFieldIcon*)
    iconWithRect:(NSRect)rect
            view:(LocationBarViewMac::LocationBarImageView*)view {
  AutocompleteTextFieldIcon* result = [[AutocompleteTextFieldIcon alloc] init];
  [result setRect:rect];
  [result setView:view];
  return [result autorelease];
}

@end

@implementation AutocompleteTextFieldCell

// @synthesize doesn't seem to compile for this transition.
- (NSAttributedString*)keywordString {
  return keywordString_.get();
}
- (NSAttributedString*)hintString {
  return hintString_.get();
}

- (CGFloat)baselineAdjust {
  return kBaselineAdjust;
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

- (void)setPageActionViewList:(LocationBarViewMac::PageActionViewList*)list {
  page_action_views_ = list;
}

- (void)setSecurityImageView:(LocationBarViewMac::SecurityImageView*)view {
  security_image_view_ = view;
}

- (void)setContentSettingViewsList:
    (LocationBarViewMac::ContentSettingViews*)views {
  content_setting_views_ = views;
}

// Overriden to account for the hint strings and hint icons.
- (NSRect)textFrameForFrame:(NSRect)cellFrame {
  NSRect textFrame([super textFrameForFrame:cellFrame]);

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
  } else {
    // Leave room for images on the right (lock icon etc).
    NSArray* iconFrames = [self layedOutIcons:cellFrame];
    CGFloat width = 0;
    if ([iconFrames count] > 0)
      width = NSMaxX(cellFrame) - NSMinX([[iconFrames lastObject] rect]);
    if (width > 0)
      width += kIconHorizontalPad;
    if (width < NSWidth(cellFrame))
      textFrame.size.width -= width;
  }

  return textFrame;
}

// Returns a rect of size |imageSize| centered vertically and right-justified in
// the |box|, with its top left corner |margin| pixels from the right end of the
// box. (The image thus occupies part of the |margin|.)
- (NSRect)rightJustifyImage:(NSSize)imageSize
                     inRect:(NSRect)box
                 withMargin:(CGFloat)margin {
  box.origin.x += box.size.width - margin;
  box.origin.y += floor((box.size.height - imageSize.height) / 2);
  box.size = imageSize;
  return box;
}

- (NSRect)securityImageFrameForFrame:(NSRect)cellFrame {
  if (!security_image_view_ || !security_image_view_->IsVisible()) {
    return NSZeroRect;
  }

  // Calculate the total width occupied by the image, label, and padding.
  NSSize imageSize = [security_image_view_->GetImage() size];
  CGFloat widthUsed = imageSize.width + kIconHorizontalPad;
  NSAttributedString* label = security_image_view_->GetLabel();
  if (label) {
    widthUsed += ceil([label size].width) + kHintXOffset;
  }

  return [self rightJustifyImage:imageSize
                          inRect:cellFrame
                      withMargin:widthUsed];
}

- (size_t)pageActionCount {
  // page_action_views_ may be NULL during testing, or if the
  // containing LocationViewMac object has already been destructed
  // (happens sometimes during window shutdown).
  if (!page_action_views_)
    return 0;
  return page_action_views_->Count();
}

- (NSRect)pageActionFrameForIndex:(size_t)index inFrame:(NSRect)cellFrame {
  LocationBarViewMac::PageActionImageView* view =
      page_action_views_->ViewAt(index);

  // When this method is called, all the icon images are still loading, so
  // just check to see whether the view is visible when deciding whether
  // its NSRect should be made available.
  if (!view->preview_enabled() && !view->IsVisible())
    return NSZeroRect;

  for (AutocompleteTextFieldIcon* icon in [self layedOutIcons:cellFrame]) {
    if (view == [icon view])
      return [icon rect];
  }
  NOTREACHED();
  return NSZeroRect;
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

- (void)drawImageView:(LocationBarViewMac::LocationBarImageView*)imageView
              inFrame:(NSRect)imageFrame
               inView:(NSView*)controlView {
  // If there's a label, draw it to the right of the icon. The caller must have
  // left sufficient space.
  NSAttributedString* label = imageView->GetLabel();
  if (label) {
    CGFloat labelWidth = ceil([label size].width) + kHintXOffset;
    NSRect textFrame(NSMakeRect(NSMaxX(imageFrame) + kIconHorizontalPad,
                                imageFrame.origin.y + kIconLabelYOffset,
                                labelWidth,
                                imageFrame.size.height - kIconLabelYOffset));
    [label drawInRect:textFrame];
  }

  // Draw the entire image.
  NSRect imageRect = NSZeroRect;
  NSImage* image = imageView->GetImage();
  image.size = [image size];
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
  } else {
    for (AutocompleteTextFieldIcon* icon in [self layedOutIcons:cellFrame]) {
      [self drawImageView:[icon view]
                  inFrame:[icon rect]
                   inView:controlView];
    }
  }

  [super drawInteriorWithFrame:cellFrame inView:controlView];
}

- (NSArray*)layedOutIcons:(NSRect)cellFrame {
  NSMutableArray* result = [NSMutableArray arrayWithCapacity:0];
  NSRect iconFrame = cellFrame;
  if (security_image_view_ && security_image_view_->IsVisible()) {
    NSRect securityImageFrame = [self securityImageFrameForFrame:iconFrame];
    [result addObject:
        [AutocompleteTextFieldIcon iconWithRect:securityImageFrame
                                           view:security_image_view_]];
    iconFrame.size.width -= NSMaxX(iconFrame) - NSMinX(securityImageFrame);
  }

  const size_t pageActionCount = [self pageActionCount];
  for (size_t i = 0; i < pageActionCount; ++i) {
    LocationBarViewMac::PageActionImageView* view =
        page_action_views_->ViewAt(i);
    if (view->preview_enabled() || view->IsVisible()) {
      // If this function is called right after a page action icon has been
      // created, the images for all views will still be loading; in this case,
      // each visible view will give us its default size.
      NSSize iconSize = view->GetPreferredImageSize();
      NSRect pageActionFrame =
          [self rightJustifyImage:iconSize
                           inRect:iconFrame
                       withMargin:kIconHorizontalPad + iconSize.width];
      [result addObject:
          [AutocompleteTextFieldIcon iconWithRect:pageActionFrame view:view]];
      iconFrame.size.width -= NSMaxX(iconFrame) - NSMinX(pageActionFrame);
    }
  }

  if (content_setting_views_) {
    // We use a reverse_iterator here because we're laying out the views from
    // right to left but in the vector they're ordered left to right.
    for (LocationBarViewMac::ContentSettingViews::const_reverse_iterator
            it(content_setting_views_->rbegin());
        it != const_cast<const LocationBarViewMac::ContentSettingViews*>(
            content_setting_views_)->rend();
        ++it) {
      if ((*it)->IsVisible()) {
        NSImage* image = (*it)->GetImage();
        NSRect blockedContentFrame =
            [self rightJustifyImage:[image size]
                             inRect:iconFrame
                         withMargin:[image size].width + kIconHorizontalPad];
        [result addObject:
            [AutocompleteTextFieldIcon iconWithRect:blockedContentFrame
                                               view:*it]];
        iconFrame.size.width -= NSMaxX(iconFrame) - NSMinX(blockedContentFrame);
      }
    }
  }
  return result;
}

@end
