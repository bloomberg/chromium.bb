// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// The amount of time the promo should stay onscreen if the user
// never hovers over it.
constexpr base::TimeDelta kDelayDefault = base::TimeDelta::FromSeconds(5);

// The amount of time the promo should stay onscreen after the
// user stops hovering over it.
constexpr base::TimeDelta kDelayShort = base::TimeDelta::FromSeconds(1);

}  // namespace

FeaturePromoBubbleView::FeaturePromoBubbleView(
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    int string_specifier,
    ActivationAction activation_action)
    : FeaturePromoBubbleView(anchor_view,
                             gfx::Rect(),
                             arrow,
                             string_specifier,
                             activation_action) {}

FeaturePromoBubbleView::FeaturePromoBubbleView(const gfx::Rect& anchor_rect,
                                               views::BubbleBorder::Arrow arrow,
                                               int string_specifier)
    : FeaturePromoBubbleView(nullptr,
                             anchor_rect,
                             arrow,
                             string_specifier,
                             ActivationAction::ACTIVATE) {}

FeaturePromoBubbleView::FeaturePromoBubbleView(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    views::BubbleBorder::Arrow arrow,
    int string_specifier,
    ActivationAction activation_action)
    : BubbleDialogDelegateView(anchor_view, arrow) {
  UseCompactMargins();
  if (!anchor_view)
    SetAnchorRect(anchor_rect);

  auto box_layout = base::MakeUnique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(), 0);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(box_layout.release());

  AddChildView(new views::Label(l10n_util::GetStringUTF16(string_specifier)));

  if (activation_action == ActivationAction::DO_NOT_ACTIVATE)
    set_can_activate(activation_action == ActivationAction::ACTIVATE);
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);
  if (activation_action == ActivationAction::DO_NOT_ACTIVATE)
    SetArrowPaintType(views::BubbleBorder::PAINT_TRANSPARENT);
  UseCompactMargins();
  widget->Show();
  if (activation_action == ActivationAction::ACTIVATE)
    StartAutoCloseTimer(kDelayDefault);
}

FeaturePromoBubbleView::~FeaturePromoBubbleView() = default;

void FeaturePromoBubbleView::CloseBubble() {
  GetWidget()->Close();
}

int FeaturePromoBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool FeaturePromoBubbleView::OnMousePressed(const ui::MouseEvent& event) {
  CloseBubble();
  return true;
}

void FeaturePromoBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  timer_.Stop();
}

void FeaturePromoBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  StartAutoCloseTimer(kDelayShort);
}

void FeaturePromoBubbleView::StartAutoCloseTimer(
    base::TimeDelta auto_close_duration) {
  timer_.Start(FROM_HERE, auto_close_duration, this,
               &FeaturePromoBubbleView::CloseBubble);
}
