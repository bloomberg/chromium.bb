// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_header.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"

TabGroupHeader::TabGroupHeader() {
  // TODO(crbug.com/905491): Call TabStyle::GetContentsInsets.
  constexpr gfx::Insets kPlaceholderInsets = gfx::Insets(2, 10);
  SetBorder(views::CreateEmptyBorder(kPlaceholderInsets));

  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // TODO(crbug.com/905491): Get title from TabGroupData::title().
  auto* title = new views::Label(base::ASCIIToUTF16("Placeholder Title"));
  title->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title->SetElideBehavior(gfx::FADE_TAIL);
  AddChildView(title);
  layout->SetFlexForView(title, views::FlexSpecification::ForSizeRule(
                                    views::MinimumFlexSizeRule::kScaleToZero,
                                    views::MaximumFlexSizeRule::kUnbounded));

  auto* group_menu_button =
      views::CreateVectorImageButton(/*listener*/ nullptr);
  views::SetImageFromVectorIcon(group_menu_button, kBrowserToolsIcon);
  AddChildView(group_menu_button);
}

void TabGroupHeader::OnPaint(gfx::Canvas* canvas) {
  // TODO(crbug.com/905491): Call TabStyle::PaintTab.
  constexpr SkColor kPlaceholderColor = SkColorSetRGB(0xAA, 0xBB, 0xCC);
  canvas->FillRect(GetLocalBounds(), kPlaceholderColor);
}
