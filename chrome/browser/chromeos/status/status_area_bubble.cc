// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/status_area_bubble.h"

#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "views/controls/label.h"

namespace chromeos {

namespace {

const size_t kCloseBubbleTimerInSec = 5;

class CloseBubbleWhenClickedView : public views::View {
 public:
  explicit CloseBubbleWhenClickedView(StatusAreaBubbleController* controller)
      : controller_(controller) {
  }

 protected:
  // views::View implementation
  bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    return true;
  }
  void OnMouseReleased(const views::MouseEvent& event) OVERRIDE {
    controller_->HideBubble();
  }

 private:
  StatusAreaBubbleController* controller_;

  DISALLOW_COPY_AND_ASSIGN(CloseBubbleWhenClickedView);
};

}  // namespace

StatusAreaBubbleContentView::StatusAreaBubbleContentView(
    views::View* icon_view, const string16& message)
    : icon_view_(icon_view),
      message_view_(new views::Label(message)) {
  // Padding around status.
  const int kPadX = 10, kPadY = 5;
  // Padding between image and text.
  const int kTextPadX = 10;

  // Set layout.
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                        kPadX, kPadY, kTextPadX));
  // Add icon view
  AddChildView(icon_view);
  // Add message view
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


StatusAreaBubbleController::StatusAreaBubbleController()
    : bubble_(NULL), content_(NULL) {
}

StatusAreaBubbleController::~StatusAreaBubbleController() {
  HideBubble();
}

// static
StatusAreaBubbleController* StatusAreaBubbleController::
ShowBubbleUnderViewForAWhile(views::View* view,
                             StatusAreaBubbleContentView* content) {
  gfx::Rect button_bounds = view->GetScreenBounds();
  button_bounds.set_y(button_bounds.y() + 1);  // See login/message_bubble.cc.

  StatusAreaBubbleController* controller = new StatusAreaBubbleController;
  views::View* content_parent = new CloseBubbleWhenClickedView(controller);
  content_parent->SetLayoutManager(new views::FillLayout);
  content_parent->AddChildView(content);
  controller->content_ = content;
  controller->bubble_ =
      Bubble::ShowFocusless(view->GetWidget(),
                            button_bounds,
                            views::BubbleBorder::TOP_RIGHT,
                            views::BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR,
                            content_parent,
                            controller,
                            true /* show_while_screen_is_locked */);
  controller->timer_.Start(FROM_HERE,
                           base::TimeDelta::FromSeconds(kCloseBubbleTimerInSec),
                           controller,
                           &StatusAreaBubbleController::HideBubble);
  return controller;
}

bool StatusAreaBubbleController::IsBubbleShown() const {
  return bubble_;
}

void StatusAreaBubbleController::HideBubble() {
  if (!IsBubbleShown())
    return;
  timer_.Stop();  // no-op if it's not running.
  bubble_->Close();
}

void StatusAreaBubbleController::BubbleClosing(Bubble* bubble,
                                               bool closed_by_escape) {
  bubble_ = NULL;
}

bool StatusAreaBubbleController::CloseOnEscape() {
  return true;
}

bool StatusAreaBubbleController::FadeInOnShow() {
  return false;
}

string16 StatusAreaBubbleController::GetAccessibleName() {
  return content_->GetMessage();
}

}  // namespace chromeos
