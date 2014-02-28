// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/styled_text_field_cell.h"

@class AutocompleteTextField;
class LocationBarDecoration;

// AutocompleteTextFieldCell extends StyledTextFieldCell to provide support for
// certain decorations to be applied to the field.  These are the search hint
// ("Type to search" on the right-hand side), the keyword hint ("Press [Tab] to
// search Engine" on the right-hand side), and keyword mode ("Search Engine:" in
// a button-like token on the left-hand side).
@interface AutocompleteTextFieldCell : StyledTextFieldCell {
 @private
  // Decorations which live to the left and right of the text, ordered
  // from outside in.  Decorations are owned by |LocationBarViewMac|.
  std::vector<LocationBarDecoration*> leftDecorations_;
  std::vector<LocationBarDecoration*> rightDecorations_;

  // If YES then the text field will not draw a focus ring or show the insertion
  // pointer.
  BOOL hideFocusState_;

  // YES if this field is shown in a popup window.
  BOOL isPopupMode_;

  // Retains the NSEvent that caused the controlView to become firstResponder.
  base::scoped_nsobject<NSEvent> focusEvent_;
}

@property(assign, nonatomic) BOOL isPopupMode;

// Line height used for text in this cell.
- (CGFloat)lineHeight;

// Clear |leftDecorations_| and |rightDecorations_|.
- (void)clearDecorations;

// Add a new left-side decoration to the right of the existing
// left-side decorations.
- (void)addLeftDecoration:(LocationBarDecoration*)decoration;

// Add a new right-side decoration to the left of the existing
// right-side decorations.
- (void)addRightDecoration:(LocationBarDecoration*)decoration;

// The width available after accounting for decorations.
- (CGFloat)availableWidthInFrame:(const NSRect)frame;

// Return the frame for |aDecoration| if the cell is in |cellFrame|.
// Returns |NSZeroRect| for decorations which are not currently
// visible.
- (NSRect)frameForDecoration:(const LocationBarDecoration*)aDecoration
                     inFrame:(NSRect)cellFrame;

// Find the decoration under the event.  |NULL| if |theEvent| is not
// over anything.
- (LocationBarDecoration*)decorationForEvent:(NSEvent*)theEvent
                                      inRect:(NSRect)cellFrame
                                      ofView:(AutocompleteTextField*)field;

// Return the appropriate menu for any decorations under event.
// Returns nil if no menu is present for the decoration, or if the
// event is not over a decoration.
- (NSMenu*)decorationMenuForEvent:(NSEvent*)theEvent
                           inRect:(NSRect)cellFrame
                           ofView:(AutocompleteTextField*)controlView;

// Called by |AutocompleteTextField| to let page actions intercept
// clicks.  Returns |YES| if the click has been intercepted.
- (BOOL)mouseDown:(NSEvent*)theEvent
           inRect:(NSRect)cellFrame
           ofView:(AutocompleteTextField*)controlView;

// These messages are passed down from the AutocompleteTextField, where they are
// received from tracking areas registered for decorations that act as buttons.
- (void)mouseEntered:(NSEvent*)theEvent
              inView:(AutocompleteTextField*)controlView;
- (void)mouseExited:(NSEvent*)theEvent
             inView:(AutocompleteTextField*)controlView;

// Setup tracking areas for the decorations that are part of this cell, so they
// can receive |mouseEntered:| and |mouseExited:| events.
- (void)setUpTrackingAreasInRect:(NSRect)frame
                          ofView:(AutocompleteTextField*)view;

// Overridden from StyledTextFieldCell to include decorations adjacent
// to the text area which don't handle mouse clicks themselves.
// Keyword-search bubble, for instance.
- (NSRect)textCursorFrameForFrame:(NSRect)cellFrame;

// Setup decoration tooltips on |controlView| by calling
// |-addToolTip:forRect:|.
- (void)updateToolTipsInRect:(NSRect)cellFrame
                      ofView:(AutocompleteTextField*)controlView;

// Gets and sets |hideFocusState|. This allows the text field to have focus but
// to appear unfocused.
- (BOOL)hideFocusState;
- (void)setHideFocusState:(BOOL)hideFocusState
                   ofView:(AutocompleteTextField*)controlView;

// Handles the |event| that caused |controlView| to become firstResponder.
// If it is a mouse click on a ButtonDecoration, focus notifications are
// postponed until the ButtonDecoration's OnMousePressed() was invoked.
// Otherwise, they are called immediately.
- (void)handleFocusEvent:(NSEvent*)event
                  ofView:(AutocompleteTextField*)controlView;
@end
