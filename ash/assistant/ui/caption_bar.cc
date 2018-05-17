// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/caption_bar.h"

#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 24;
constexpr int kPaddingDip = 14;
constexpr int kPreferredHeightDip = 48;
constexpr int kWindowControlSizeDip = 12;

}  // namespace

CaptionBar::CaptionBar() {
  InitLayout();
}

CaptionBar::~CaptionBar() = default;

gfx::Size CaptionBar::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int CaptionBar::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void CaptionBar::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingDip)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Icon.
  views::ImageView* icon_view = new views::ImageView();
  icon_view->SetImage(gfx::CreateVectorIcon(kAssistantIcon, kIconSizeDip));
  icon_view->SetImageSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  icon_view->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  AddChildView(icon_view);

  // Spacer.
  views::View* spacer = new views::View();
  AddChildView(spacer);

  layout_manager->SetFlexForView(spacer, 1);

  // TODO(dmblack): Handle close button.
  // Close.
  views::ImageView* close_view = new views::ImageView();
  close_view->SetImage(gfx::CreateVectorIcon(
      kWindowControlCloseIcon, kWindowControlSizeDip, gfx::kGoogleGrey700));
  close_view->SetImageSize(
      gfx::Size(kWindowControlSizeDip, kWindowControlSizeDip));
  close_view->SetPreferredSize(
      gfx::Size(kWindowControlSizeDip, kWindowControlSizeDip));
  AddChildView(close_view);
}

}  // namespace ash
