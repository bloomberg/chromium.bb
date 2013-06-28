// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_MATRIX_H_
#define CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_MATRIX_H_

#import <Cocoa/Cocoa.h>

#import "ui/base/cocoa/tracking_area.h"
#include "ui/base/window_open_disposition.h"

@class OmniboxPopupMatrix;

class OmniboxPopupMatrixDelegate {
 public:
  // Called when the selection in the matrix changes.
  virtual void OnMatrixRowSelected(OmniboxPopupMatrix* matrix, size_t row) = 0;

  // Called when the user clicks on a row.
  virtual void OnMatrixRowClicked(OmniboxPopupMatrix* matrix, size_t row) = 0;

  // Called when the user middle clicks on a row.
  virtual void OnMatrixRowMiddleClicked(OmniboxPopupMatrix* matrix,
                                        size_t row) = 0;
};

// Sets up a tracking area to implement hover by highlighting the cell the mouse
// is over.
@interface OmniboxPopupMatrix : NSMatrix {
  OmniboxPopupMatrixDelegate* delegate_;  // weak
  ui::ScopedCrTrackingArea trackingArea_;
}

// Create a zero-size matrix.
- (id)initWithDelegate:(OmniboxPopupMatrixDelegate*)delegate;

// Sets the delegate.
- (void)setDelegate:(OmniboxPopupMatrixDelegate*)delegate;

// Return the currently highlighted row.  Returns -1 if no row is highlighted.
- (NSInteger)highlightedRow;

@end

#endif  // CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_POPUP_MATRIX_H_
