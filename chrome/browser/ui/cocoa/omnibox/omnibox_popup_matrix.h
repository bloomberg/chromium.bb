// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_MATRIX_H_
#define CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_MATRIX_H_

#import <Cocoa/Cocoa.h>

class OmniboxPopupViewMac;

@interface OmniboxPopupMatrix : NSMatrix {
 @private
  // Target for click and middle-click.
  OmniboxPopupViewMac* popupView_;  // weak, owns us.
}

// Create a zero-size matrix initializing |popupView_|.
- (id)initWithPopupView:(OmniboxPopupViewMac*)popupView;

// Set |popupView_|.
- (void)setPopupView:(OmniboxPopupViewMac*)popupView;

// Return the currently highlighted row.  Returns -1 if no row is
// highlighted.
- (NSInteger)highlightedRow;

@end

#endif  // CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_MATRIX_H_
