// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"

// AutocompleteTextFieldCell customizes the look of the Omnibox text
// field.  The border and focus ring are modified, as is the font
// baseline.

// The cell also provides support for certain decorations to be
// applied to the field.  These are the search hint ("Type to search"
// on the right-hand side), the keyword hint ("Press [Tab] to search
// Engine" on the right-hand side), and keyword mode ("Search Engine:"
// in a button-like token on the left-hand side).
//
// The cell arranges the field-editor's placement via the standard
// -editWithFrame:* and -selectWithFrame:* methods.  When the visible
// decoration changes, the cell's look may change, and if the cell is
// currently being edited the field editor will require adjustment.
// The cell signals this requirement by returning YES for
// -fieldEditorNeedsReset, which is used by AutocompleteTextField's
// -resetFieldEditorFrameIfNeeded in testing if re-layout is needed.

@interface AutocompleteTextFieldCell : NSTextFieldCell {
 @private
  // Set if there is a string to display in a rounded rect on the
  // left-hand side of the field.  Exclusive WRT |hintString_|.
  scoped_nsobject<NSAttributedString> keywordString_;

  // Set if there is a string to display as a hint on the right-hand
  // side of the field.  Exclusive WRT |keywordString_|;
  scoped_nsobject<NSAttributedString> hintString_;

  // YES if the info cell has been changed in a way which would result
  // in the cell needing to be laid out again.
  BOOL fieldEditorNeedsReset_;
}

@property BOOL fieldEditorNeedsReset;

// TODO(shess): There should be two alternatives for
// -setKeywordString:, the normal string and the min string.  Min can
// be used when the text field's contents gets too wide to fit both it
// and this.

// The following setup |keywordString_| or |hintString_| based on the
// input, and set |fieldEditorNeedsReset_| if the layout of the field
// changed.
- (void)setKeywordString:(NSString*)aString;
- (void)setKeywordHintPrefix:(NSString*)prefixString
                       image:(NSImage*)anImage
                      suffix:(NSString*)suffixString;
- (void)setSearchHintString:(NSString*)aString;
- (void)clearKeywordAndHint;

@end

// Internal methods here exposed for unit testing.
@interface AutocompleteTextFieldCell (UnitTesting)

@property(readonly) NSAttributedString* keywordString;
@property(readonly) NSAttributedString* hintString;

// Return the portion of the cell to use for text display.
- (NSRect)textFrameForFrame:(NSRect)cellFrame;

@end
