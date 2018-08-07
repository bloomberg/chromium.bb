// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_opt_in_view.h"

#include <memory>
#include <vector>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPreferredHeightDip = 32;

// Helpers ---------------------------------------------------------------------

views::StyledLabel::RangeStyleInfo CreateStyleInfo(
    gfx::Font::Weight weight = gfx::Font::Weight::NORMAL) {
  views::StyledLabel::RangeStyleInfo style;
  style.custom_font = assistant::ui::GetDefaultFontList()
                          .DeriveWithSizeDelta(2)
                          .DeriveWithWeight(weight);
  style.override_color = SK_ColorWHITE;
  return style;
}

// AssistantOptInContainer -----------------------------------------------------

class AssistantOptInContainer : public views::View {
 public:
  AssistantOptInContainer() = default;
  ~AssistantOptInContainer() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    const int preferred_width = views::View::CalculatePreferredSize().width();
    return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
  }

  int GetHeightForWidth(int width) const override {
    return kPreferredHeightDip;
  }

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(gfx::kGoogleBlue500);
    canvas->DrawRoundRect(GetContentsBounds(), height() / 2, flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantOptInContainer);
};

}  // namespace

// AssistantOptInView ----------------------------------------------------------

AssistantOptInView::AssistantOptInView() {
  InitLayout();
}

AssistantOptInView::~AssistantOptInView() = default;

void AssistantOptInView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantOptInView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  label_->SizeToFit(width());
}

void AssistantOptInView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_END);

  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::MAIN_AXIS_ALIGNMENT_CENTER);

  // Container.
  AssistantOptInContainer* container = new AssistantOptInContainer();

  layout_manager =
      container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingDip)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  AddChildView(container);

  // Label.
  label_ = new views::StyledLabel(base::string16(), /*listener=*/nullptr);
  label_->set_auto_color_readability_enabled(false);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);

  // First substitution string: "Unlock more Assistant features."
  const base::string16 unlock_features =
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_OPT_IN_UNLOCK_MORE_FEATURES);

  // Second substitution string: "Get Started".
  const base::string16 get_started =
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_OPT_IN_GET_STARTED);

  // Set the text, having replaced placeholders in the opt in prompt with
  // substitution strings and caching their offset positions for styling.
  std::vector<size_t> offsets;
  label_->SetText(l10n_util::GetStringFUTF16(
      IDS_ASH_ASSISTANT_OPT_IN_PROMPT, unlock_features, get_started, &offsets));

  // Style the first substitution string.
  label_->AddStyleRange(
      gfx::Range(offsets.at(0), offsets.at(0) + unlock_features.length()),
      CreateStyleInfo());

  // Style the second substitution string.
  label_->AddStyleRange(
      gfx::Range(offsets.at(1), offsets.at(1) + get_started.length()),
      CreateStyleInfo(gfx::Font::Weight::BOLD));

  container->AddChildView(label_);
}

}  // namespace ash
