// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/transparent_activate_window_button.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_state.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Transparent button that handles events which activate windows in overview
// mode.
class TransparentButton : public views::CustomButton {
 public:
  explicit TransparentButton(views::ButtonListener* listener)
      : CustomButton(listener) {
  }
  virtual ~TransparentButton() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TransparentButton);
};

// Initializes the event handler transparent window.
views::Widget* InitEventHandler(aura::Window* root_window) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = true;
  params.parent = Shell::GetContainer(root_window,
                                      ash::kShellWindowId_OverlayContainer);
  widget->set_focus_on_creation(false);
  widget->Init(params);
  widget->Show();

  aura::Window* handler = widget->GetNativeWindow();
  handler->parent()->StackChildAtBottom(handler);

  return widget;
}

}  // namespace

TransparentActivateWindowButton::TransparentActivateWindowButton(
    aura::Window* activate_window)
    : event_handler_widget_(InitEventHandler(activate_window->GetRootWindow())),
      activate_window_(activate_window) {
  views::Button* transparent_button = new TransparentButton(this);
  transparent_button->SetAccessibleName(activate_window->title());
  event_handler_widget_->SetContentsView(transparent_button);
}

void TransparentActivateWindowButton::SetBounds(const gfx::Rect& bounds) {
  event_handler_widget_->SetBounds(bounds);
}

void TransparentActivateWindowButton::SendFocusAlert() const {
  event_handler_widget_->GetContentsView()->
      NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, true);
}

TransparentActivateWindowButton::~TransparentActivateWindowButton() {
}

void TransparentActivateWindowButton::ButtonPressed(views::Button* sender,
                                                    const ui::Event& event) {
  wm::GetWindowState(activate_window_)->Activate();
}

}  // namespace ash
