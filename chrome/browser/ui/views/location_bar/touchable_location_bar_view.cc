// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/touchable_location_bar_view.h"

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/base/layout.h"
#include "ui/views/border.h"

// static
void TouchableLocationBarView::Init(views::View* view) {
  int horizontal_padding = GetBuiltInHorizontalPaddingImpl();
  if (horizontal_padding != 0) {
    view->set_border(views::Border::CreateEmptyBorder(
        3, horizontal_padding, 3, horizontal_padding));
  }
}

// static
int TouchableLocationBarView::GetBuiltInHorizontalPaddingImpl() {
  return ui::GetDisplayLayout() == ui::LAYOUT_TOUCH ?
      LocationBarView::GetItemPadding() / 2 : 0;
}
