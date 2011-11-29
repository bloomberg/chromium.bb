// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/status_area_bubble.h"

#include "chrome/browser/ui/views/window.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/views/layout/box_layout.h"
#include "views/controls/label.h"

namespace {

// TODO(msw): Get color from theme/window color.
const SkColor kColor = SK_ColorWHITE;

const size_t kCloseBubbleTimerInSec = 5;

}  // namespace

namespace chromeos {

StatusAreaBubbleContentView::StatusAreaBubbleContentView(
    views::View* anchor_view,
    views::View* icon_view,
    const string16& message)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT, kColor),
      icon_view_(icon_view),
      message_view_(new views::Label(message)) {
  // Padding around status.
  const int kPadX = 10, kPadY = 5;
  // Padding between image and text.
  const int kTextPadX = 10;

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                        kPadX, kPadY, kTextPadX));
  AddChildView(icon_view_);
  message_view_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(message_view_);
}

StatusAreaBubbleContentView::~StatusAreaBubbleContentView() {
}

bool StatusAreaBubbleContentView::OnMousePressed(
    const views::MouseEvent& event) {
  // Handle mouse events to close when clicked.
  return true;
}

void StatusAreaBubbleContentView::OnMouseReleased(
    const views::MouseEvent& event) {
  // Close the bubble when clicked.
  GetWidget()->Close();
}

string16 StatusAreaBubbleContentView::GetMessage() const {
  return message_view_->GetText();
}

void StatusAreaBubbleContentView::SetMessage(const string16& message) {
  message_view_->SetText(message);
}

void StatusAreaBubbleContentView::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_STATICTEXT;
  state->state = ui::AccessibilityTypes::STATE_READONLY;
  state->name = GetMessage();
}

StatusAreaBubbleController::StatusAreaBubbleController()
    : bubble_(NULL) {
}

StatusAreaBubbleController::~StatusAreaBubbleController() {
  HideBubble();
}

// static
StatusAreaBubbleController* StatusAreaBubbleController::ShowBubbleForAWhile(
    StatusAreaBubbleContentView* bubble) {
  StatusAreaBubbleController* controller = new StatusAreaBubbleController;
  controller->bubble_ = bubble;
  bubble->set_use_focusless(true);
  browser::CreateViewsBubbleAboveLockScreen(bubble);
  bubble->Show();
  controller->timer_.Start(FROM_HERE,
                           base::TimeDelta::FromSeconds(kCloseBubbleTimerInSec),
                           controller,
                           &StatusAreaBubbleController::HideBubble);

  bubble->GetWidget()->AddObserver(controller);
  return controller;
}

void StatusAreaBubbleController::OnWidgetClosing(views::Widget* widget) {
  bubble_ = NULL;
}

bool StatusAreaBubbleController::IsBubbleShown() const {
  return bubble_;
}

void StatusAreaBubbleController::HideBubble() {
  if (!IsBubbleShown())
    return;
  timer_.Stop();  // no-op if it's not running.
  bubble_->GetWidget()->Close();
}

}  // namespace chromeos
