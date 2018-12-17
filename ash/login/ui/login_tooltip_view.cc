// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_tooltip_view.h"

#include "ash/login/ui/views_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

LoginTooltipView::LoginTooltipView(const base::string16& message,
                                   views::View* anchor_view)
    : LoginBaseBubbleView(anchor_view) {
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  SetText(message);
}

LoginTooltipView::~LoginTooltipView() = default;

void LoginTooltipView::SetText(const base::string16& message) {
  views::Label* text =
      login_views_utils::CreateBubbleLabel(message, SK_ColorWHITE);
  text->SetMultiLine(true);
  RemoveAllChildViews(true /*delete_children*/);
  AddChildView(text);
}

void LoginTooltipView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTooltip;
}

}  // namespace ash
