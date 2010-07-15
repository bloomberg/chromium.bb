// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar/autocomplete_text_field_cell.h"

#include "base/logging.h"
#import "chrome/browser/cocoa/image_utils.h"
#import "chrome/browser/cocoa/location_bar/location_bar_decoration.h"

@interface AutocompleteTextAttachmentCell : NSTextAttachmentCell {
}

// TODO(shess):
// Override -cellBaselineOffset to allow the image to be shifted up or
// down relative to the containing text's baseline.

// Draw the image using |DrawImageInRect()| helper function for
// flipped consistency with other image drawing.
- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)aView;

@end

namespace {

const CGFloat kBaselineAdjust = 2.0;

// Matches the clipping radius of |GradientButtonCell|.
const CGFloat kCornerRadius = 4.0;

// How far to shift bounding box of hint down from top of field.
// Assumes -setFlipped:YES.
const NSInteger kHintYOffset = 4;

// TODO(shess): The keyword hint image wants to sit on the baseline.
// This moves it down so that there is approximately as much image
// above the lowercase ascender as below the baseline.  A better
// technique would be nice to have, though.
const NSInteger kKeywordHintImageBaseline = -6;

// How far to shift bounding box of hint icon label down from top of field.
const NSInteger kIconLabelYOffset = 7;

// How far the editor insets itself, for purposes of determining if
// decorations need to be trimmed.
const CGFloat kEditorHorizontalInset = 3.0;

// How far to inset the left-hand decorations from the field's bounds.
const CGFloat kLeftDecorationXOffset = 3.0;

// How far to inset the right-hand decorations from the field's bounds.
// TODO(shess): Why is this different from |kLeftDecorationXOffset|?
const CGFloat kRightDecorationXOffset = 4.0;

// The amount of padding on either side reserved for drawing decorations.
const CGFloat kDecorationHorizontalPad = 3;

// How long to wait for mouse-up on the location icon before assuming
// that the user wants to drag.
const NSTimeInterval kLocationIconDragTimeout = 0.25;

// Conveniences to centralize width+offset calculations.
CGFloat WidthForHint(NSAttributedString* hintString) {
  return kRightDecorationXOffset + ceil([hintString size].width);
}

// Convenience to draw |image| in the |rect| portion of |view|.
void DrawImageInRect(NSImage* image, NSView* view, const NSRect& rect) {
  // If there is an image, make sure we calculated the target size
  // correctly.
  DCHECK(!image || NSEqualSizes([image size], rect.size));
  [image drawInRect:rect
           fromRect:NSZeroRect  // Entire image
          operation:NSCompositeSourceOver
           fraction:1.0
       neverFlipped:YES];
}

// Helper function to generate an attributed string containing
// |anImage|.  If |baselineAdjustment| is 0, the image sits on the
// text baseline, positive values shift it up, negative values shift
// it down.
NSAttributedString* AttributedStringForImage(NSImage* anImage,
                                             CGFloat baselineAdjustment) {
  scoped_nsobject<AutocompleteTextAttachmentCell> attachmentCell(
      [[AutocompleteTextAttachmentCell alloc] initImageCell:anImage]);
  scoped_nsobject<NSTextAttachment> attachment(
      [[NSTextAttachment alloc] init]);
  [attachment setAttachmentCell:attachmentCell];

  scoped_nsobject<NSMutableAttributedString> as(
      [[NSAttributedString attributedStringWithAttachment:attachment]
        mutableCopy]);
  [as addAttribute:NSBaselineOffsetAttributeName
      value:[NSNumber numberWithFloat:baselineAdjustment]
      range:NSMakeRange(0, [as length])];

  return [[as copy] autorelease];
}

// Calculate the positions for a set of decorations.  |frame| is the
// overall frame to do layout in, |remaining_frame| will get the
// left-over space.  |all_decorations| is the set of decorations to
// lay out, |decorations| will be set to the decorations which are
// visible and which fit, in the same order as |all_decorations|,
// while |decoration_frames| will be the corresponding frames.
// |x_edge| describes the edge to layout the decorations against
// (|NSMinXEdge| or |NSMaxXEdge|).  |initial_padding| is the padding
// from the edge of |cell_frame| (|kDecorationHorizontalPad| is used
// between decorations).
void CalculatePositionsHelper(
    NSRect frame,
    const std::vector<LocationBarDecoration*>& all_decorations,
    NSRectEdge x_edge,
    CGFloat initial_padding,
    std::vector<LocationBarDecoration*>* decorations,
    std::vector<NSRect>* decoration_frames,
    NSRect* remaining_frame) {
  DCHECK(x_edge == NSMinXEdge || x_edge == NSMaxXEdge);
  DCHECK_EQ(decorations->size(), decoration_frames->size());

  // The outer-most decoration will be inset a bit further from the
  // edge.
  CGFloat padding = initial_padding;

  for (size_t i = 0; i < all_decorations.size(); ++i) {
    if (all_decorations[i]->IsVisible()) {
      NSRect padding_rect, available;

      // Peel off the outside padding.
      NSDivideRect(frame, &padding_rect, &available, padding, x_edge);

      // Find out how large the decoration will be in the remaining
      // space.
      const CGFloat used_width =
          all_decorations[i]->GetWidthForSpace(NSWidth(available));

      if (used_width != LocationBarDecoration::kOmittedWidth) {
        DCHECK_GT(used_width, 0.0);
        NSRect decoration_frame;

        // Peel off the desired width, leaving the remainder in
        // |frame|.
        NSDivideRect(available, &decoration_frame, &frame,
                     used_width, x_edge);

        decorations->push_back(all_decorations[i]);
        decoration_frames->push_back(decoration_frame);
        DCHECK_EQ(decorations->size(), decoration_frames->size());

        // Adjust padding for between decorations.
        padding = kDecorationHorizontalPad;
      }
    }
  }

  DCHECK_EQ(decorations->size(), decoration_frames->size());
  *remaining_frame = frame;
}

// Helper function for calculating placement of decorations w/in the
// cell.  |frame| is the cell's boundary rectangle, |remaining_frame|
// will get any space left after decorations are laid out (for text).
// |left_decorations| is a set of decorations for the left-hand side
// of the cell, |right_decorations| for the right-hand side.
// |decorations| will contain the resulting visible decorations, and
// |decoration_frames| will contain their frames in the same
// coordinates as |frame|.  Decorations will be ordered left to right.
void CalculatePositionsInFrame(
    NSRect frame,
    const std::vector<LocationBarDecoration*>& left_decorations,
    const std::vector<LocationBarDecoration*>& right_decorations,
    std::vector<LocationBarDecoration*>* decorations,
    std::vector<NSRect>* decoration_frames,
    NSRect* remaining_frame) {
  decorations->clear();
  decoration_frames->clear();

  // Layout |left_decorations| against the LHS.
  CalculatePositionsHelper(frame, left_decorations,
                           NSMinXEdge, kLeftDecorationXOffset,
                           decorations, decoration_frames, &frame);
  DCHECK_EQ(decorations->size(), decoration_frames->size());

  // Capture the number of visible left-hand decorations.
  const size_t left_count = decorations->size();

  // Layout |right_decorations| against the RHS.
  CalculatePositionsHelper(frame, right_decorations,
                           NSMaxXEdge, kRightDecorationXOffset,
                           decorations, decoration_frames, &frame);
  DCHECK_EQ(decorations->size(), decoration_frames->size());

  // Reverse the right-hand decorations so that overall everything is
  // sorted left to right.
  std::reverse(decorations->begin() + left_count, decorations->end());
  std::reverse(decoration_frames->begin() + left_count,
               decoration_frames->end());

  *remaining_frame = frame;
}

}  // namespace

@implementation AutocompleteTextAttachmentCell

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)aView {
  // Draw image with |DrawImageInRect()| to get consistent
  // flipped treatment.
  DrawImageInRect([self image], aView, cellFrame);
}

@end

@implementation AutocompleteTextFieldIcon

@synthesize rect = rect_;
@synthesize view = view_;

// Private helper.
- (id)initWithView:(LocationBarViewMac::LocationBarImageView*)view
           isLabel:(BOOL)isLabel {
  self = [super init];
  if (self) {
    isLabel_ = isLabel;
    view_ = view;
    rect_ = NSZeroRect;
  }
  return self;
}

- (id)initImageWithView:(LocationBarViewMac::LocationBarImageView*)view {
  return [self initWithView:view isLabel:NO];
}

- (id)initLabelWithView:(LocationBarViewMac::LocationBarImageView*)view {
  return [self initWithView:view isLabel:YES];
}

- (void)positionInFrame:(NSRect)frame {
  if (isLabel_) {
    NSAttributedString* label = view_->GetLabel();
    DCHECK(label);
    const CGFloat labelWidth = ceil([label size].width);
    rect_ = NSMakeRect(NSMaxX(frame) - labelWidth,
                       NSMinY(frame) + kIconLabelYOffset,
                       labelWidth, NSHeight(frame) - kIconLabelYOffset);
  } else {
    const NSSize imageSize = view_->GetImageSize();
    const CGFloat yOffset = floor((NSHeight(frame) - imageSize.height) / 2);
    rect_ = NSMakeRect(NSMaxX(frame) - imageSize.width,
                       NSMinY(frame) + yOffset,
                       imageSize.width, imageSize.height);
  }
}

- (void)drawInView:(NSView*)controlView {
  // Make sure someone called |-positionInFrame:|.
  DCHECK(!NSIsEmptyRect(rect_));
  if (isLabel_) {
    NSAttributedString* label = view_->GetLabel();
    [label drawInRect:rect_];
  } else {
    DrawImageInRect(view_->GetImage(), controlView, rect_);
  }
}

@end

@implementation AutocompleteTextFieldCell

// @synthesize doesn't seem to compile for this transition.
- (NSAttributedString*)hintString {
  return hintString_.get();
}

- (CGFloat)baselineAdjust {
  return kBaselineAdjust;
}

- (CGFloat)cornerRadius {
  return kCornerRadius;
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

  // Adjust for space between editor and decorations.
  width -= 2 * kEditorHorizontalInset;

  // If |hintString_| is now too wide, clear it so that we don't pass
  // the equality tests.
  if (hintString_ && WidthForHint(hintString_) > width) {
    [self clearHint];
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
    NSAttributedString* is =
        AttributedStringForImage(anImage, kKeywordHintImageBaseline);

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

  // If |hintString_| is now too wide, clear it so that we don't pass
  // the equality tests.
  if (hintString_ && WidthForHint(hintString_) > width) {
    [self clearHint];
  }

  if (![[hintString_ string] isEqualToString:aString]) {
    scoped_nsobject<NSAttributedString> as(
        [[NSAttributedString alloc] initWithString:aString
                                        attributes:[self hintAttributes]]);

    // If too wide, don't keep the hint.
    hintString_.reset(WidthForHint(as) > width ? nil : [as copy]);
  }
}

- (void)clearHint {
  hintString_.reset();
}

- (void)setPageActionViewList:(LocationBarViewMac::PageActionViewList*)list {
  page_action_views_ = list;
}

- (void)setContentSettingViewsList:
    (LocationBarViewMac::ContentSettingViews*)views {
  content_setting_views_ = views;
}

- (void)clearDecorations {
  leftDecorations_.clear();
  rightDecorations_.clear();
}

- (void)addLeftDecoration:(LocationBarDecoration*)decoration {
  leftDecorations_.push_back(decoration);
}

- (void)addRightDecoration:(LocationBarDecoration*)decoration {
  rightDecorations_.push_back(decoration);
}

- (CGFloat)availableWidthInFrame:(const NSRect)frame {
  std::vector<LocationBarDecoration*> decorations;
  std::vector<NSRect> decorationFrames;
  NSRect textFrame;
  CalculatePositionsInFrame(frame, leftDecorations_, rightDecorations_,
                            &decorations, &decorationFrames, &textFrame);

  return NSWidth(textFrame);
}

- (NSRect)frameForDecoration:(const LocationBarDecoration*)aDecoration
                     inFrame:(NSRect)cellFrame {
  // Short-circuit if the decoration is known to be not visible.
  if (aDecoration && !aDecoration->IsVisible())
    return NSZeroRect;

  // Layout the decorations.
  std::vector<LocationBarDecoration*> decorations;
  std::vector<NSRect> decorationFrames;
  NSRect textFrame;
  CalculatePositionsInFrame(cellFrame, leftDecorations_, rightDecorations_,
                            &decorations, &decorationFrames, &textFrame);

  // Find our decoration and return the corresponding frame.
  std::vector<LocationBarDecoration*>::const_iterator iter =
      std::find(decorations.begin(), decorations.end(), aDecoration);
  if (iter != decorations.end()) {
    const size_t index = iter - decorations.begin();
    return decorationFrames[index];
  }

  // Decorations which are not visible should have been filtered out
  // at the top, but return |NSZeroRect| rather than a 0-width rect
  // for consistency.
  NOTREACHED();
  return NSZeroRect;
}

// Overriden to account for the hint strings and hint icons.
- (NSRect)textFrameForFrame:(NSRect)cellFrame {
  // Get the frame adjusted for decorations.
  std::vector<LocationBarDecoration*> decorations;
  std::vector<NSRect> decorationFrames;
  NSRect textFrame = [super textFrameForFrame:cellFrame];
  CalculatePositionsInFrame(textFrame, leftDecorations_, rightDecorations_,
                            &decorations, &decorationFrames, &textFrame);

  // NOTE: This function must closely match the logic in
  // |-drawInteriorWithFrame:inView:|.

  // Leave room for items on the right (SSL label, page actions, etc).
  // Icons are laid out in |cellFrame| rather than |textFrame| for
  // consistency with drawing code.
  NSArray* icons = [self layedOutIcons:cellFrame];
  if ([icons count]) {
    // Max x for resulting text frame.
    const CGFloat maxX = NSMinX([[icons objectAtIndex:0] rect]);
    textFrame.size.width = maxX - NSMinX(textFrame);
  }

  // Keyword string or hint string if they fit.
  if (hintString_) {
    const CGFloat hintWidth(WidthForHint(hintString_));

    // TODO(shess): This could be better.  Show the hint until the
    // non-hint text bumps against it?
    if (hintWidth < NSWidth(textFrame)) {
      textFrame.size.width -= hintWidth;
    }
  }

  return textFrame;
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
  if (!view->IsVisible())
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

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  std::vector<LocationBarDecoration*> decorations;
  std::vector<NSRect> decorationFrames;
  NSRect workingFrame;
  CalculatePositionsInFrame(cellFrame, leftDecorations_, rightDecorations_,
                            &decorations, &decorationFrames, &workingFrame);

  // Draw the decorations first.
  for (size_t i = 0; i < decorations.size(); ++i) {
    if (decorations[i])
      decorations[i]->DrawInFrame(decorationFrames[i], controlView);
  }

  // NOTE: This function must closely match the logic in
  // |-textFrameForFrame:|.

  NSArray* icons = [self layedOutIcons:cellFrame];
  for (AutocompleteTextFieldIcon* icon in icons) {
    [icon drawInView:controlView];
  }
  if ([icons count]) {
    // Max x for resulting text frame.
    const CGFloat maxX = NSMinX([[icons objectAtIndex:0] rect]);
    workingFrame.size.width = maxX - NSMinX(workingFrame);
  }

  // Keyword string or hint string if they fit.
  if (hintString_) {
    const CGFloat hintWidth(WidthForHint(hintString_));

    // TODO(shess): This could be better.  Show the hint until the
    // non-hint text bumps against it?
    if (hintWidth < NSWidth(workingFrame)) {
      [self drawHintWithFrame:cellFrame inView:controlView];
      workingFrame.size.width -= hintWidth;
    }
  }

  // Superclass draws text portion WRT original |cellFrame|.
  [super drawInteriorWithFrame:cellFrame inView:controlView];
}

- (NSArray*)layedOutIcons:(NSRect)cellFrame {
  // Trim the decoration area from |cellFrame|.  This is duplicate
  // work WRT the caller in some cases, but locating this here is
  // simpler, and this code will go away soon.
  std::vector<LocationBarDecoration*> decorations;
  std::vector<NSRect> decorationFrames;
  CalculatePositionsInFrame(cellFrame, leftDecorations_, rightDecorations_,
                            &decorations, &decorationFrames, &cellFrame);

  // The set of views to display right-justified in the cell, from
  // left to right.
  NSMutableArray* result = [NSMutableArray array];

  // Collect the image views for bulk processing.
  // TODO(shess): Refactor with LocationBarViewMac to make the
  // different types of items more consistent.
  std::vector<LocationBarViewMac::LocationBarImageView*> views;

  if (content_setting_views_) {
    views.insert(views.end(),
                 content_setting_views_->begin(),
                 content_setting_views_->end());
  }

  // TODO(shess): Previous implementation of this method made a
  // right-to-left array, so add the page-action items in that order.
  // As part of the refactor mentioned above, lay everything out
  // nicely left-to-right.
  for (size_t i = [self pageActionCount]; i-- > 0;) {
    views.push_back(page_action_views_->ViewAt(i));
  }

  // Load the visible views into |result|.
  for (std::vector<LocationBarViewMac::LocationBarImageView*>::const_iterator
           iter = views.begin(); iter != views.end(); ++iter) {
    if ((*iter)->IsVisible()) {
      scoped_nsobject<AutocompleteTextFieldIcon> icon(
          [[AutocompleteTextFieldIcon alloc] initImageWithView:*iter]);
      [result addObject:icon];
    }
  }

  // Padding from right-hand decoration.  There should always be at
  // least one (the star).
  cellFrame.size.width -= kDecorationHorizontalPad;

  // Position each view within the frame from right to left.
  for (AutocompleteTextFieldIcon* icon in [result reverseObjectEnumerator]) {
    [icon positionInFrame:cellFrame];

    // Trim the icon's space from the frame.
    cellFrame.size.width =
        NSMinX([icon rect]) - NSMinX(cellFrame) - kDecorationHorizontalPad;
  }
  return result;
}

// Returns the decoration under |theEvent|, or NULL if none.
// Like |-iconForEvent:inRect:ofView:|, but for decorations.
- (LocationBarDecoration*)decorationForEvent:(NSEvent*)theEvent
                                      inRect:(NSRect)cellFrame
                                      ofView:(AutocompleteTextField*)controlView
{
  const BOOL flipped = [controlView isFlipped];
  const NSPoint location =
      [controlView convertPoint:[theEvent locationInWindow] fromView:nil];

  std::vector<LocationBarDecoration*> decorations;
  std::vector<NSRect> decorationFrames;
  NSRect textFrame;
  CalculatePositionsInFrame(cellFrame, leftDecorations_, rightDecorations_,
                            &decorations, &decorationFrames, &textFrame);

  for (size_t i = 0; i < decorations.size(); ++i) {
    if (NSMouseInRect(location, decorationFrames[i], flipped))
      return decorations[i];
  }

  return NULL;
}

- (AutocompleteTextFieldIcon*)iconForEvent:(NSEvent*)theEvent
                                    inRect:(NSRect)cellFrame
                                    ofView:(AutocompleteTextField*)controlView {
  const BOOL flipped = [controlView isFlipped];
  const NSPoint location =
      [controlView convertPoint:[theEvent locationInWindow] fromView:nil];

  for (AutocompleteTextFieldIcon* icon in [self layedOutIcons:cellFrame]) {
    if (NSMouseInRect(location, [icon rect], flipped))
      return icon;
  }

  return nil;
}

- (NSMenu*)actionMenuForEvent:(NSEvent*)theEvent
                       inRect:(NSRect)cellFrame
                       ofView:(AutocompleteTextField*)controlView {
  LocationBarDecoration* decoration =
      [self decorationForEvent:theEvent inRect:cellFrame ofView:controlView];
  if (decoration)
    return decoration->GetMenu();

  AutocompleteTextFieldIcon*
      icon = [self iconForEvent:theEvent inRect:cellFrame ofView:controlView];
  if (icon)
    return [icon view]->GetMenu();
  return nil;
}

- (BOOL)mouseDown:(NSEvent*)theEvent
           inRect:(NSRect)cellFrame
           ofView:(AutocompleteTextField*)controlView {
  LocationBarDecoration* decoration =
      [self decorationForEvent:theEvent inRect:cellFrame ofView:controlView];
  AutocompleteTextFieldIcon* icon =
      [self iconForEvent:theEvent inRect:cellFrame ofView:controlView];
  if (!decoration && !icon)
    return NO;

  NSRect decorationRect = NSZeroRect;
  if (icon) {
    decorationRect = [icon rect];
  } else if (decoration) {
    decorationRect = [self frameForDecoration:decoration inFrame:cellFrame];
  }

  // If the icon is draggable, then initiate a drag if the user drags
  // or holds the mouse down for awhile.
  if ((icon && [icon view]->IsDraggable()) ||
      (decoration && decoration->IsDraggable())) {
    DCHECK(icon || decoration);
    NSDate* timeout =
        [NSDate dateWithTimeIntervalSinceNow:kLocationIconDragTimeout];
    NSEvent* event = [NSApp nextEventMatchingMask:(NSLeftMouseDraggedMask |
                                                   NSLeftMouseUpMask)
                                        untilDate:timeout
                                           inMode:NSEventTrackingRunLoopMode
                                          dequeue:YES];
    if (!event || [event type] == NSLeftMouseDragged) {
      NSPasteboard* pboard = nil;
      if (icon) pboard = [icon view]->GetDragPasteboard();
      if (decoration) pboard = decoration->GetDragPasteboard();
      DCHECK(pboard);

      // TODO(shess): My understanding is that the -isFlipped
      // adjustment should not be necessary.  But without it, the
      // image is nowhere near the cursor.  Perhaps the icon's rect is
      // incorrectly calculated?
      // http://crbug.com/40711
      NSPoint dragPoint = decorationRect.origin;
      if ([controlView isFlipped])
        dragPoint.y += NSHeight(decorationRect);

      NSImage* image = nil;
      if (icon) image = [icon view]->GetImage();
      if (decoration) image = decoration->GetDragImage();
      [controlView dragImage:image
                          at:dragPoint
                      offset:NSZeroSize
                       event:event ? event : theEvent
                  pasteboard:pboard
                      source:self
                   slideBack:YES];
      return YES;
    }

    // On mouse-up fall through to mouse-pressed case.
    DCHECK_EQ([event type], NSLeftMouseUp);
  }

  if (icon) {
    [icon view]->OnMousePressed(decorationRect);
  } else if (decoration) {
    if (!decoration->OnMousePressed(decorationRect))
      return NO;
  }

  return YES;
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  return NSDragOperationCopy;
}

@end
