// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/cocoa/styled_text_field_cell.h"

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/location_bar_view_mac.h"

// AutocompleteTextFieldCell extends StyledTextFieldCell to provide support for
// certain decorations to be applied to the field.  These are the search hint
// ("Type to search" on the right-hand side), the keyword hint ("Press [Tab] to
// search Engine" on the right-hand side), and keyword mode ("Search Engine:" in
// a button-like token on the left-hand side).
@interface AutocompleteTextFieldCell : StyledTextFieldCell {
 @private
  // Set if there is a string to display in a rounded rect on the
  // left-hand side of the field.  Exclusive WRT |hintString_|.
  scoped_nsobject<NSAttributedString> keywordString_;

  // Set if there is a string to display as a hint on the right-hand
  // side of the field.  Exclusive WRT |keywordString_|;
  scoped_nsobject<NSAttributedString> hintString_;

  // View showing the state of the SSL connection. Owned by the location bar.
  // Display is exclusive WRT the |hintString_| and |keywordString_|.
  // This may be NULL during testing.
  LocationBarViewMac::SecurityImageView* security_image_view_;

  // List of views showing visible Page Actions. Owned by the location bar.
  // Display is exclusive WRT the |hintString_| and |keywordString_|.
  // This may be NULL during testing.
  LocationBarViewMac::PageActionViewList* page_action_views_;
}

// Chooses |partialString| if |width| won't fit |fullString|.  Strings
// must be non-nil.
- (void)setKeywordString:(NSString*)fullString
           partialString:(NSString*)partialString
          availableWidth:(CGFloat)width;

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
- (void)clearKeywordAndHint;

- (void)setSecurityImageView:(LocationBarViewMac::SecurityImageView*)view;
- (void)setPageActionViewList:(LocationBarViewMac::PageActionViewList*)list;

// Returns the total number of installed Page Actions, visible or not.
- (size_t)pageActionCount;

// Called when the security icon is visible and clicked. Passed through to the
// security_image_view_ to handle the click (i.e., show the page info dialog).
- (void)onSecurityIconMousePressed;

// Returns the portion of the cell to use for displaying the security (SSL lock)
// icon, leaving space for its label if any.
- (NSRect)securityImageFrameForFrame:(NSRect)cellFrame;

// Returns the portion of the cell to use for displaying the Page Action icon
// at the given index. May be NSZeroRect if the index's action is not visible.
- (NSRect)pageActionFrameForIndex:(size_t)index inFrame:(NSRect)cellFrame;

// Called when the Page Action at the given index, whose icon is drawn in the
// iconFrame, is visible and clicked. Passed through to the list of views to
// handle the click.
- (void)onPageActionMousePressedIn:(NSRect)iconFrame forIndex:(size_t)index;

@end

// Internal methods here exposed for unit testing.
@interface AutocompleteTextFieldCell (UnitTesting)

@property(readonly) NSAttributedString* keywordString;
@property(readonly) NSAttributedString* hintString;
@property(readonly) NSAttributedString* hintIconLabel;

@end
