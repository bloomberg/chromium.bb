// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/base/assistant_button.h"

#include "ash/assistant/util/histogram_util.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"

namespace ash {

namespace {

// Appearance.
constexpr float kInkDropHighlightOpacity = 0.08f;
constexpr int kInkDropInset = 2;

}  // namespace

AssistantButton::AssistantButton(views::ButtonListener* listener,
                                 AssistantButtonId button_id)
    : views::ImageButton(this), listener_(listener) {
  constexpr SkColor kInkDropBaseColor = SK_ColorBLACK;
  constexpr float kInkDropVisibleOpacity = 0.06f;

  // Focus.
  SetFocusForPlatform();

  // Image.
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);

  // Ink drop.
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  set_ink_drop_base_color(kInkDropBaseColor);
  set_ink_drop_visible_opacity(kInkDropVisibleOpacity);

  set_id(static_cast<int>(button_id));
}

AssistantButton::~AssistantButton() = default;

const char* AssistantButton::GetClassName() const {
  return "AssistantButton";
}

void AssistantButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Note that the current assumption is that button bounds are square.
  DCHECK_EQ(width(), height());
  SetFocusPainter(views::Painter::CreateSolidRoundRectPainter(
      SkColorSetA(GetInkDropBaseColor(), 0xff * kInkDropHighlightOpacity),
      width() / 2 - kInkDropInset, gfx::Insets(kInkDropInset)));
}

std::unique_ptr<views::InkDrop> AssistantButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      std::make_unique<views::InkDropImpl>(this, size());
  ink_drop->SetAutoHighlightMode(
      views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
  ink_drop->SetShowHighlightOnHover(true);
  return ink_drop;
}

std::unique_ptr<views::InkDropHighlight>
AssistantButton::CreateInkDropHighlight() const {
  return std::make_unique<views::InkDropHighlight>(
      gfx::PointF(GetLocalBounds().CenterPoint()),
      std::make_unique<views::CircleLayerDelegate>(
          SkColorSetA(GetInkDropBaseColor(), 0xff * kInkDropHighlightOpacity),
          size().width() / 2 - kInkDropInset));
}

std::unique_ptr<views::InkDropMask> AssistantButton::CreateInkDropMask() const {
  return std::make_unique<views::RoundRectInkDropMask>(
      size(), gfx::Insets(kInkDropInset), size().width() / 2);
}

std::unique_ptr<views::InkDropRipple> AssistantButton::CreateInkDropRipple()
    const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), gfx::Insets(kInkDropInset), GetInkDropCenterBasedOnLastEvent(),
      GetInkDropBaseColor(), ink_drop_visible_opacity());
}

void AssistantButton::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  assistant::util::IncrementAssistantButtonClickCount(
      static_cast<AssistantButtonId>(sender->id()));
  listener_->ButtonPressed(sender, event);
}

}  // namespace ash
