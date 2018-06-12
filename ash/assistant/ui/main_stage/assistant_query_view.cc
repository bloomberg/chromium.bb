// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_query_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 24;

}  // namespace

AssistantQueryView::AssistantQueryView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      label_(new views::StyledLabel(base::string16(), /*listener=*/nullptr)) {
  InitLayout();
  OnQueryChanged(assistant_controller_->interaction_model()->query());

  // The Assistant controller indirectly owns the view hierarchy to which
  // AssistantQueryView belongs so is guaranteed to outlive it.
  assistant_controller_->AddInteractionModelObserver(this);
}

AssistantQueryView::~AssistantQueryView() {
  assistant_controller_->RemoveInteractionModelObserver(this);
}

gfx::Size AssistantQueryView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

void AssistantQueryView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantQueryView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  label_->SizeToFit(width());
}

void AssistantQueryView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_START);

  // Icon.
  views::ImageView* icon = new views::ImageView();
  icon->SetImage(gfx::CreateVectorIcon(kAssistantIcon, kIconSizeDip));
  icon->SetImageSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  icon->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  AddChildView(icon);

  // Label.
  label_->set_auto_color_readability_enabled(false);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  AddChildView(label_);
}

void AssistantQueryView::OnQueryChanged(const AssistantQuery& query) {
  // Empty query.
  if (query.Empty()) {
    OnQueryCleared();
  } else {
    // Populated query.
    switch (query.type()) {
      case AssistantQueryType::kText: {
        const AssistantTextQuery& text_query =
            static_cast<const AssistantTextQuery&>(query);
        SetText(text_query.text());
        break;
      }
      case AssistantQueryType::kVoice: {
        const AssistantVoiceQuery& voice_query =
            static_cast<const AssistantVoiceQuery&>(query);
        SetText(voice_query.high_confidence_speech(),
                voice_query.low_confidence_speech());
        break;
      }
      case AssistantQueryType::kEmpty:
        // Empty queries are already handled.
        NOTREACHED();
        break;
    }
  }
}

void AssistantQueryView::OnQueryCleared() {
  SetText(l10n_util::GetStringUTF8(IDS_ASH_ASSISTANT_PROMPT_DEFAULT));
}

void AssistantQueryView::SetText(const std::string& high_confidence_text,
                                 const std::string& low_confidence_text) {
  const base::string16& high_confidence_text_16 =
      base::UTF8ToUTF16(high_confidence_text);

  if (low_confidence_text.empty()) {
    label_->SetText(high_confidence_text_16);
    label_->AddStyleRange(gfx::Range(0, high_confidence_text_16.length()),
                          CreateStyleInfo(kTextColorPrimary));
  } else {
    const base::string16& low_confidence_text_16 =
        base::UTF8ToUTF16(low_confidence_text);

    label_->SetText(high_confidence_text_16 + low_confidence_text_16);

    // High confidence text styling.
    label_->AddStyleRange(gfx::Range(0, high_confidence_text_16.length()),
                          CreateStyleInfo(kTextColorPrimary));

    // Low confidence text styling.
    label_->AddStyleRange(gfx::Range(high_confidence_text_16.length(),
                                     high_confidence_text_16.length() +
                                         low_confidence_text_16.length()),
                          CreateStyleInfo(kTextColorHint));
  }

  label_->SizeToFit(width());
  PreferredSizeChanged();
}

views::StyledLabel::RangeStyleInfo AssistantQueryView::CreateStyleInfo(
    SkColor color) const {
  views::StyledLabel::RangeStyleInfo style;
  style.custom_font =
      label_->GetDefaultFontList().DeriveWithSizeDelta(8).DeriveWithWeight(
          gfx::Font::Weight::MEDIUM);
  style.override_color = color;
  return style;
}

}  // namespace ash
