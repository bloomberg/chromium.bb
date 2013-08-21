// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_matrix.h"

#include "base/logging.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_cell.h"

namespace {

// NSEvent -buttonNumber for middle mouse button.
const NSInteger kMiddleButtonNumber = 2;

}  // namespace

@interface OmniboxPopupMatrix()
- (void)resetTrackingArea;
- (void)highlightRowAt:(NSInteger)rowIndex;
- (void)highlightRowUnder:(NSEvent*)theEvent;
- (BOOL)selectCellForEvent:(NSEvent*)theEvent;
@end

@implementation OmniboxPopupMatrix

- (id)initWithDelegate:(OmniboxPopupMatrixDelegate*)delegate {
  if ((self = [super initWithFrame:NSZeroRect])) {
    delegate_ = delegate;
    [self setCellClass:[OmniboxPopupCell class]];

    // Cells pack with no spacing.
    [self setIntercellSpacing:NSMakeSize(0.0, 0.0)];

    [self setDrawsBackground:YES];
    [self setBackgroundColor:[NSColor controlBackgroundColor]];
    [self renewRows:0 columns:1];
    [self setAllowsEmptySelection:YES];
    [self setMode:NSRadioModeMatrix];
    [self deselectAllCells];

    [self resetTrackingArea];
  }
  return self;
}

- (void)setDelegate:(OmniboxPopupMatrixDelegate*)delegate {
  delegate_ = delegate;
}

- (NSInteger)highlightedRow {
  NSArray* cells = [self cells];
  const NSUInteger count = [cells count];
  for(NSUInteger i = 0; i < count; ++i) {
    if ([[cells objectAtIndex:i] isHighlighted]) {
      return i;
    }
  }
  return -1;
}

- (void)updateTrackingAreas {
  [self resetTrackingArea];
  [super updateTrackingAreas];
}

// Callbacks from tracking area.
- (void)mouseEntered:(NSEvent*)theEvent {
  [self highlightRowUnder:theEvent];
}

- (void)mouseMoved:(NSEvent*)theEvent {
  [self highlightRowUnder:theEvent];
}

- (void)mouseExited:(NSEvent*)theEvent {
  [self highlightRowAt:-1];
}

// The tracking area events aren't forwarded during a drag, so handle
// highlighting manually for middle-click and middle-drag.
- (void)otherMouseDown:(NSEvent*)theEvent {
  if ([theEvent buttonNumber] == kMiddleButtonNumber) {
    [self highlightRowUnder:theEvent];
  }
  [super otherMouseDown:theEvent];
}

- (void)otherMouseDragged:(NSEvent*)theEvent {
  if ([theEvent buttonNumber] == kMiddleButtonNumber) {
    [self highlightRowUnder:theEvent];
  }
  [super otherMouseDragged:theEvent];
}

- (void)otherMouseUp:(NSEvent*)theEvent {
  // Only intercept middle button.
  if ([theEvent buttonNumber] != kMiddleButtonNumber) {
    [super otherMouseUp:theEvent];
    return;
  }

  // -otherMouseDragged: should always have been called at this location, but
  // make sure the user is getting the right feedback.
  [self highlightRowUnder:theEvent];

  const NSInteger highlightedRow = [self highlightedRow];
  if (highlightedRow != -1) {
    DCHECK(delegate_);
    delegate_->OnMatrixRowMiddleClicked(self, highlightedRow);
  }
}

// Track the mouse until released, keeping the cell under the mouse selected.
// If the mouse wanders off-view, revert to the originally-selected cell. If
// the mouse is released over a cell, call the delegate to open the row's URL.
- (void)mouseDown:(NSEvent*)theEvent {
  NSCell* selectedCell = [self selectedCell];

  // Clear any existing highlight.
  [self highlightRowAt:-1];

  do {
    if (![self selectCellForEvent:theEvent]) {
      [self selectCell:selectedCell];
    }

    const NSUInteger mask = NSLeftMouseUpMask | NSLeftMouseDraggedMask;
    theEvent = [[self window] nextEventMatchingMask:mask];
  } while ([theEvent type] == NSLeftMouseDragged);

  // Do not message the delegate if released outside view.
  if (![self selectCellForEvent:theEvent]) {
    [self selectCell:selectedCell];
  } else {
    const NSInteger selectedRow = [self selectedRow];

    // No row could be selected if the model failed to update.
    if (selectedRow == -1) {
      NOTREACHED();
      return;
    }

    DCHECK(delegate_);
    delegate_->OnMatrixRowClicked(self, selectedRow);
  }
}

- (void)resetTrackingArea {
  if (trackingArea_.get())
    [self removeTrackingArea:trackingArea_.get()];

  trackingArea_.reset([[CrTrackingArea alloc]
      initWithRect:[self frame]
           options:NSTrackingMouseEnteredAndExited |
                   NSTrackingMouseMoved |
                   NSTrackingActiveInActiveApp |
                   NSTrackingInVisibleRect
             owner:self
          userInfo:nil]);
  [self addTrackingArea:trackingArea_.get()];
}

- (void)highlightRowAt:(NSInteger)rowIndex {
  // highlightCell will be nil if rowIndex is out of range, so no cell will be
  // highlighted.
  NSCell* highlightCell = [self cellAtRow:rowIndex column:0];

  for (NSCell* cell in [self cells]) {
    [cell setHighlighted:(cell == highlightCell)];
  }
}

- (void)highlightRowUnder:(NSEvent*)theEvent {
  NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  NSInteger row, column;
  if ([self getRow:&row column:&column forPoint:point]) {
    [self highlightRowAt:row];
  } else {
    [self highlightRowAt:-1];
  }
}

// Select cell under |theEvent|, returning YES if a selection is made.
- (BOOL)selectCellForEvent:(NSEvent*)theEvent {
  NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];

  NSInteger row, column;
  if ([self getRow:&row column:&column forPoint:point]) {
    DCHECK_EQ(column, 0);
    DCHECK(delegate_);
    delegate_->OnMatrixRowSelected(self, row);
    return YES;
  }
  return NO;
}

@end
