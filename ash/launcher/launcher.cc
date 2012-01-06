// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher.h"

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
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Used to draw the background of the shelf.
class ShelfPainter : public views::Painter {
 public:
  ShelfPainter() {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    image_ = *rb.GetImageNamed(IDR_AURA_LAUNCHER_BACKGROUND).ToSkBitmap();
  }

  virtual void Paint(int w, int h, gfx::Canvas* canvas) OVERRIDE {
    canvas->TileImageInt(image_, 0, 0, w, h);
  }

 private:
  SkBitmap image_;

  DISALLOW_COPY_AND_ASSIGN(ShelfPainter);
};

}  // namespace

// The contents view of the Widget. This view contains LauncherView and
// sizes it to the width of the widget minus the size of the status area.
class Launcher::DelegateView : public views::WidgetDelegateView {
 public:
  explicit DelegateView();
  virtual ~DelegateView();

  void SetStatusWidth(int width);
  int status_width() const { return status_width_; }

  // views::View overrides
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

 private:
  int status_width_;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

Launcher::DelegateView::DelegateView()
    : status_width_(0) {
  set_background(
      views::Background::CreateBackgroundPainter(true, new ShelfPainter()));
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

Launcher::Launcher(aura::Window* window_container)
    : widget_(NULL),
      window_container_(window_container),
      delegate_view_(NULL) {
  window_container->AddObserver(this);

  model_.reset(new LauncherModel);

  widget_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.create_texture_for_layer = true;
  params.transparent = true;
  params.parent = Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_LauncherContainer);
  internal::LauncherView* launcher_view =
      new internal::LauncherView(model_.get());
  launcher_view->Init();
  delegate_view_ = new DelegateView;
  delegate_view_->AddChildView(launcher_view);
  params.delegate = delegate_view_;
  widget_->Init(params);
  gfx::Size pref = static_cast<views::View*>(launcher_view)->GetPreferredSize();
  widget_->SetBounds(gfx::Rect(0, 0, pref.width(), pref.height()));
  widget_->SetContentsView(delegate_view_);
  widget_->Show();
  widget_->GetNativeView()->SetName("LauncherView");
}

Launcher::~Launcher() {
  widget_->CloseNow();
  window_container_->RemoveObserver(this);
  for (WindowMap::iterator i = known_windows_.begin();
       i != known_windows_.end(); ++i) {
    i->first->RemoveObserver(this);
  }
}

void Launcher::SetStatusWidth(int width) {
  delegate_view_->SetStatusWidth(width);
}

int Launcher::GetStatusWidth() {
  return delegate_view_->status_width();
}

void Launcher::MaybeAdd(aura::Window* window) {
  if (known_windows_[window] == true)
    return;  // We already tried to add this window.

  known_windows_[window] = true;
  ShellDelegate* delegate = Shell::GetInstance()->delegate();
  if (!delegate)
    return;
  LauncherItem item;
  item.window = window;
  if (!delegate->ConfigureLauncherItem(&item))
    return;  // The delegate doesn't want to show this item in the launcher.
  model_->Add(model_->items().size(), item);
}

void Launcher::OnWindowAdded(aura::Window* new_window) {
  if (new_window->parent() != window_container_)
    return;

  DCHECK(known_windows_.find(new_window) == known_windows_.end());
  known_windows_[new_window] = false;
  new_window->AddObserver(this);
  // Windows are created initially invisible. Wait until the window is made
  // visible before asking, as otherwise the delegate likely doesn't know about
  // window (it's still creating it).
  if (new_window->IsVisible())
    MaybeAdd(new_window);
}

void Launcher::OnWillRemoveWindow(aura::Window* window) {
  if (window->parent() != window_container_)
    return;

  window->RemoveObserver(this);
  known_windows_.erase(window);
  LauncherItems::const_iterator i = model_->ItemByWindow(window);
  if (i != model_->items().end())
    model_->RemoveItemAt(i - model_->items().begin());
}

void Launcher::OnWindowVisibilityChanged(aura::Window* window,
                                         bool visibile) {
  if (visibile && !known_windows_[window])
    MaybeAdd(window);
}

}  // namespace ash
