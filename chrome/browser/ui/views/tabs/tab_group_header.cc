// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_header.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"

TabGroupHeader::TabGroupHeader(TabController* controller, int group)
    : controller_(controller), group_(group) {
  DCHECK(controller);

  // TODO(crbug.com/905491): Call TabStyle::GetContentsInsets.
  constexpr gfx::Insets kPlaceholderInsets = gfx::Insets(4, 27);
  SetBorder(views::CreateEmptyBorder(kPlaceholderInsets));

  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  auto title = std::make_unique<views::Label>(GetGroupData()->title());
  title->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title->SetElideBehavior(gfx::FADE_TAIL);
  title_label_ = AddChildView(std::move(title));
  layout->SetFlexForView(title_label_,
                         views::FlexSpecification::ForSizeRule(
                             views::MinimumFlexSizeRule::kScaleToZero,
                             views::MaximumFlexSizeRule::kUnbounded));
}

void TabGroupHeader::OnPaint(gfx::Canvas* canvas) {
  // TODO(crbug.com/905491): Call TabStyle::PaintTab.
  gfx::Rect fill_bounds(GetLocalBounds());
  fill_bounds.Inset(TabStyle::GetTabOverlap(), 0);
  const SkColor color = GetGroupData()->color();
  canvas->FillRect(fill_bounds, color);
  title_label_->SetBackgroundColor(color);
}

const TabGroupData* TabGroupHeader::GetGroupData() {
  return controller_->GetDataForGroup(group_);
}
