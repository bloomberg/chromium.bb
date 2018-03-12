// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_slider_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

UnifiedSliderView::UnifiedSliderView(UnifiedSliderListener* listener,
                                     const gfx::VectorIcon& icon,
                                     int accessible_name_id)
    : slider_(new views::Slider(listener)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, kUnifiedMenuItemPadding,
      kUnifiedTopShortcutSpacing));

  AddChildView(new TopShortcutButton(listener, icon, accessible_name_id));
  AddChildView(slider_);

  slider_->SetBorder(views::CreateEmptyBorder(kUnifiedSliderPadding));
  layout->SetFlexForView(slider_, 1);
}

UnifiedSliderView::~UnifiedSliderView() = default;

}  // namespace ash
