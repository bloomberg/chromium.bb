// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_TOUCHABLE_LOCATION_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_TOUCHABLE_LOCATION_BAR_VIEW_H_
#pragma once

#include "ui/views/view.h"

// An interface for a View in within the LocationBar that knows how to
// increase its size (without affecting visual layout) in a touch
// layout, to increase the size of the touch target.  Its border
// extends a few pixels up and down, which doesn't affect layout, and
// extends half of the padding used between items in the location bar
// to the left and right.
//
// To make your location bar View extend itself into the padding
// around it to get an enlarged touch target, inherit from
// TouchableLocationBarView interface, implement its
// GetBuiltInHorizontalPadding method by calling
// GetBuiltInHorizontalPaddingImpl, and call
// TouchableLocationBarView::Init() from your constructor.
class TouchableLocationBarView {
 public:
  // Returns the number of pixels of built-in padding to the left and
  // right of the image for this view.
  virtual int GetBuiltInHorizontalPadding() const = 0;

 protected:
  // Call this from the constructor (or during early initialization)
  // of a class that inherits from TouchableLocationBarView.
  static void Init(views::View* view);

  // Use this to implement GetBuiltInHorizontalPadding().
  static int GetBuiltInHorizontalPaddingImpl();
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_TOUCHABLE_LOCATION_BAR_VIEW_H_
