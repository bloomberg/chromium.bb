// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"

#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

// Appearance.
constexpr int kIconSizeDip = 32;
constexpr SkColor kLabelContainerBackgroundColor = SK_ColorWHITE;
constexpr int kLabelContainerBackgroundCornerRadiusDip = 16;
constexpr SkColor kLabelContainerBackgroundStrokeColor =
    SkColorSetA(gfx::kGoogleGrey900, 0x24);
constexpr int kLabelContainerBackgroundStrokeWidthDip = 1;
constexpr int kLabelContainerPaddingHorizontalDip = 16;
constexpr int kLabelContainerPaddingVerticalDip = 6;

namespace {

// LabelContainerBackground ----------------------------------------------------

class LabelContainerBackground : public views::Background {
 public:
  LabelContainerBackground() = default;

  ~LabelContainerBackground() override = default;

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    gfx::Rect bounds = view->GetContentsBounds();

    // Background.
    flags.setColor(kLabelContainerBackgroundColor);
    canvas->DrawRoundRect(bounds, kLabelContainerBackgroundCornerRadiusDip,
                          flags);

    // Stroke should be drawn within our contents bounds.
    bounds.Inset(gfx::Insets(kLabelContainerBackgroundStrokeWidthDip));

    // Stroke.
    flags.setColor(kLabelContainerBackgroundStrokeColor);
    flags.setStrokeWidth(kLabelContainerBackgroundStrokeWidthDip);
    flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
    canvas->DrawRoundRect(bounds, kLabelContainerBackgroundCornerRadiusDip,
                          flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LabelContainerBackground);
};

}  // namespace

// AssistantTextElementView ----------------------------------------------------

AssistantTextElementView::AssistantTextElementView(
    const AssistantTextElement* text_element) {
  InitLayout(text_element);
}

AssistantTextElementView::~AssistantTextElementView() = default;

void AssistantTextElementView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantTextElementView::InitLayout(
    const AssistantTextElement* text_element) {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          2 * kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_START);

  // Icon.
  views::ImageView* icon = new views::ImageView();
  icon->SetImage(gfx::CreateVectorIcon(kAssistantIcon, kIconSizeDip));
  icon->SetImageSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  icon->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  AddChildView(icon);

  // Label Container.
  views::View* label_container = new views::View();
  label_container->SetBackground(std::make_unique<LabelContainerBackground>());
  label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kLabelContainerPaddingVerticalDip,
                  kLabelContainerPaddingHorizontalDip)));
  AddChildView(label_container);

  // Label.
  views::Label* label =
      new views::Label(base::UTF8ToUTF16(text_element->text()));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(kTextColorPrimary);
  label->SetFontList(views::Label::GetDefaultFontList().DeriveWithSizeDelta(4));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetMultiLine(true);
  label_container->AddChildView(label);
}

}  // namespace ash
