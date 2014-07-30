// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_CELL_H_
#define CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_CELL_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "components/autocomplete/autocomplete_match.h"

class OmniboxPopupViewMac;

// OmniboxPopupCell overrides how backgrounds are displayed to
// handle hover versus selected.  So long as we're in there, it also
// provides some default initialization.
@interface OmniboxPopupCell : NSButtonCell {
 @private
  // The popup view parent of this cell.
  OmniboxPopupViewMac* parent_;

  // The match which will be rendered for this row in omnibox dropdown.
  AutocompleteMatch match_;

  // NSAttributedString instances for various match components.
  base::scoped_nsobject<NSAttributedString> separator_;
  base::scoped_nsobject<NSAttributedString> description_;

  // NOTE: While |prefix_| is used only for postfix suggestions, it still needs
  // to be a member of the class. This allows the |NSAttributedString| instance
  // to stay alive between the call to |drawTitle| and the actual paint event
  // which accesses the |NSAttributedString| instance.
  base::scoped_nsobject<NSAttributedString> prefix_;

  // The width of widest match contents in a set of infinite suggestions.
  CGFloat maxMatchContentsWidth_;

  // The offset at which the infinite suggesiton contents should be displayed.
  CGFloat contentsOffset_;
}

- (void)setMatch:(const AutocompleteMatch&)match;

- (void)setMaxMatchContentsWidth:(CGFloat)maxMatchContentsWidth;

- (void)setContentsOffset:(CGFloat)contentsOffset;

// Returns the width of the match contents.
- (CGFloat)getMatchContentsWidth;

// Returns the offset of the start of the contents in the input text for the
// given match. It is costly to compute this offset, so it is computed once and
// shared by all OmniboxPopupCell instances through OmniboxPopupViewMac parent.
+ (CGFloat)computeContentsOffset:(const AutocompleteMatch&)match;

@end

#endif  // CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_CELL_H_
