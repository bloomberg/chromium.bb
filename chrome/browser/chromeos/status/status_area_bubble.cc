// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/status_area_bubble.h"

#include "chrome/browser/ui/views/window.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {

const size_t kCloseBubbleTimerInSec = 5;

}  // namespace

namespace chromeos {

StatusAreaBubbleContentView::StatusAreaBubbleContentView(
    views::View* icon_view,
    const string16& message)
    : icon_view_(icon_view),
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


// A BubbleDelegateView implementation to be used by StatusAreaBubbleController.
// This class is also responsible for handling events while
// StatusAreaContentView is only responsible for showing the content.
class StatusAreaBubbleController::StatusAreaBubbleDelegateView
    : public views::BubbleDelegateView {
 public:
  explicit StatusAreaBubbleDelegateView(views::View* anchor_view)
      : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT) {
  }

 protected:
  // views::View override
  bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    // Handle mouse events to close when clicked.
    return true;
  }
  // views::View override
  void OnMouseReleased(const views::MouseEvent& event) OVERRIDE {
    GetWidget()->Close();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StatusAreaBubbleDelegateView);
};


StatusAreaBubbleController::StatusAreaBubbleController()
    : bubble_(NULL) {
}

StatusAreaBubbleController::~StatusAreaBubbleController() {
  HideBubble();
}

// static
StatusAreaBubbleController* StatusAreaBubbleController::
ShowBubbleUnderViewForAWhile(views::View* view,
                             StatusAreaBubbleContentView* content) {
  StatusAreaBubbleController* controller = new StatusAreaBubbleController;
  StatusAreaBubbleDelegateView* bubble = new StatusAreaBubbleDelegateView(view);
  bubble->SetLayoutManager(new views::FillLayout);
  bubble->AddChildView(content);
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
  // There are two ways to close the bubble.
  // 1. Call Widget::Close. (This way is used by the bubble implementation.)
  // 2. Call HideBubble. (This way is used by code outside the bubble impl.)
  // OnWidgetClosing is only called in the case of 1.
  bubble_ = NULL;
}

bool StatusAreaBubbleController::IsBubbleShown() const {
  return bubble_;
}

void StatusAreaBubbleController::HideBubble() {
  if (!IsBubbleShown())
    return;
  timer_.Stop();  // no-op if it's not running.

  // Instead of setting |bubble_| NULL in OnWidgetClosing, do it here because
  // |this| may be deleted when OnWidgetClosing is called.
  bubble_->GetWidget()->RemoveObserver(this);
  bubble_->GetWidget()->Close();
  bubble_ = NULL;
}

}  // namespace chromeos
