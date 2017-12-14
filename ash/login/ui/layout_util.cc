// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/layout_util.h"

#include "ash/login/ui/non_accessible_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace login_layout_util {

views::View* WrapViewForPreferredSize(views::View* view) {
  auto* proxy = new NonAccessibleView();
  auto layout_manager =
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical);
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  proxy->SetLayoutManager(std::move(layout_manager));
  proxy->AddChildView(view);
  return proxy;
}

}  // namespace login_layout_util
}  // namespace ash
