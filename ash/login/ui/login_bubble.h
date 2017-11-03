// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_BUBBLE_H_
#define ASH_LOGIN_UI_LOGIN_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_base_bubble_view.h"
#include "base/strings/string16.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class StyledLabel;
}

namespace ash {
class LoginButton;

// A wrapper for the bubble view in the login screen.
// This class observes keyboard events, mouse clicks and touch down events
// and dismisses the bubble accordingly.
class ASH_EXPORT LoginBubble : public views::WidgetObserver,
                               public ui::EventHandler,
                               public ui::LayerAnimationObserver {
 public:
  LoginBubble();
  ~LoginBubble() override;

  // Shows an error bubble for authentication failure.
  // |anchor_view| is the anchor for placing the bubble view.
  void ShowErrorBubble(views::StyledLabel* label, views::View* anchor_view);

  // Shows a user menu bubble.
  // |anchor_view| is the anchor for placing the bubble view.
  // |bubble_opener| is a view that could open/close the bubble.
  // |show_remove_user| indicate whether or not we show the
  // "Remove this user" action.
  void ShowUserMenu(const base::string16& message,
                    const base::string16& sub_message,
                    views::View* anchor_view,
                    LoginButton* bubble_opener,
                    bool show_remove_user);

  // Shows a tooltip.
  void ShowTooltip(const base::string16& message, views::View* anchor_view);

  // Schedule animation for closing the bubble.
  // The bubble widget will be closed when the animation is ended.
  void Close();

  // True if the bubble is visible.
  bool IsVisible();

  // views::WidgetObservers:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // gfx::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override{};
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override{};

  LoginBaseBubbleView* bubble_view_for_test() { return bubble_view_; }

 private:
  // Show the bubble widget and schedule animation for bubble showing.
  void Show();

  // Close the bubble immediately, without scheduling animation.
  // Used to clean up old bubble widget when a new bubble is
  // going to be created.
  void CloseImmediately();

  void ProcessPressedEvent(const ui::LocatedEvent* event);

  // Starts show/hide animation.
  void ScheduleAnimation(bool visible);

  LoginBaseBubbleView* bubble_view_ = nullptr;

  // A button that could open/close the bubble.
  LoginButton* bubble_opener_ = nullptr;

  // The status of bubble after animation ends.
  bool is_visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(LoginBubble);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_BUBBLE_H_
