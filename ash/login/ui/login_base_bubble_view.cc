// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_base_bubble_view.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/scoped_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_handler.h"
#include "ui/views/layout/box_layout.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// Total width of the bubble view.
constexpr int kBubbleTotalWidthDp = 178;

// Horizontal margin of the bubble view.
constexpr int kBubbleHorizontalMarginDp = 14;

// Top margin of the bubble view.
constexpr int kBubbleTopMarginDp = 13;

// Bottom margin of the bubble view.
constexpr int kBubbleBottomMarginDp = 18;

// The amount of time for bubble show/hide animation.
constexpr base::TimeDelta kBubbleAnimationDuration =
    base::TimeDelta::FromMilliseconds(300);

}  // namespace

// This class handles keyboard, mouse, and focus events, and dismisses the
// associated bubble in response.
class LoginBubbleHandler : public ui::EventHandler,
                           public aura::client::FocusChangeObserver {
 public:
  LoginBubbleHandler(LoginBaseBubbleView* bubble) : bubble_(bubble) {
    Shell::Get()->AddPreTargetHandler(this);
    focus_observer_.Add(
        aura::client::GetFocusClient(Shell::GetPrimaryRootWindow()));
  }

  ~LoginBubbleHandler() override { Shell::Get()->RemovePreTargetHandler(this); }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_PRESSED)
      ProcessPressedEvent(event->AsLocatedEvent());
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::ET_GESTURE_TAP ||
        event->type() == ui::ET_GESTURE_TAP_DOWN) {
      ProcessPressedEvent(event->AsLocatedEvent());
    }
  }

  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->type() != ui::ET_KEY_PRESSED ||
        event->key_code() == ui::VKEY_PROCESSKEY) {
      return;
    }

    if (!bubble_->IsVisible())
      return;

    if (bubble_->GetBubbleOpener() && bubble_->GetBubbleOpener()->HasFocus())
      return;

    if (bubble_->GetWidget()->IsActive())
      return;

    if (!bubble_->IsPersistent())
      bubble_->Hide();
  }

  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override {
    if (!bubble_->IsVisible())
      return;

    if (gained_focus &&
        bubble_->GetWidget()->GetNativeView()->Contains(gained_focus)) {
      return;
    }

    if (!bubble_->IsPersistent())
      bubble_->Hide();
  }

 private:
  void ProcessPressedEvent(const ui::LocatedEvent* event) {
    if (!bubble_->IsVisible())
      return;

    gfx::Point screen_location = event->location();
    ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event->target()),
                               &screen_location);

    // Don't do anything with clicks inside the bubble.
    if (bubble_->GetBoundsInScreen().Contains(screen_location))
      return;

    // Let the bubble opener handle clicks on itself.
    if (bubble_->GetBubbleOpener() &&
        bubble_->GetBubbleOpener()->GetBoundsInScreen().Contains(
            screen_location)) {
      return;
    }

    if (!bubble_->IsPersistent())
      bubble_->Hide();
  }

  LoginBaseBubbleView* bubble_;

  ScopedObserver<aura::client::FocusClient, aura::client::FocusChangeObserver>
      focus_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginBubbleHandler);
};

LoginBaseBubbleView::LoginBaseBubbleView(views::View* anchor_view)
    : LoginBaseBubbleView(anchor_view, nullptr) {}

LoginBaseBubbleView::LoginBaseBubbleView(views::View* anchor_view,
                                         aura::Window* parent_window)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::NONE),
      bubble_handler_(std::make_unique<LoginBubbleHandler>(this)) {
  set_margins(gfx::Insets(kBubbleTopMarginDp, kBubbleHorizontalMarginDp,
                          kBubbleBottomMarginDp, kBubbleHorizontalMarginDp));
  set_color(SK_ColorBLACK);
  set_can_activate(false);
  set_close_on_deactivate(false);

  // Layer rendering is needed for animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  set_parent_window(parent_window);
}

LoginBaseBubbleView::~LoginBaseBubbleView() = default;

void LoginBaseBubbleView::Show() {
  views::Widget* widget = GetWidget();

  if (!widget)
    widget = views::BubbleDialogDelegateView::CreateBubble(this);

  layer()->GetAnimator()->RemoveObserver(this);

  Layout();
  SizeToContents();

  widget->ShowInactive();
  widget->StackAtTop();

  ScheduleAnimation(true /*visible*/);

  // Tell ChromeVox to read bubble contents.
  NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                           true /*send_native_event*/);
}

void LoginBaseBubbleView::Hide() {
  if (GetWidget())
    ScheduleAnimation(false /*visible*/);
}

bool LoginBaseBubbleView::IsVisible() {
  return GetWidget() && GetWidget()->IsVisible();
}

LoginButton* LoginBaseBubbleView::GetBubbleOpener() const {
  return nullptr;
}

bool LoginBaseBubbleView::IsPersistent() const {
  return false;
}

void LoginBaseBubbleView::SetPersistent(bool persistent) {}

void LoginBaseBubbleView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  // This case only gets called if the bubble has no anchor and no parent
  // container was specified. In this case, the parent container should default
  // to MenuContainer, so that login bubbles are visible over the shelf and
  // virtual keyboard. Shell may be null in tests.
  if (!params->parent && Shell::HasInstance()) {
    params->parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                         kShellWindowId_MenuContainer);
  }
}

int LoginBaseBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void LoginBaseBubbleView::SetAnchorView(views::View* anchor_view) {
  views::BubbleDialogDelegateView::SetAnchorView(anchor_view);
}

void LoginBaseBubbleView::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  layer()->GetAnimator()->RemoveObserver(this);
  GetWidget()->Hide();
}

gfx::Size LoginBaseBubbleView::CalculatePreferredSize() const {
  gfx::Size size;
  size.set_width(kBubbleTotalWidthDp);
  size.set_height(GetHeightForWidth(kBubbleTotalWidthDp));
  return size;
}

void LoginBaseBubbleView::OnWidgetVisibilityChanged(views::Widget* widget,
                                                    bool visible) {
  if (visible)
    EnsureInScreen();
}

void LoginBaseBubbleView::OnWidgetBoundsChanged(views::Widget* widget,
                                                const gfx::Rect& new_bounds) {
  EnsureInScreen();
}

void LoginBaseBubbleView::ScheduleAnimation(bool visible) {
  if (GetBubbleOpener()) {
    GetBubbleOpener()->AnimateInkDrop(visible
                                          ? views::InkDropState::ACTIVATED
                                          : views::InkDropState::DEACTIVATED,
                                      nullptr /*event*/);
  }

  layer()->GetAnimator()->StopAnimating();

  float opacity_start = 0.0f;
  float opacity_end = 1.0f;
  if (!visible) {
    std::swap(opacity_start, opacity_end);
    // We only need to handle animation ending if we're hiding the bubble.
    layer()->GetAnimator()->AddObserver(this);
  }

  layer()->SetOpacity(opacity_start);
  {
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.SetTransitionDuration(kBubbleAnimationDuration);
    settings.SetTweenType(visible ? gfx::Tween::EASE_OUT : gfx::Tween::EASE_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer()->SetOpacity(opacity_end);
  }
}

void LoginBaseBubbleView::EnsureInScreen() {
  DCHECK(GetWidget());

  const gfx::Rect view_bounds = GetBoundsInScreen();
  const gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(GetWidget()->GetNativeWindow())
          .work_area();

  int horizontal_offset = 0;

  // If the widget extends past the right side of the screen, make it go to
  // the left instead.
  if (work_area.right() < view_bounds.right()) {
    horizontal_offset = -view_bounds.width();
  }

  set_anchor_view_insets(
      anchor_view_insets().Offset(gfx::Vector2d(horizontal_offset, 0)));
  OnAnchorBoundsChanged();
}

}  // namespace ash
