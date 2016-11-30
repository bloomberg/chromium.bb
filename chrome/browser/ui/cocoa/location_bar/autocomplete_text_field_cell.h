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

  // Decorations with tracking areas attached to the AutocompleteTextField.
  std::vector<LocationBarDecoration*> mouseTrackingDecorations_;

  // If YES then the text field will not draw a focus ring or show the insertion
  // pointer.
  BOOL hideFocusState_;

  // YES if this field is shown in a popup window.
  BOOL isPopupMode_;

  // Retains the NSEvent that caused the controlView to become firstResponder.
  base::scoped_nsobject<NSEvent> focusEvent_;

  // The coordinate system line width that draws a single pixel line.
  CGFloat singlePixelLineWidth_;
}

@property(assign, nonatomic) BOOL isPopupMode;
@property(assign, nonatomic) CGFloat singlePixelLineWidth;

// Line height used for text in this cell.
- (CGFloat)lineHeight;

// Remove all of the tracking areas.
- (void)clearTrackingArea;

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
// Returns |NSZeroRect| for decorations which are not currently visible.
- (NSRect)frameForDecoration:(const LocationBarDecoration*)aDecoration
                     inFrame:(NSRect)cellFrame;

// Returns the frame representing the background of |decoration|. Also sets
// |isLeftDecoration| according to whether the decoration appears on the left or
// the right side of the text field.
- (NSRect)backgroundFrameForDecoration:(LocationBarDecoration*)decoration
                               inFrame:(NSRect)cellFrame
                      isLeftDecoration:(BOOL*)isLeftDecoration;

// Returns true if it's okay to drop dragged data into the view at the
// given location.
- (BOOL)canDropAtLocationInWindow:(NSPoint)location
                           ofView:(AutocompleteTextField*)controlView;

// Find the decoration under the location in the window. Return |NULL| if
// there's nothing in the location.
- (LocationBarDecoration*)decorationForLocationInWindow:(NSPoint)location
                                                 inRect:(NSRect)cellFrame
                                                 ofView:(AutocompleteTextField*)
                                                            field;

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

// Called by |AutocompleteTextField| to pass the mouse up event to the omnibox
// decorations.
- (void)mouseUp:(NSEvent*)theEvent
         inRect:(NSRect)cellFrame
         ofView:(AutocompleteTextField*)controlView;

// Overridden from StyledTextFieldCell to include decorations adjacent
// to the text area which don't handle mouse clicks themselves.
// Keyword-search bubble, for instance.
- (NSRect)textCursorFrameForFrame:(NSRect)cellFrame;

// Setup decoration tooltips and mouse tracking on |controlView| by calling
// |-addToolTip:forRect:| and |SetupTrackingArea()|.
- (void)updateMouseTrackingAndToolTipsInRect:(NSRect)cellFrame
                                      ofView:
                                          (AutocompleteTextField*)controlView;

// Gets and sets |hideFocusState|. This allows the text field to have focus but
// to appear unfocused.
- (BOOL)hideFocusState;
- (void)setHideFocusState:(BOOL)hideFocusState
                   ofView:(AutocompleteTextField*)controlView;

// Handles the |event| that caused |controlView| to become firstResponder.
- (void)handleFocusEvent:(NSEvent*)event
                  ofView:(AutocompleteTextField*)controlView;
@end

// Methods which are either only for testing, or only public for testing.
@interface AutocompleteTextFieldCell (TestingAPI)

// Returns |mouseTrackingDecorations_|.
- (const std::vector<LocationBarDecoration*>&)mouseTrackingDecorations;

@end
