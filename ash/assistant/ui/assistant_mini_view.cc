// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_mini_view.h"

#include <memory>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 20;
constexpr int kMaxWidthDip = 512;
constexpr int kPreferredHeightDip = 48;

// TODO(b/77638210): Replace with localized resource strings.
constexpr char kPrompt[] = "Draw with your stylus to select";

}  // namespace

AssistantMiniView::AssistantMiniView() {
  InitLayout();
}

AssistantMiniView::~AssistantMiniView() = default;

gfx::Size AssistantMiniView::CalculatePreferredSize() const {
  const int preferred_width =
      std::min(views::View::CalculatePreferredSize().width(), kMaxWidthDip);
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int AssistantMiniView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void AssistantMiniView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMiniView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingDip), 2 * kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Icon.
  views::ImageView* icon = new views::ImageView();
  icon->SetImage(gfx::CreateVectorIcon(kAssistantIcon, kIconSizeDip));
  icon->SetImageSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  icon->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  AddChildView(icon);

  // Label.
  views::Label* label = new views::Label(base::UTF8ToUTF16(kPrompt));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetFontList(views::Label::GetDefaultFontList().DeriveWithSizeDelta(4));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  AddChildView(label);
}

}  // namespace ash
