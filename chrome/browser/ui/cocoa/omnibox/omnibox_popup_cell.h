// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_CELL_H_
#define CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_CELL_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"

// Draws a single row in the omnibox popup.
@interface OmniboxPopupCell : NSButtonCell {
  base::scoped_nsobject<NSAttributedString> contentText_;
  base::scoped_nsobject<NSAttributedString> descriptionText_;
}

- (void)setContentText:(NSAttributedString*)contentText;
- (void)setDescriptionText:(NSAttributedString*)descriptionText;

@end

#endif  // CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_CELL_H_
