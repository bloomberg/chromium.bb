// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/transparent_activate_window_button.h"

#include <vector>

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overview/transparent_activate_window_button_delegate.h"
#include "ui/events/event.h"
#include "ui/events/gesture_event_details.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {
const char kTransparentButtonName[] = "TransparentButton";

// Transparent button that handles events to activate or close windows in
// overview mode.
class TransparentButton
    : public views::CustomButton,
      public views::ButtonListener {
 public:
  explicit TransparentButton(TransparentActivateWindowButtonDelegate* delegate);

  ~TransparentButton() override;

  TransparentActivateWindowButtonDelegate* delegate() { return delegate_; }

  // views::View:
  void OnGestureEvent(ui::GestureEvent* event) override;

  const char* GetClassName() const override { return kTransparentButtonName; }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  TransparentActivateWindowButtonDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TransparentButton);
};

TransparentButton::TransparentButton(
        TransparentActivateWindowButtonDelegate* delegate)
    : CustomButton(this),
      delegate_(delegate) {
}

TransparentButton::~TransparentButton() {
}

void TransparentButton::OnGestureEvent(ui::GestureEvent* event) {
  // TODO(tdanderson): Re-evaluate whether we want to set capture once
  //                   the a fix has landed to avoid crashing when a window
  //                   having an active gesture sequence is destroyed as a
  //                   result of a gesture in a separate window. Consider using
  //                   a StaticWindowTargeter instead of a transparent overlay
  //                   widget to re-direct input events.
  if (event->type() == ui::ET_GESTURE_TAP_DOWN)
    GetWidget()->SetCapture(this);

  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_END) {
    GetWidget()->ReleaseCapture();
  }

  CustomButton::OnGestureEvent(event);
  event->StopPropagation();
}

void TransparentButton::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  delegate_->Select();
}

// Initializes the event handler transparent window.
views::Widget* InitEventHandler(aura::Window* root_window) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = true;
  params.parent = Shell::GetContainer(root_window,
                                      kShellWindowId_OverlayContainer);
  widget->set_focus_on_creation(false);
  widget->Init(params);
  widget->Show();

  aura::Window* handler = widget->GetNativeWindow();
  handler->parent()->StackChildAtBottom(handler);

  return widget;
}

}  // namespace

TransparentActivateWindowButton::TransparentActivateWindowButton(
        aura::Window* root_window,
        TransparentActivateWindowButtonDelegate* delegate)
    : event_handler_widget_(InitEventHandler(root_window)),
      transparent_button_(new TransparentButton(delegate)) {
  event_handler_widget_->SetContentsView(transparent_button_);
}

TransparentActivateWindowButton::~TransparentActivateWindowButton() {
}

void TransparentActivateWindowButton::SetAccessibleName(
    const base::string16& name) {
  transparent_button_->SetAccessibleName(name);
}

void TransparentActivateWindowButton::SetBounds(const gfx::Rect& bounds) {
  aura::Window* window = event_handler_widget_->GetNativeWindow();
  const gfx::Display& display = gfx::Screen::GetScreenFor(
      window)->GetDisplayNearestWindow(window);
  // Explicitly specify the display so that the window doesn't change root
  // windows and cause the z-order of the transparent overlays to be updated.
  window->SetBoundsInScreen(bounds, display);
}

void TransparentActivateWindowButton::SendFocusAlert() const {
  event_handler_widget_->GetContentsView()->
      NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, true);
}

void TransparentActivateWindowButton::StackAbove(
    TransparentActivateWindowButton* activate_button) {
  aura::Window* this_window = event_handler_widget_->GetNativeWindow();
  aura::Window* other_window = activate_button->event_handler_widget_->
      GetNativeWindow();
  this_window->parent()->StackChildAbove(this_window, other_window);
}

}  // namespace ash
