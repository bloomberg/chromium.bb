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
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "grit/ash_resources.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
// Size of black border at bottom (or side) of launcher.
const int kNumBlackPixels = 3;
// Alpha to paint dimming image with.
const int kDimAlpha = 96;
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
    // Allow to activate as fallback.
    if (launcher_->activating_as_fallback_)
      return true;
    // Allow to activate from the focus cycler.
    if (focus_cycler_ && focus_cycler_->widget_activating() == GetWidget())
      return true;
    // Disallow activating in other cases, especially when using mouse.
    return false;
  }

  // BackgroundAnimatorDelegate overrides:
  virtual void UpdateBackground(int alpha) OVERRIDE;

 private:
  Launcher* launcher_;
  internal::FocusCycler* focus_cycler_;
  int alpha_;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

// Class used to slightly dim shelf items when maximized and visible. It also
// makes sure the widget changes size to always be of the same size as the
// shelf.
class Launcher::DimmerView : public views::WidgetDelegateView,
                   public aura::WindowObserver {
 public:
  explicit DimmerView(Launcher* launcher)
      : launcher_(launcher) {
    launcher_->widget()->GetNativeWindow()->AddObserver(this);
  }

  ~DimmerView() {
    if (launcher_)
      launcher_->widget()->GetNativeWindow()->RemoveObserver(this);
  }

 private:
  // views::View overrides:
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    gfx::ImageSkia background_image =
        *rb.GetImageSkiaNamed(IDR_AURA_LAUNCHER_DIMMING);
    if (SHELF_ALIGNMENT_BOTTOM != launcher_->alignment_)
      background_image = gfx::ImageSkiaOperations::CreateRotatedImage(
          background_image,
          (SHELF_ALIGNMENT_LEFT == launcher_->alignment_ ?
              SkBitmapOperations::ROTATION_90_CW :
              SkBitmapOperations::ROTATION_270_CW));

    SkPaint paint;
    paint.setAlpha(kDimAlpha);
    canvas->DrawImageInt(
        background_image,
        0, 0, background_image.width(), background_image.height(),
        0, 0, width(), height(),
        false,
        paint);
  }

  // aura::WindowObserver overrides:
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE {
    CHECK_EQ(window, launcher_->widget()->GetNativeWindow());
    GetWidget()->GetNativeWindow()->SetBounds(window->bounds());
  }

  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    CHECK_EQ(window, launcher_->widget()->GetNativeWindow());
    launcher_->widget()->GetNativeWindow()->RemoveObserver(this);
    launcher_ = NULL;
  }

  Launcher* launcher_;
  DISALLOW_COPY_AND_ASSIGN(DimmerView);
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
  ash::internal::ShelfLayoutManager* shelf = ash::GetRootWindowController(
          GetWidget()->GetNativeView()->GetRootWindow())->shelf();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::ImageSkia launcher_background =
      *rb.GetImageSkiaNamed(IDR_AURA_LAUNCHER_BACKGROUND);
  if (launcher_->alignment_ != SHELF_ALIGNMENT_BOTTOM)
    launcher_background = gfx::ImageSkiaOperations::CreateRotatedImage(
        launcher_background,
        shelf->SelectValueForShelfAlignment(
            SkBitmapOperations::ROTATION_90_CW,
            SkBitmapOperations::ROTATION_90_CW,
            SkBitmapOperations::ROTATION_270_CW));

  gfx::Rect black_rect = shelf->SelectValueForShelfAlignment(
    gfx::Rect(0, height() - kNumBlackPixels, width(), kNumBlackPixels),
    gfx::Rect(0, 0, kNumBlackPixels, height()),
    gfx::Rect(width() - kNumBlackPixels, 0, kNumBlackPixels, height()));

  SkPaint paint;
  paint.setAlpha(alpha_);
  canvas->DrawImageInt(
      launcher_background,
      0, 0, launcher_background.width(), launcher_background.height(),
      0, 0, width(), height(),
      false,
      paint);
  canvas->FillRect(black_rect, SK_ColorBLACK);
}

void Launcher::DelegateView::UpdateBackground(int alpha) {
  alpha_ = alpha;
  SchedulePaint();
}

// Launcher --------------------------------------------------------------------

Launcher::Launcher(LauncherModel* launcher_model,
                   LauncherDelegate* launcher_delegate,
                   aura::Window* window_container,
                   internal::ShelfLayoutManager* shelf_layout_manager)
    : widget_(NULL),
      window_container_(window_container),
      delegate_view_(new DelegateView(this)),
      launcher_view_(NULL),
      alignment_(SHELF_ALIGNMENT_BOTTOM),
      delegate_(launcher_delegate),
      background_animator_(delegate_view_, 0, kLauncherBackgroundAlpha),
      activating_as_fallback_(false) {
  widget_.reset(new views::Widget);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.transparent = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = Shell::GetContainer(
      window_container_->GetRootWindow(),
      ash::internal::kShellWindowId_LauncherContainer);
  launcher_view_ = new internal::LauncherView(
      launcher_model, delegate_, shelf_layout_manager);
  launcher_view_->Init();
  delegate_view_->AddChildView(launcher_view_);
  params.delegate = delegate_view_;
  widget_->Init(params);
  widget_->GetNativeWindow()->SetName("LauncherWindow");
  // The launcher should not take focus when it is initially shown.
  widget_->set_focus_on_creation(false);
  widget_->SetContentsView(delegate_view_);
  widget_->GetNativeView()->SetName("LauncherView");
  widget_->GetNativeView()->SetProperty(internal::kStayInSameRootWindowKey,
                                        true);

  // SetBounds() has to be called after kStayInSameRootWindowKey is set.
  gfx::Size pref =
      static_cast<views::View*>(launcher_view_)->GetPreferredSize();
  widget_->SetBounds(gfx::Rect(pref));

  widget_->AddObserver(this);
}

Launcher::~Launcher() {
  widget_->RemoveObserver(this);
}

// static
Launcher* Launcher::ForPrimaryDisplay() {
  return internal::RootWindowController::ForLauncher(
      Shell::GetPrimaryRootWindow())->launcher();
}

// static
Launcher* Launcher::ForWindow(aura::Window* window) {
  return internal::RootWindowController::ForLauncher(window)->launcher();
}

void Launcher::SetFocusCycler(internal::FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
  if (focus_cycler)
    focus_cycler->AddWidget(widget_.get());
}

internal::FocusCycler* Launcher::GetFocusCycler() {
  return delegate_view_->focus_cycler();
}

void Launcher::SetAlignment(ShelfAlignment alignment) {
  alignment_ = alignment;
  delegate_view_->SchedulePaint();
  launcher_view_->OnShelfAlignmentChanged();
  // ShelfLayoutManager will resize the launcher.
}

void Launcher::SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type) {
  background_animator_.SetPaintsBackground(value, change_type);
}

void Launcher::SetDimsShelf(bool value) {
  if (value == (dimmer_.get() != NULL))
    return;

  if (!value) {
    dimmer_.reset();
    return;
  }

  dimmer_.reset(new views::Widget);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.transparent = true;
  params.can_activate = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = Shell::GetContainer(
      window_container_->GetRootWindow(),
      ash::internal::kShellWindowId_LauncherContainer);
  params.accept_events = false;
  dimmer_->Init(params);
  dimmer_->GetNativeWindow()->SetName("LauncherDimmer");
  dimmer_->SetBounds(widget_->GetWindowBoundsInScreen());
  // The launcher should not take focus when it is initially shown.
  dimmer_->set_focus_on_creation(false);
  dimmer_->SetContentsView(new DimmerView(this));
  dimmer_->GetNativeView()->SetName("LauncherDimmerView");
  dimmer_->GetNativeView()->SetProperty(internal::kStayInSameRootWindowKey,
                                        true);
  dimmer_->Show();
}

bool Launcher::GetDimsShelf() const {
  return dimmer_.get() && dimmer_->IsVisible();
}

void Launcher::SetStatusSize(const gfx::Size& size) {
  if (status_size_ == size)
    return;

  status_size_ = size;
  delegate_view_->Layout();
}

gfx::Rect Launcher::GetScreenBoundsOfItemIconForWindow(aura::Window* window) {
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
  const ash::LauncherItems& items =
      launcher_view_->model()->items();
  delegate_->ItemClicked(items[index], ui::EF_NONE);
}

void Launcher::CycleWindowLinear(CycleDirection direction) {
  int item_index = GetNextActivatedItemIndex(
      *(launcher_view_->model()), direction);
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

void Launcher::ShowContextMenu(const gfx::Point& location) {
  launcher_view_->ShowContextMenu(location, false);
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

void Launcher::SetWidgetBounds(const gfx::Rect bounds) {
  widget_->SetBounds(bounds);
  if (dimmer_.get())
    dimmer_->SetBounds(bounds);
}

void Launcher::SwitchToWindow(int window_index) {
  LauncherModel* launcher_model = launcher_view_->model();
  const LauncherItems& items = launcher_model->items();
  int item_count = launcher_model->item_count();
  int indexes_left = window_index >= 0 ? window_index : item_count;
  int found_index = -1;

  // Iterating until we have hit the index we are interested in which
  // is true once indexes_left becomes negative.
  for (int i = 0; i < item_count && indexes_left >= 0; i++) {
    if (items[i].type != TYPE_APP_LIST &&
        items[i].type != TYPE_BROWSER_SHORTCUT) {
      found_index = i;
      indexes_left--;
    }
  }

  // There are two ways how found_index can be valid: a.) the nth item was
  // found (which is true when indexes_left is -1) or b.) the last item was
  // requested (which is true when index was passed in as a negative number).
  if (found_index >= 0 && (indexes_left == -1 || window_index < 0) &&
      (items[found_index].status == ash::STATUS_RUNNING ||
       items[found_index].status == ash::STATUS_CLOSED)) {
    // Then set this one as active.
    ActivateLauncherItem(found_index);
  }
}

void Launcher::OnWidgetActivationChanged(views::Widget* widget, bool active) {
  activating_as_fallback_ = false;
  if (active) {
    delegate_view_->SetPaneFocusAndFocusDefault();
  } else {
    delegate_view_->GetFocusManager()->ClearFocus();
  }
}

internal::LauncherView* Launcher::GetLauncherViewForTest() {
  return launcher_view_;
}

}  // namespace ash
