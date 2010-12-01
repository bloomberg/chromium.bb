// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_cell.h"

#include "base/logging.h"
#import "chrome/browser/ui/cocoa/image_utils.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_decoration.h"

namespace {

const CGFloat kBaselineAdjust = 3.0;

// Matches the clipping radius of |GradientButtonCell|.
const CGFloat kCornerRadius = 4.0;

// How far to inset the left-hand decorations from the field's bounds.
const CGFloat kLeftDecorationXOffset = 5.0;

// How far to inset the right-hand decorations from the field's bounds.
// TODO(shess): Why is this different from |kLeftDecorationXOffset|?
// |kDecorationOuterXOffset|?
const CGFloat kRightDecorationXOffset = 5.0;

// The amount of padding on either side reserved for drawing
// decorations.  [Views has |kItemPadding| == 3.]
const CGFloat kDecorationHorizontalPad = 3.0;

// How long to wait for mouse-up on the location icon before assuming
// that the user wants to drag.
const NSTimeInterval kLocationIconDragTimeout = 0.25;

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
// As a convenience returns the index of the first right-hand
// decoration.
size_t CalculatePositionsInFrame(
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
  return left_count;
}

}  // namespace

@implementation AutocompleteTextFieldCell

- (CGFloat)baselineAdjust {
  return kBaselineAdjust;
}

- (CGFloat)cornerRadius {
  return kCornerRadius;
}

- (BOOL)shouldDrawBezel {
  return YES;
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

// Overriden to account for the decorations.
- (NSRect)textFrameForFrame:(NSRect)cellFrame {
  // Get the frame adjusted for decorations.
  std::vector<LocationBarDecoration*> decorations;
  std::vector<NSRect> decorationFrames;
  NSRect textFrame = [super textFrameForFrame:cellFrame];
  CalculatePositionsInFrame(textFrame, leftDecorations_, rightDecorations_,
                            &decorations, &decorationFrames, &textFrame);

  // NOTE: This function must closely match the logic in
  // |-drawInteriorWithFrame:inView:|.

  return textFrame;
}

- (NSRect)textCursorFrameForFrame:(NSRect)cellFrame {
  std::vector<LocationBarDecoration*> decorations;
  std::vector<NSRect> decorationFrames;
  NSRect textFrame;
  size_t left_count =
      CalculatePositionsInFrame(cellFrame, leftDecorations_, rightDecorations_,
                                &decorations, &decorationFrames, &textFrame);

  // Determine the left-most extent for the i-beam cursor.
  CGFloat minX = NSMinX(textFrame);
  for (size_t index = left_count; index--; ) {
    if (decorations[index]->AcceptsMousePress())
      break;

    // If at leftmost decoration, expand to edge of cell.
    if (!index) {
      minX = NSMinX(cellFrame);
    } else {
      minX = NSMinX(decorationFrames[index]) - kDecorationHorizontalPad;
    }
  }

  // Determine the right-most extent for the i-beam cursor.
  CGFloat maxX = NSMaxX(textFrame);
  for (size_t index = left_count; index < decorations.size(); ++index) {
    if (decorations[index]->AcceptsMousePress())
      break;

    // If at rightmost decoration, expand to edge of cell.
    if (index == decorations.size() - 1) {
      maxX = NSMaxX(cellFrame);
    } else {
      maxX = NSMaxX(decorationFrames[index]) + kDecorationHorizontalPad;
    }
  }

  // I-beam cursor covers left-most to right-most.
  return NSMakeRect(minX, NSMinY(textFrame), maxX - minX, NSHeight(textFrame));
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  std::vector<LocationBarDecoration*> decorations;
  std::vector<NSRect> decorationFrames;
  NSRect workingFrame;
  CalculatePositionsInFrame(cellFrame, leftDecorations_, rightDecorations_,
                            &decorations, &decorationFrames, &workingFrame);

  // Draw the decorations.
  for (size_t i = 0; i < decorations.size(); ++i) {
    if (decorations[i])
      decorations[i]->DrawInFrame(decorationFrames[i], controlView);
  }

  // NOTE: This function must closely match the logic in
  // |-textFrameForFrame:|.

  // Superclass draws text portion WRT original |cellFrame|.
  [super drawInteriorWithFrame:cellFrame inView:controlView];
}

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

- (NSMenu*)decorationMenuForEvent:(NSEvent*)theEvent
                           inRect:(NSRect)cellFrame
                           ofView:(AutocompleteTextField*)controlView {
  LocationBarDecoration* decoration =
      [self decorationForEvent:theEvent inRect:cellFrame ofView:controlView];
  if (decoration)
    return decoration->GetMenu();
  return nil;
}

- (BOOL)mouseDown:(NSEvent*)theEvent
           inRect:(NSRect)cellFrame
           ofView:(AutocompleteTextField*)controlView {
  LocationBarDecoration* decoration =
      [self decorationForEvent:theEvent inRect:cellFrame ofView:controlView];
  if (!decoration || !decoration->AcceptsMousePress())
    return NO;

  NSRect decorationRect =
      [self frameForDecoration:decoration inFrame:cellFrame];

  // If the decoration is draggable, then initiate a drag if the user
  // drags or holds the mouse down for awhile.
  if (decoration->IsDraggable()) {
    NSDate* timeout =
        [NSDate dateWithTimeIntervalSinceNow:kLocationIconDragTimeout];
    NSEvent* event = [NSApp nextEventMatchingMask:(NSLeftMouseDraggedMask |
                                                   NSLeftMouseUpMask)
                                        untilDate:timeout
                                           inMode:NSEventTrackingRunLoopMode
                                          dequeue:YES];
    if (!event || [event type] == NSLeftMouseDragged) {
      NSPasteboard* pboard = decoration->GetDragPasteboard();
      DCHECK(pboard);

      NSImage* image = decoration->GetDragImage();
      DCHECK(image);

      NSRect dragImageRect = decoration->GetDragImageFrame(decorationRect);

      // If the original click is not within |dragImageRect|, then
      // center the image under the mouse.  Otherwise, will drag from
      // where the click was on the image.
      const NSPoint mousePoint =
          [controlView convertPoint:[theEvent locationInWindow] fromView:nil];
      if (!NSMouseInRect(mousePoint, dragImageRect, [controlView isFlipped])) {
        dragImageRect.origin =
            NSMakePoint(mousePoint.x - NSWidth(dragImageRect) / 2.0,
                        mousePoint.y - NSHeight(dragImageRect) / 2.0);
      }

      // -[NSView dragImage:at:*] wants the images lower-left point,
      // regardless of -isFlipped.  Converting the rect to window base
      // coordinates doesn't require any special-casing.  Note that
      // -[NSView dragFile:fromRect:*] takes a rect rather than a
      // point, likely for this exact reason.
      const NSPoint dragPoint =
          [controlView convertRect:dragImageRect toView:nil].origin;
      [[controlView window] dragImage:image
                                   at:dragPoint
                               offset:NSZeroSize
                                event:theEvent
                           pasteboard:pboard
                               source:self
                            slideBack:YES];

      return YES;
    }

    // On mouse-up fall through to mouse-pressed case.
    DCHECK_EQ([event type], NSLeftMouseUp);
  }

  if (!decoration->OnMousePressed(decorationRect))
    return NO;

  return YES;
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  return NSDragOperationCopy;
}

- (void)updateToolTipsInRect:(NSRect)cellFrame
                      ofView:(AutocompleteTextField*)controlView {
  std::vector<LocationBarDecoration*> decorations;
  std::vector<NSRect> decorationFrames;
  NSRect textFrame;
  CalculatePositionsInFrame(cellFrame, leftDecorations_, rightDecorations_,
                            &decorations, &decorationFrames, &textFrame);

  for (size_t i = 0; i < decorations.size(); ++i) {
    NSString* tooltip = decorations[i]->GetToolTip();
    if ([tooltip length] > 0)
      [controlView addToolTip:tooltip forRect:decorationFrames[i]];
  }
}

@end
