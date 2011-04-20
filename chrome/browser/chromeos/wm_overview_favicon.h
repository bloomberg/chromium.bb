// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_FAVICON_H_
#define CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_FAVICON_H_
#pragma once

#include "base/basictypes.h"

class SkBitmap;

namespace views {
class ImageView;
class Widget;
}

namespace chromeos {

class WmOverviewSnapshot;

// A single favicon displayed by WmOverviewController.
class WmOverviewFavicon {
 public:
  static const int kIconSize;

  WmOverviewFavicon();
  ~WmOverviewFavicon();

  // Initializes the favicon to 0x0 size.
  void Init(WmOverviewSnapshot* snapshot);

  // Setting the favicon sets the bounds to the size of the given
  // image.
  void SetFavicon(const SkBitmap& image);

  views::Widget* widget() { return widget_; }

 private:
  // This control is the contents view for this widget.
  views::ImageView* favicon_view_;

  // Not owned, deletes itself when the underlying widget is destroyed.
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(WmOverviewFavicon);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_FAVICON_H_
