// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_FAV_ICON_H_
#define CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_FAV_ICON_H_
#pragma once

#include "views/widget/widget_gtk.h"

class SkBitmap;

namespace views {
class ImageView;
}

namespace chromeos {

class WmOverviewSnapshot;

// A single favicon displayed by WmOverviewController.
class WmOverviewFavIcon : public views::WidgetGtk {
 public:
  static const int kIconSize;

  WmOverviewFavIcon();

  // Initializes the favicon to 0x0 size.
  void Init(WmOverviewSnapshot* snapshot);

  // Setting the favicon sets the bounds to the size of the given
  // image.
  void SetFavIcon(const SkBitmap& image);

 private:
  // This control is the contents view for this widget.
  views::ImageView* fav_icon_view_;

  DISALLOW_COPY_AND_ASSIGN(WmOverviewFavIcon);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WM_OVERVIEW_FAV_ICON_H_
