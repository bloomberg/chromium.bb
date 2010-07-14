// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/cocoa/styled_text_field_cell.h"

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/location_bar/location_bar_view_mac.h"

class ExtensionAction;
class LocationBarDecoration;

// Holds a |LocationBarImageView| and its current rect. Do not keep references
// to this object, only use it directly after calling |-layedOutIcons:|.
// TODO(shess): This class is basically a helper for laying out the
// icons.  Try to refactor it away.  If that is not reasonable, at
// least split the image and label cases into subclasses once the
// Omnibox stuff is settled.
@interface AutocompleteTextFieldIcon : NSObject {
  // YES to draw the label part of |view_|, otherwise draw the image
  // part.
  BOOL isLabel_;

  // The frame rect of |view_|.
  NSRect rect_;

  // weak, owned by LocationBarViewMac.
  LocationBarViewMac::LocationBarImageView* view_;
}

@property(assign, nonatomic) NSRect rect;
@property(assign, nonatomic) LocationBarViewMac::LocationBarImageView* view;

- (id)initImageWithView:(LocationBarViewMac::LocationBarImageView*)view;
- (id)initLabelWithView:(LocationBarViewMac::LocationBarImageView*)view;

// Position |view_| right-justified in |frame|.
- (void)positionInFrame:(NSRect)frame;

// Draw image or label of |view_| in |rect_| within |controlView|.
// Only call after |-positionInFrame:| has set |rect_| (or after an
// explicit |-setRect:|).
- (void)drawInView:(NSView*)controlView;

@end

// AutocompleteTextFieldCell extends StyledTextFieldCell to provide support for
// certain decorations to be applied to the field.  These are the search hint
// ("Type to search" on the right-hand side), the keyword hint ("Press [Tab] to
// search Engine" on the right-hand side), and keyword mode ("Search Engine:" in
// a button-like token on the left-hand side).
@interface AutocompleteTextFieldCell : StyledTextFieldCell {
 @private
  // Decorations which live to the left of the text.  Owned by
  // |LocationBarViewMac|.
  std::vector<LocationBarDecoration*> leftDecorations_;

  // Set if there is a string to display as a hint on the right-hand
  // side of the field.  Exclusive WRT |keywordString_|;
  scoped_nsobject<NSAttributedString> hintString_;

  // The star icon sits at the right-hand side of the field when an
  // URL is being shown.
  LocationBarViewMac::LocationBarImageView* starIconView_;

  // List of views showing visible Page Actions. Owned by the location bar.
  // Display is exclusive WRT the |hintString_| and |keywordString_|.
  // This may be NULL during testing.
  LocationBarViewMac::PageActionViewList* page_action_views_;

  // List of content blocked icons. This may be NULL during testing.
  LocationBarViewMac::ContentSettingViews* content_setting_views_;
}

// Chooses |anImage| only if all pieces won't fit w/in |width|.
// Inputs must be non-nil.
- (void)setKeywordHintPrefix:(NSString*)prefixString
                       image:(NSImage*)anImage
                      suffix:(NSString*)suffixString
              availableWidth:(CGFloat)width;

// Suppresses hint entirely if |aString| won't fit w/in |width|.
// String must be non-nil.
- (void)setSearchHintString:(NSString*)aString
             availableWidth:(CGFloat)width;
- (void)clearHint;

// Clear |leftDecorations_|.
- (void)clearDecorations;

// Add a new left-side decoration to the right of the existing
// left-side decorations.
- (void)addLeftDecoration:(LocationBarDecoration*)decoration;

// The width available after accounting for decorations.
- (CGFloat)availableWidthInFrame:(const NSRect)frame;

- (void)setStarIconView:(LocationBarViewMac::LocationBarImageView*)view;
- (void)setPageActionViewList:(LocationBarViewMac::PageActionViewList*)list;
- (void)setContentSettingViewsList:
    (LocationBarViewMac::ContentSettingViews*)views;

// Returns an array of the visible AutocompleteTextFieldIcon objects. Returns
// only visible icons.
- (NSArray*)layedOutIcons:(NSRect)cellFrame;

// Return the rectangle the star is being shown in, for purposes of
// positioning the bookmark bubble.
- (NSRect)starIconFrameForFrame:(NSRect)cellFrame;

// Return the frame for |aDecoration| if the cell is in |cellFrame|.
// Returns |NSZeroRect| for decorations which are not currently
// visible.
- (NSRect)frameForDecoration:(const LocationBarDecoration*)aDecoration
                     inFrame:(NSRect)cellFrame;

// Returns the portion of the cell to use for displaying the Page
// Action icon at the given index. May be NSZeroRect if the index's
// action is not visible.  This does a linear walk over all page
// actions, so do not call this in a loop to get the position of all
// page actions. Use |-layedOutIcons:| instead in that case.
- (NSRect)pageActionFrameForIndex:(size_t)index inFrame:(NSRect)cellFrame;

// Find the icon under the event.  |nil| if |theEvent| is not over
// anything.
- (AutocompleteTextFieldIcon*)iconForEvent:(NSEvent*)theEvent
                                    inRect:(NSRect)cellFrame
                                    ofView:(AutocompleteTextField*)controlView;

// Return the appropriate menu for any page actions under event.
// Returns nil if no menu is present for the action, or if the event
// is not over an action.
- (NSMenu*)actionMenuForEvent:(NSEvent*)theEvent
                       inRect:(NSRect)cellFrame
                       ofView:(AutocompleteTextField*)controlView;

// Called by |AutocompleteTextField| to let page actions intercept
// clicks.  Returns |YES| if the click has been intercepted.
- (BOOL)mouseDown:(NSEvent*)theEvent
           inRect:(NSRect)cellFrame
           ofView:(AutocompleteTextField*)controlView;

@end

// Internal methods here exposed for unit testing.
@interface AutocompleteTextFieldCell (UnitTesting)

@property(nonatomic, readonly) NSAttributedString* hintString;
@property(nonatomic, readonly) NSAttributedString* hintIconLabel;

// Returns the total number of installed Page Actions, visible or not.
- (size_t)pageActionCount;

@end
