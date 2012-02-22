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
#include "grit/ui_resources.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/image/image.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Used to draw the background of the shelf.
class ShelfPainter : public views::Painter {
 public:
  ShelfPainter() {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    image_ = *rb.GetImageNamed(IDR_AURA_LAUNCHER_BACKGROUND).ToSkBitmap();
  }

  // Overridden from views::Painter:
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) OVERRIDE {
    canvas->TileImageInt(image_, 0, 0, size.width(), size.height());
  }

 private:
  SkBitmap image_;

  DISALLOW_COPY_AND_ASSIGN(ShelfPainter);
};

}  // namespace

// The contents view of the Widget. This view contains LauncherView and
// sizes it to the width of the widget minus the size of the status area.
class Launcher::DelegateView : public views::WidgetDelegate,
                               public views::AccessiblePaneView {
 public:
  explicit DelegateView();
  virtual ~DelegateView();

  void SetStatusWidth(int width);
  int status_width() const { return status_width_; }

  void set_focus_cycler(const internal::FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }

  // views::View overrides
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

  virtual views::Widget* GetWidget() OVERRIDE {
    return View::GetWidget();
  }

  virtual const views::Widget* GetWidget() const OVERRIDE {
    return View::GetWidget();
  }

  // views::WidgetDelegateView overrides:
  virtual bool CanActivate() const OVERRIDE {
    // We don't want mouse clicks to activate us, but we need to allow
    // activation when the user is using the keyboard (FocusCycler).
    return focus_cycler_ && focus_cycler_->widget_activating() == GetWidget();
  }

 private:
  int status_width_;
  const internal::FocusCycler* focus_cycler_;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

Launcher::DelegateView::DelegateView()
    : status_width_(0),
      focus_cycler_(NULL) {
  set_background(
      views::Background::CreateBackgroundPainter(true, new ShelfPainter()));
}

Launcher::DelegateView::~DelegateView() {
}

void Launcher::SetFocusCycler(const internal::FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
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

Launcher::Launcher(aura::Window* window_container)
    : widget_(NULL),
      window_container_(window_container),
      delegate_view_(NULL) {
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
  internal::LauncherView* launcher_view =
      new internal::LauncherView(model_.get(), delegate_.get());
  launcher_view->Init();
  delegate_view_ = new DelegateView;
  delegate_view_->AddChildView(launcher_view);
  params.delegate = delegate_view_;
  widget_->Init(params);
  widget_->GetNativeWindow()->SetName("LauncherWindow");
  gfx::Size pref = static_cast<views::View*>(launcher_view)->GetPreferredSize();
  widget_->SetBounds(gfx::Rect(0, 0, pref.width(), pref.height()));
  // The launcher should not take focus when it is initially shown.
  widget_->set_focus_on_creation(false);
  widget_->SetContentsView(delegate_view_);
  widget_->Show();
  widget_->GetNativeView()->SetName("LauncherView");
}

Launcher::~Launcher() {
}

void Launcher::SetStatusWidth(int width) {
  delegate_view_->SetStatusWidth(width);
}

int Launcher::GetStatusWidth() {
  return delegate_view_->status_width();
}

}  // namespace ash
