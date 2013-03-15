// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_widget.h"

#include "ash/focus_cycler.h"
#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_navigator.h"
#include "ash/launcher/launcher_view.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/property_util.h"
#include "ash/wm/status_area_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/workspace_controller.h"
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

// The contents view of the Shelf. This view contains LauncherView and
// sizes it to the width of the shelf minus the size of the status area.
class ShelfWidget::DelegateView : public views::WidgetDelegate,
                                  public views::AccessiblePaneView,
                                  public internal::BackgroundAnimatorDelegate {
 public:
  explicit DelegateView(ShelfWidget* shelf);
  virtual ~DelegateView();

  void set_focus_cycler(internal::FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }
  internal::FocusCycler* focus_cycler() {
    return focus_cycler_;
  }

  // Set if the shelf area is dimmed (eg when a window is maximized).
  void SetDimmed(bool dimmed) {
    if (dimmed_ != dimmed) {
      dimmed_ = dimmed;
      SchedulePaint();
    }
  }
  bool GetDimmed() const { return dimmed_; }

  // views::View overrides:
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

  // views::WidgetDelegateView overrides:
  virtual views::Widget* GetWidget() OVERRIDE {
    return View::GetWidget();
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return View::GetWidget();
  }

  virtual bool CanActivate() const OVERRIDE;
  virtual void Layout() OVERRIDE;

  // BackgroundAnimatorDelegate overrides:
  virtual void UpdateBackground(int alpha) OVERRIDE;

 private:
  ShelfWidget* shelf_;
  internal::FocusCycler* focus_cycler_;
  int alpha_;
  bool dimmed_;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

ShelfWidget::DelegateView::DelegateView(ShelfWidget* shelf)
    : shelf_(shelf),
      focus_cycler_(NULL),
      alpha_(0),
      dimmed_(false) {
}

ShelfWidget::DelegateView::~DelegateView() {
}

void ShelfWidget::DelegateView::OnPaintBackground(gfx::Canvas* canvas) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::ImageSkia launcher_background =
      *rb.GetImageSkiaNamed(IDR_AURA_LAUNCHER_BACKGROUND);
  if (SHELF_ALIGNMENT_BOTTOM != shelf_->GetAlignment())
    launcher_background = gfx::ImageSkiaOperations::CreateRotatedImage(
        launcher_background,
        shelf_->shelf_layout_manager()->SelectValueForShelfAlignment(
            SkBitmapOperations::ROTATION_90_CW,
            SkBitmapOperations::ROTATION_90_CW,
            SkBitmapOperations::ROTATION_270_CW,
            SkBitmapOperations::ROTATION_180_CW));

  gfx::Rect black_rect =
      shelf_->shelf_layout_manager()->SelectValueForShelfAlignment(
          gfx::Rect(0, height() - kNumBlackPixels, width(), kNumBlackPixels),
          gfx::Rect(0, 0, kNumBlackPixels, height()),
          gfx::Rect(width() - kNumBlackPixels, 0, kNumBlackPixels, height()),
          gfx::Rect(0, 0, width(), kNumBlackPixels));

  SkPaint paint;
  paint.setAlpha(alpha_);
  canvas->DrawImageInt(
      launcher_background,
      0, 0, launcher_background.width(), launcher_background.height(),
      0, 0, width(), height(),
      false,
      paint);
  canvas->FillRect(black_rect, SK_ColorBLACK);

  if (dimmed_) {
    gfx::ImageSkia background_image =
        *rb.GetImageSkiaNamed(IDR_AURA_LAUNCHER_DIMMING);
    if (SHELF_ALIGNMENT_BOTTOM != shelf_->GetAlignment())
      background_image = gfx::ImageSkiaOperations::CreateRotatedImage(
          background_image,
          shelf_->shelf_layout_manager()->SelectValueForShelfAlignment(
              SkBitmapOperations::ROTATION_90_CW,
              SkBitmapOperations::ROTATION_90_CW,
              SkBitmapOperations::ROTATION_270_CW,
              SkBitmapOperations::ROTATION_180_CW));

    SkPaint paint;
    paint.setAlpha(kDimAlpha);
    canvas->DrawImageInt(
        background_image,
        0, 0, background_image.width(), background_image.height(),
        0, 0, width(), height(),
        false,
        paint);
  }
}

bool ShelfWidget::DelegateView::CanActivate() const {
  // Allow to activate as fallback.
  if (shelf_->activating_as_fallback_)
    return true;
  // Allow to activate from the focus cycler.
  if (focus_cycler_ && focus_cycler_->widget_activating() == GetWidget())
    return true;
  // Disallow activating in other cases, especially when using mouse.
  return false;
}

void ShelfWidget::DelegateView::Layout() {
  for(int i = 0; i < child_count(); ++i) {
    if (shelf_->shelf_layout_manager()->IsHorizontalAlignment()) {
      child_at(i)->SetBounds(child_at(i)->x(), child_at(i)->y(),
                             child_at(i)->width(), height());
    } else {
      child_at(i)->SetBounds(child_at(i)->x(), child_at(i)->y(),
                             width(), child_at(i)->height());
    }
  }
}

void ShelfWidget::DelegateView::UpdateBackground(int alpha) {
  alpha_ = alpha;
  SchedulePaint();
}

ShelfWidget::ShelfWidget(
    aura::Window* shelf_container,
    aura::Window* status_container,
    internal::WorkspaceController* workspace_controller) :
    launcher_(NULL),
    delegate_view_(new DelegateView(this)),
    background_animator_(delegate_view_, 0, kLauncherBackgroundAlpha),
    activating_as_fallback_(false),
    window_container_(shelf_container) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.transparent = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = shelf_container;
  params.delegate = delegate_view_;
  Init(params);

  // The shelf should not take focus when initially shown.
  set_focus_on_creation(false);
  SetContentsView(delegate_view_);

  status_area_widget_ = new internal::StatusAreaWidget(status_container);
  status_area_widget_->CreateTrayViews();
  if (Shell::GetInstance()->delegate()->IsSessionStarted())
    status_area_widget_->Show();
  Shell::GetInstance()->focus_cycler()->AddWidget(status_area_widget_);

  shelf_layout_manager_ = new internal::ShelfLayoutManager(this);
  shelf_container->SetLayoutManager(shelf_layout_manager_);
  shelf_layout_manager_->set_workspace_controller(workspace_controller);
  workspace_controller->SetShelf(shelf_layout_manager_);

  status_container->SetLayoutManager(
      new internal::StatusAreaLayoutManager(this));

  views::Widget::AddObserver(this);
}

ShelfWidget::~ShelfWidget() {
  RemoveObserver(this);
}

void ShelfWidget::SetPaintsBackground(
    bool value,
    internal::BackgroundAnimator::ChangeType change_type) {
  background_animator_.SetPaintsBackground(value, change_type);
}

ShelfAlignment ShelfWidget::GetAlignment() const {
  return shelf_layout_manager_->GetAlignment();
}

void ShelfWidget::SetAlignment(ShelfAlignment alignment) {
  shelf_layout_manager_->SetAlignment(alignment);
  shelf_layout_manager_->LayoutShelf();
  delegate_view_->SchedulePaint();
}

void ShelfWidget::SetDimsShelf(bool dimming) {
  delegate_view_->SetDimmed(dimming);
}

bool ShelfWidget::GetDimsShelf() const {
  return delegate_view_->GetDimmed();
}

void ShelfWidget::CreateLauncher() {
  if (!launcher_.get()) {
    Shell* shell = Shell::GetInstance();
    // This needs to be called before launcher_model().
    shell->GetLauncherDelegate();
    launcher_.reset(new Launcher(shell->launcher_model(),
                                 shell->GetLauncherDelegate(),
                                 this));

    SetFocusCycler(shell->focus_cycler());

    // Inform the root window controller.
    internal::RootWindowController::ForWindow(window_container_)->
        OnLauncherCreated();

    ShellDelegate* delegate = shell->delegate();
    if (delegate)
      launcher_->SetVisible(delegate->IsSessionStarted());

    Show();
  }
}

bool ShelfWidget::IsLauncherVisible() const {
  return launcher_.get() && launcher_->IsVisible();
}

void ShelfWidget::SetLauncherVisibility(bool visible) {
  if (launcher_.get())
    launcher_->SetVisible(visible);
}

void ShelfWidget::SetFocusCycler(internal::FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
  if (focus_cycler)
    focus_cycler->AddWidget(this);
}

internal::FocusCycler* ShelfWidget::GetFocusCycler() {
  return delegate_view_->focus_cycler();
}

void ShelfWidget::OnWidgetActivationChanged(views::Widget* widget,
                                            bool active) {
  activating_as_fallback_ = false;
  if (active)
    delegate_view_->SetPaneFocusAndFocusDefault();
  else
    delegate_view_->GetFocusManager()->ClearFocus();
}

void ShelfWidget::ShutdownStatusAreaWidget() {
  if (status_area_widget_)
    status_area_widget_->Shutdown();
  status_area_widget_ = NULL;
}

}  // namespace ash

