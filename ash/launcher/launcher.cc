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
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

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

  void set_focus_cycler(internal::FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }
  internal::FocusCycler* focus_cycler() {
    return focus_cycler_;
  }

  // views::View overrides
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

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
  Launcher* launcher_;

  // Width of the status area.
  int status_width_;

  internal::FocusCycler* focus_cycler_;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

Launcher::DelegateView::DelegateView(Launcher* launcher)
    : launcher_(launcher),
      status_width_(0),
      focus_cycler_(NULL) {
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

// Launcher --------------------------------------------------------------------

Launcher::Launcher(aura::Window* window_container)
    : widget_(NULL),
      window_container_(window_container),
      delegate_view_(NULL),
      launcher_view_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          background_animator_(this, 0, kBackgroundAlpha)) {
  model_.reset(new LauncherModel);
  if (Shell::GetInstance()->delegate()) {
    delegate_.reset(
        Shell::GetInstance()->delegate()->CreateLauncherDelegate(model_.get()));
  }

  widget_.reset(new views::Widget);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  // The launcher only ever draws a solid color.
  params.layer_type = ui::LAYER_SOLID_COLOR;
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
}

Launcher::~Launcher() {
}

void Launcher::SetFocusCycler(internal::FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
  focus_cycler->AddWidget(widget_.get());
}

internal::FocusCycler* Launcher::GetFocusCycler() {
  return delegate_view_->focus_cycler();
}

void Launcher::SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type) {
  background_animator_.SetPaintsBackground(value, change_type);
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

void Launcher::AddIconObserver(LauncherIconObserver* observer) {
  launcher_view_->AddIconObserver(observer);
}

void Launcher::RemoveIconObserver(LauncherIconObserver* observer) {
  launcher_view_->RemoveIconObserver(observer);
}

bool Launcher::IsShowingMenu() const {
  return launcher_view_->IsShowingMenu();
}

internal::LauncherView* Launcher::GetLauncherViewForTest() {
  return launcher_view_;
}

void Launcher::UpdateBackground(int alpha) {
  ui::Layer* layer = widget_->GetNativeView()->layer();
  layer->SetColor(SkColorSetARGB(alpha, 0, 0, 0));
}

}  // namespace ash
