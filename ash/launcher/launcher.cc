// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher.h"

#include "ash/focus_cycler.h"
#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_view.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/shelf_layout_manager.h"
#include "base/timer.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/image/image.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Duration of the background animation.
const int kBackgroundDurationMS = 1000;

// Delay before showing the launcher after the mouse enters the view.
const int kShowDelayMS = 300;

// Max alpha of the background.
const int kBackgroundAlpha = 128;

}

// The contents view of the Widget. This view contains LauncherView and
// sizes it to the width of the widget minus the size of the status area.
class Launcher::DelegateView : public views::WidgetDelegate,
                               public views::AccessiblePaneView{
 public:
  explicit DelegateView(Launcher* launcher);
  virtual ~DelegateView();

  void SetStatusWidth(int width);
  int status_width() const { return status_width_; }

  void set_focus_cycler(const internal::FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }

  // views::View overrides
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;

  // views::WidgetDelegateView overrides:
  virtual views::Widget* GetWidget() OVERRIDE {
    return View::GetWidget();
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return View::GetWidget();
  }
  virtual bool CanActivate() const OVERRIDE {
    // We don't want mouse clicks to activate us, but we need to allow
    // activation when the user is using the keyboard (FocusCycler).
    return focus_cycler_ && focus_cycler_->widget_activating() == GetWidget();
  }

 private:
  // Shows the launcher.
  void ShowLauncher();

  Launcher* launcher_;

  // Width of the status area.
  int status_width_;

  const internal::FocusCycler* focus_cycler_;

  base::OneShotTimer<DelegateView> show_timer_;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

Launcher::DelegateView::DelegateView(Launcher* launcher)
    : launcher_(launcher),
      status_width_(0),
      focus_cycler_(NULL) {
  set_notify_enter_exit_on_child(true);
}

Launcher::DelegateView::~DelegateView() {
}

void Launcher::DelegateView::SetStatusWidth(int width) {
  if (status_width_ == width)
    return;

  status_width_ = width;
  Layout();
}

gfx::Size Launcher::DelegateView::GetPreferredSize() {
  return child_count() > 0 ? child_at(0)->GetPreferredSize() : gfx::Size();
}

void Launcher::DelegateView::Layout() {
  if (child_count() == 0)
    return;
  child_at(0)->SetBounds(0, 0, std::max(0, width() - status_width_), height());
}

void Launcher::DelegateView::OnMouseEntered(const views::MouseEvent& event) {
  if (!show_timer_.IsRunning()) {
    // The user may be trying to target a button near the bottom of the screen
    // and accidentally moved into the launcher area. Delay showing.
    show_timer_.Start(FROM_HERE,
                      base::TimeDelta::FromMilliseconds(kShowDelayMS),
                      this, &DelegateView::ShowLauncher);
  }
}

void Launcher::DelegateView::OnMouseExited(const views::MouseEvent& event) {
  show_timer_.Stop();
  internal::ShelfLayoutManager* shelf = Shell::GetInstance()->shelf();
  shelf->SetState(shelf->visibility_state(),
                  internal::ShelfLayoutManager::AUTO_HIDE_HIDDEN);
}

void Launcher::DelegateView::ShowLauncher() {
  show_timer_.Stop();
  internal::ShelfLayoutManager* shelf = Shell::GetInstance()->shelf();
  shelf->SetState(shelf->visibility_state(),
                  internal::ShelfLayoutManager::AUTO_HIDE_SHOWN);
}


// Launcher --------------------------------------------------------------------

Launcher::Launcher(aura::Window* window_container)
    : widget_(NULL),
      window_container_(window_container),
      delegate_view_(NULL),
      launcher_view_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(background_animation_(this)),
      renders_background_(false),
      background_alpha_(0) {
  model_.reset(new LauncherModel);
  if (Shell::GetInstance()->delegate()) {
    delegate_.reset(
        Shell::GetInstance()->delegate()->CreateLauncherDelegate(model_.get()));
  }

  widget_.reset(new views::Widget);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.create_texture_for_layer = true;
  params.transparent = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_LauncherContainer);
  launcher_view_ = new internal::LauncherView(model_.get(), delegate_.get());
  launcher_view_->Init();
  delegate_view_ = new DelegateView(this);
  delegate_view_->AddChildView(launcher_view_);
  params.delegate = delegate_view_;
  widget_->Init(params);
  widget_->GetNativeWindow()->SetName("LauncherWindow");
  gfx::Size pref =
      static_cast<views::View*>(launcher_view_)->GetPreferredSize();
  widget_->SetBounds(gfx::Rect(0, 0, pref.width(), pref.height()));
  // The launcher should not take focus when it is initially shown.
  widget_->set_focus_on_creation(false);
  widget_->SetContentsView(delegate_view_);
  widget_->Show();
  widget_->GetNativeView()->SetName("LauncherView");
  background_animation_.SetSlideDuration(kBackgroundDurationMS);
}

Launcher::~Launcher() {
}

void Launcher::SetFocusCycler(const internal::FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
}

void Launcher::SetRendersBackground(bool value, BackgroundChangeSpeed speed) {
  if (renders_background_ == value)
    return;
  renders_background_ = value;
  if (speed == CHANGE_IMMEDIATE && !background_animation_.is_animating()) {
    background_animation_.Reset(value ? 1.0f : 0.0f);
    AnimationProgressed(&background_animation_);
    return;
  }
  if (renders_background_)
    background_animation_.Show();
  else
    background_animation_.Hide();
}

void Launcher::SetStatusWidth(int width) {
  delegate_view_->SetStatusWidth(width);
}

int Launcher::GetStatusWidth() {
  return delegate_view_->status_width();
}

gfx::Rect Launcher::GetScreenBoundsOfItemIconForWindow(aura::Window* window) {
  if (!delegate_.get())
    return gfx::Rect();

  LauncherID id = delegate_->GetIDByWindow(window);
  gfx::Rect bounds(launcher_view_->GetIdealBoundsOfItemIcon(id));
  if (bounds.IsEmpty())
    return bounds;

  gfx::Point screen_origin;
  views::View::ConvertPointToScreen(launcher_view_, &screen_origin);
  return gfx::Rect(screen_origin.x() + bounds.x(),
                   screen_origin.y() + bounds.y(),
                   bounds.width(),
                   bounds.height());
}

internal::LauncherView* Launcher::GetLauncherViewForTest() {
  return static_cast<internal::LauncherView*>(
      widget_->GetContentsView()->child_at(0));
}

void Launcher::AnimationProgressed(const ui::Animation* animation) {
  int alpha = animation->CurrentValueBetween(0, kBackgroundAlpha);
  if (background_alpha_ == alpha)
    return;
  background_alpha_ = alpha;
  if (alpha == 0) {
    delegate_view_->set_background(NULL);
  } else {
    delegate_view_->set_background(
        views::Background::CreateSolidBackground(0, 0, 0, alpha));
  }
  delegate_view_->SchedulePaint();
}

}  // namespace ash
