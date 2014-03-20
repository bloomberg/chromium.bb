// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_CELL_H_
#define CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_CELL_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"

// OmniboxPopupCell overrides how backgrounds are displayed to
// handle hover versus selected.  So long as we're in there, it also
// provides some default initialization.
@interface OmniboxPopupCell : NSButtonCell {
 @private
  // The match which will be rendered for this row in omnibox dropdown.
  AutocompleteMatch match_;

  base::scoped_nsobject<NSAttributedString> separator_;
  base::scoped_nsobject<NSAttributedString> description_;
}

- (void)setMatch:(const AutocompleteMatch&)match;

@end

#endif  // CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_CELL_H_
