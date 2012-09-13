// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher.h"

#include <algorithm>
#include <cmath>

#include "ash/focus_cycler.h"
#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_navigator.h"
#include "ash/launcher/launcher_view.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/shelf_layout_manager.h"
#include "grit/ash_resources.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
// Size of black border at bottom (or side) of launcher.
const int kNumBlackPixels = 3;
}

namespace ash {

// The contents view of the Widget. This view contains LauncherView and
// sizes it to the width of the widget minus the size of the status area.
class Launcher::DelegateView : public views::WidgetDelegate,
                               public views::AccessiblePaneView,
                               public internal::BackgroundAnimatorDelegate {
 public:
  explicit DelegateView(Launcher* launcher);
  virtual ~DelegateView();

  void set_focus_cycler(internal::FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }
  internal::FocusCycler* focus_cycler() {
    return focus_cycler_;
  }

  // views::View overrides
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

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

  // BackgroundAnimatorDelegate overrides:
  virtual void UpdateBackground(int alpha) OVERRIDE;

 private:
  Launcher* launcher_;
  internal::FocusCycler* focus_cycler_;
  int alpha_;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

Launcher::DelegateView::DelegateView(Launcher* launcher)
    : launcher_(launcher),
      focus_cycler_(NULL),
      alpha_(0) {
}

Launcher::DelegateView::~DelegateView() {
}

gfx::Size Launcher::DelegateView::GetPreferredSize() {
  return child_count() > 0 ? child_at(0)->GetPreferredSize() : gfx::Size();
}

void Launcher::DelegateView::Layout() {
  if (child_count() == 0)
    return;
  View* launcher_view = child_at(0);

  if (launcher_->alignment_ == SHELF_ALIGNMENT_BOTTOM) {
    int w = std::max(0, width() - launcher_->status_size_.width());
    launcher_view->SetBounds(0, 0, w, height());
  } else {
    int h = std::max(0, height() - launcher_->status_size_.height());
    launcher_view->SetBounds(0, 0, width(), h);
  }
}

void Launcher::DelegateView::OnPaintBackground(gfx::Canvas* canvas) {
  if (launcher_->alignment_ == SHELF_ALIGNMENT_BOTTOM) {
    SkPaint paint;
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    static const gfx::ImageSkia* launcher_background = NULL;
    if (!launcher_background) {
      launcher_background =
          rb.GetImageNamed(IDR_AURA_LAUNCHER_BACKGROUND).ToImageSkia();
    }
    paint.setAlpha(alpha_);
    canvas->DrawImageInt(
        *launcher_background,
        0, 0, launcher_background->width(), launcher_background->height(),
        0, 0, width(), height(),
        false,
        paint);
    canvas->FillRect(
        gfx::Rect(0, height() - kNumBlackPixels, width(), kNumBlackPixels),
        SK_ColorBLACK);
  } else {
    // TODO(davemoore): when we get an image for the side launcher background
    // use it, and handle black border.
    canvas->DrawColor(SkColorSetARGB(alpha_, 0, 0, 0));
  }
}

void Launcher::DelegateView::UpdateBackground(int alpha) {
  alpha_ = alpha;
  SchedulePaint();
}

// Launcher --------------------------------------------------------------------

Launcher::Launcher(aura::Window* window_container,
                   internal::ShelfLayoutManager* shelf_layout_manager)
    : widget_(NULL),
      window_container_(window_container),
      delegate_view_(new DelegateView(this)),
      launcher_view_(NULL),
      alignment_(SHELF_ALIGNMENT_BOTTOM),
      background_animator_(delegate_view_, 0, kLauncherBackgroundAlpha) {
  model_.reset(new LauncherModel);
  if (Shell::GetInstance()->delegate()) {
    delegate_.reset(
        Shell::GetInstance()->delegate()->CreateLauncherDelegate(model_.get()));
  }

  widget_.reset(new views::Widget);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.transparent = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = Shell::GetContainer(
      window_container_->GetRootWindow(),
      ash::internal::kShellWindowId_LauncherContainer);
  launcher_view_ = new internal::LauncherView(
      model_.get(), delegate_.get(), shelf_layout_manager);
  launcher_view_->Init();
  delegate_view_->AddChildView(launcher_view_);
  params.delegate = delegate_view_;
  widget_->Init(params);
  widget_->GetNativeWindow()->SetName("LauncherWindow");
  gfx::Size pref =
      static_cast<views::View*>(launcher_view_)->GetPreferredSize();
  widget_->SetBounds(gfx::Rect(pref));
  // The launcher should not take focus when it is initially shown.
  widget_->set_focus_on_creation(false);
  widget_->SetContentsView(delegate_view_);
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

void Launcher::SetAlignment(ShelfAlignment alignment) {
  alignment_ = alignment;
  launcher_view_->SetAlignment(alignment);
  // ShelfLayoutManager will resize the launcher.
}

void Launcher::SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type) {
  background_animator_.SetPaintsBackground(value, change_type);
}

void Launcher::SetStatusSize(const gfx::Size& size) {
  if (status_size_ == size)
    return;

  status_size_ = size;
  delegate_view_->Layout();
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

void Launcher::ActivateLauncherItem(int index) {
  DCHECK(delegate_.get());
  const ash::LauncherItems& items = model_->items();
  delegate_->ItemClicked(items[index], ui::EF_NONE);
}

void Launcher::CycleWindowLinear(CycleDirection direction) {
  int item_index = GetNextActivatedItemIndex(*model(), direction);
  if (item_index >= 0)
    ActivateLauncherItem(item_index);
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

bool Launcher::IsShowingOverflowBubble() const {
  return launcher_view_->IsShowingOverflowBubble();
}

void Launcher::SetVisible(bool visible) const {
  delegate_view_->SetVisible(visible);
}

views::View* Launcher::GetAppListButtonView() const {
  return launcher_view_->GetAppListButtonView();
}

internal::LauncherView* Launcher::GetLauncherViewForTest() {
  return launcher_view_;
}

}  // namespace ash
