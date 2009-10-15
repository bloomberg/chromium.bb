// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/autocomplete_text_field.h"

#include "base/logging.h"
#include "chrome/browser/cocoa/autocomplete_text_field_cell.h"

@implementation AutocompleteTextField

@synthesize observer = observer_;

+ (Class)cellClass {
  return [AutocompleteTextFieldCell class];
}

- (void)awakeFromNib {
  DCHECK([[self cell] isKindOfClass:[AutocompleteTextFieldCell class]]);
}

- (void)flagsChanged:(NSEvent*)theEvent {
  bool controlFlag = ([theEvent modifierFlags]&NSControlKeyMask) != 0;
  observer_->OnControlKeyChanged(controlFlag);
}

- (AutocompleteTextFieldCell*)autocompleteTextFieldCell {
  DCHECK([[self cell] isKindOfClass:[AutocompleteTextFieldCell class]]);
  return static_cast<AutocompleteTextFieldCell*>([self cell]);
}

// Cocoa text fields are edited by placing an NSTextView as subview,
// positioned by the cell's -editWithFrame:inView:... method.  Using
// the standard -makeFirstResponder: machinery to reposition the field
// editor results in resetting the field editor's editing state, which
// AutocompleteEditViewMac monitors.  This causes problems because
// editing can require the field editor to be repositioned, which
// could disrupt editing.  This code repositions the subview directly,
// which causes no editing-state changes.
- (void)resetFieldEditorFrameIfNeeded {
  AutocompleteTextFieldCell* cell = [self cell];
  if ([cell fieldEditorNeedsReset]) {
    // No change to bounds necessary if not editing.
    NSText* editor = [self currentEditor];
    if (editor) {
      // When editing, we should have exactly one subview, which is a
      // clipview containing the editor (for purposes of scrolling).
      NSArray* subviews = [self subviews];
      DCHECK_EQ([subviews count], 1U);
      DCHECK([editor isDescendantOf:self]);
      if ([subviews count] > 0) {
        const NSRect frame([cell drawingRectForBounds:[self bounds]]);
        [[subviews objectAtIndex:0] setFrame:frame];

        // Make sure the selection remains visible.
        // TODO(shess) Could this be janky?
        [editor scrollRangeToVisible:[editor selectedRange]];
      }
    }
    [cell setFieldEditorNeedsReset:NO];
  }
}

// Reroute events for the decoration area to the field editor.  This
// will cause the cursor to be moved as close to the edge where the
// event was seen as possible.
//
// The reason for this code's existence is subtle.  NSTextField
// implements text selection and editing in terms of a "field editor".
// This is an NSTextView which is installed as a subview of the
// control when the field becomes first responder.  When the field
// editor is installed, it will get -mouseDown: events and handle
// them, rather than the text field - EXCEPT for the event which
// caused the change in first responder, or events which fall in the
// decorations outside the field editor's area.  In that case, the
// default NSTextField code will setup the field editor all over
// again, which has the side effect of doing "select all" on the text.
// This effect can be observed with a normal NSTextField if you click
// in the narrow border area, and is only really a problem because in
// our case the focus ring surrounds decorations which look clickable.
//
// When the user first clicks on the field, after installing the field
// editor the default NSTextField code detects if the hit is in the
// field editor area, and if so sets the selection to {0,0} to clear
// the selection before forwarding the event to the field editor for
// processing (it will set the cursor position).  This also starts the
// click-drag selection machinery.
//
// This code does the same thing for cases where the click was in the
// decoration area.  This allows the user to click-drag starting from
// a decoration area and get the expected selection behaviour,
// likewise for multiple clicks in those areas.
- (void)mouseDown:(NSEvent *)theEvent {
  const NSPoint locationInWindow = [theEvent locationInWindow];
  const NSPoint location = [self convertPoint:locationInWindow fromView:nil];

  AutocompleteTextFieldCell* cell = [self cell];
  const NSRect textFrame([cell textFrameForFrame:[self bounds]]);

  // A version of the textFrame which extends across the field's
  // entire width.
  const NSRect bounds([self bounds]);
  const NSRect fullFrame(NSMakeRect(bounds.origin.x, textFrame.origin.y,
                                    bounds.size.width, textFrame.size.height));

  // If the mouse is in the editing area, or above or below where the
  // editing area would be if we didn't add decorations, forward to
  // NSTextField -mouseDown: because it does the right thing.  The
  // above/below test is needed because NSTextView treats mouse events
  // above/below as select-to-end-in-that-direction, which makes
  // things janky.
  if (NSMouseInRect(location, textFrame, [self isFlipped]) ||
      !NSMouseInRect(location, fullFrame, [self isFlipped])) {
    [super mouseDown:theEvent];

    // After the event has been handled, if the current event is a
    // mouse up and no selection was created (the mouse didn't move),
    // select the entire field.
    // NOTE(shess): This does not interfere with single-clicking to
    // place caret after a selection is made.  An NSTextField only has
    // a selection when it has a field editor.  The field editor is an
    // NSText subview, which will receive the -mouseDown: in that
    // case, and this code will never fire.
    NSText* editor = [self currentEditor];
    if (editor) {
      NSEvent* currentEvent = [NSApp currentEvent];
      if ([currentEvent type] == NSLeftMouseUp &&
          ![editor selectedRange].length) {
        [editor selectAll:nil];
      }
    }

    return;
  }

  // Check to see if the user clicked the hint icon in the cell. If so, we need
  // to display the page info window.
  const NSRect hintIconFrame = [cell hintImageFrameForFrame:[self bounds]];
  if (NSMouseInRect(location, hintIconFrame, [self isFlipped])) {
    observer_->OnSecurityIconClicked();
    return;
  }

  NSText* editor = [self currentEditor];

  // We should only be here if we accepted first-responder status and
  // have a field editor.  If one of these fires, it means some
  // assumptions are being broken.
  DCHECK(editor != nil);
  DCHECK([editor isDescendantOf:self]);

  // -becomeFirstResponder does a select-all, which we don't want
  // because it can lead to a dragged-text situation.  Clear the
  // selection (any valid empty selection will do).
  [editor setSelectedRange:NSMakeRange(0, 0)];

  // If the event is to the right of the editing area, scroll the
  // field editor to the end of the content so that the selection
  // doesn't initiate from somewhere in the middle of the text.
  if (location.x > NSMaxX(textFrame)) {
    [editor scrollRangeToVisible:NSMakeRange([[self stringValue] length], 0)];
  }

  [editor mouseDown:theEvent];
}

- (CGFloat)availableDecorationWidth {
  NSAttributedString* as = [self attributedStringValue];
  const NSSize size([as size]);
  const NSRect bounds([self bounds]);
  return NSWidth(bounds) - size.width;
}

- (void)setFrame:(NSRect)frameRect {
  [super setFrame:frameRect];
  if (observer_) {
    observer_->OnFrameChanged();
  }
}

@end
