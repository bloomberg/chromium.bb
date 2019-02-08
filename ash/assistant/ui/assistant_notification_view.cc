// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_notification_view.h"

#include <string>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/main_stage/suggestion_chip_view.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kLineHeightDip = 20;
constexpr int kPaddingLeftDip = 16;
constexpr int kPaddingRightDip = 8;
constexpr int kPreferredHeightDip = 48;
constexpr int kShadowElevationDip = 6;

// Helpers ---------------------------------------------------------------------

views::View* CreateButton(const std::string& label) {
  SuggestionChipView::Params params;
  params.text = base::UTF8ToUTF16(label);
  return new SuggestionChipView(params, /*listener=*/nullptr);
}

}  // namespace

AssistantNotificationView::AssistantNotificationView() {
  InitLayout();
}

AssistantNotificationView::~AssistantNotificationView() = default;

const char* AssistantNotificationView::GetClassName() const {
  return "AssistantNotificationView";
}

gfx::Size AssistantNotificationView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int AssistantNotificationView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void AssistantNotificationView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  UpdateBackground();
}

void AssistantNotificationView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Background/shadow.
  background_layer_.SetFillsBoundsOpaquely(false);
  layer()->Add(&background_layer_);

  // Container.
  views::View* container = new views::View();
  container->SetPreferredSize(gfx::Size(INT_MAX, INT_MAX));
  container->SetPaintToLayer();
  container->layer()->SetFillsBoundsOpaquely(false);
  AddChildView(container);

  auto* layout_manager =
      container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingLeftDip, 0, kPaddingRightDip), kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  gfx::FontList font_list =
      assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(1);

  // Title.
  auto* title = new views::Label(base::UTF8ToUTF16("Placeholder Title"));
  title->SetAutoColorReadabilityEnabled(false);
  title->SetEnabledColor(kTextColorPrimary);
  title->SetFontList(font_list.DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title->SetLineHeight(kLineHeightDip);
  container->AddChildView(title);

  // Message.
  auto* message = new views::Label(base::UTF8ToUTF16("Placeholder Message"));
  message->SetAutoColorReadabilityEnabled(false);
  message->SetEnabledColor(kTextColorSecondary);
  message->SetFontList(font_list);
  message->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  message->SetLineHeight(kLineHeightDip);
  container->AddChildView(message);

  layout_manager->SetFlexForView(message, 1);

  // Buttons.
  container->AddChildView(CreateButton("Placeholder Button 1"));
  container->AddChildView(CreateButton("Placeholder Button 2"));
}

void AssistantNotificationView::UpdateBackground() {
  gfx::ShadowValues shadow_values =
      gfx::ShadowValue::MakeMdShadowValues(kShadowElevationDip);

  shadow_delegate_ = std::make_unique<views::BorderShadowLayerDelegate>(
      shadow_values, GetLocalBounds(),
      /*fill_color=*/SK_ColorWHITE,
      /*corner_radius=*/height() / 2);

  background_layer_.set_delegate(shadow_delegate_.get());
  background_layer_.SetBounds(
      gfx::ToEnclosingRect(shadow_delegate_->GetPaintedBounds()));
}

}  // namespace ash
