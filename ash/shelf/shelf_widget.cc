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
#include "ui/base/events/event_constants.h"
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
const int kDimAlpha = 128;

// The time to dim and un-dim.
const int kTimeToDimMs = 3000;  // Slow in dimming.
const int kTimeToUnDimMs = 200;  // Fast in activating.

// Class used to slightly dim shelf items when maximized and visible.
class DimmerView : public views::View,
                   public views::WidgetDelegate,
                   ash::internal::BackgroundAnimatorDelegate {
 public:
  // If |disable_dimming_animations_for_test| is set, all alpha animations will
  // be performed instantly.
  DimmerView(ash::ShelfWidget* shelf_widget,
             bool disable_dimming_animations_for_test);
  virtual ~DimmerView();

  // Called by |DimmerEventFilter| when the mouse |hovered| state changes.
  void SetHovered(bool hovered);

  // Force the dimmer to be undimmed.
  void ForceUndimming(bool force);

  // views::WidgetDelegate overrides:
  virtual views::Widget* GetWidget() OVERRIDE {
    return View::GetWidget();
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return View::GetWidget();
  }

  // ash::internal::BackgroundAnimatorDelegate overrides:
  virtual void UpdateBackground(int alpha) {
    alpha_ = alpha;
    SchedulePaint();
  }

  // views::View overrides:
  void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

  // A function to test the current alpha used.
  int get_dimming_alpha_for_test() { return alpha_; }

 private:
  // This class monitors mouse events to see if it is on top of the launcher.
  class DimmerEventFilter : public ui::EventHandler {
   public:
    explicit DimmerEventFilter(DimmerView* owner);
    ~DimmerEventFilter();

    // Overridden from ui::EventHandler:
    virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
    virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

   private:
    // The owning class.
    DimmerView* owner_;

    // TRUE if the mouse is inside the shelf.
    bool mouse_inside_;

    // TRUE if a touch event is inside the shelf.
    bool touch_inside_;

    DISALLOW_COPY_AND_ASSIGN(DimmerEventFilter);
  };

  // The owning shelf.
  ash::ShelfWidget* shelf_;

  // The alpha to use for covering the shelf.
  int alpha_;

  // True if the event filter claims that we should not be dimmed.
  bool is_hovered_;

  // True if someone forces us not to be dimmed (e.g. a menu is open).
  bool force_hovered_;

  // True if animations should be suppressed for a test.
  bool disable_dimming_animations_for_test_;

  // The animator for the background transitions.
  ash::internal::BackgroundAnimator background_animator_;

  // Notification of entering / exiting of the shelf area by mouse.
  scoped_ptr<DimmerEventFilter> event_filter_;

  DISALLOW_COPY_AND_ASSIGN(DimmerView);
};

DimmerView::DimmerView(ash::ShelfWidget* shelf_widget,
                       bool disable_dimming_animations_for_test)
    : shelf_(shelf_widget),
      alpha_(kDimAlpha),
      is_hovered_(false),
      force_hovered_(false),
      disable_dimming_animations_for_test_(disable_dimming_animations_for_test),
      background_animator_(this, 0, kDimAlpha) {
  event_filter_.reset(new DimmerEventFilter(this));
  // Make sure it is undimmed at the beginning and then fire off the dimming
  // animation.
  background_animator_.SetPaintsBackground(false,
      ash::internal::BackgroundAnimator::CHANGE_IMMEDIATE);
  SetHovered(false);
}

DimmerView::~DimmerView() {
}

void DimmerView::SetHovered(bool hovered) {
  // Remember the hovered state so that we can correct the state once a
  // possible force state has disappeared.
  is_hovered_ = hovered;
  // Undimm also if we were forced to by e.g. an open menu.
  hovered |= force_hovered_;
  background_animator_.SetDuration(hovered ? kTimeToUnDimMs : kTimeToDimMs);
  background_animator_.SetPaintsBackground(!hovered,
      disable_dimming_animations_for_test_ ?
          ash::internal::BackgroundAnimator::CHANGE_IMMEDIATE :
          ash::internal::BackgroundAnimator::CHANGE_ANIMATE);
}

void DimmerView::ForceUndimming(bool force) {
  bool previous = force_hovered_;
  force_hovered_ = force;
  // If the forced change does change the result we apply the change.
  if (is_hovered_ || force_hovered_ != is_hovered_ || previous)
    SetHovered(is_hovered_);
}

void DimmerView::OnPaintBackground(gfx::Canvas* canvas) {
  SkPaint paint;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::ImageSkia launcher_background =
      *rb.GetImageNamed(IDR_AURA_LAUNCHER_DIMMING).ToImageSkia();

  if (shelf_->GetAlignment() != ash::SHELF_ALIGNMENT_BOTTOM) {
    launcher_background = gfx::ImageSkiaOperations::CreateRotatedImage(
        launcher_background,
        shelf_->shelf_layout_manager()->SelectValueForShelfAlignment(
            SkBitmapOperations::ROTATION_90_CW,
            SkBitmapOperations::ROTATION_90_CW,
            SkBitmapOperations::ROTATION_270_CW,
            SkBitmapOperations::ROTATION_180_CW));
  }
  paint.setAlpha(alpha_);
  canvas->DrawImageInt(
      launcher_background,
      0, 0, launcher_background.width(), launcher_background.height(),
      0, 0, width(), height(),
      false,
      paint);
}

DimmerView::DimmerEventFilter::DimmerEventFilter(DimmerView* owner)
    : owner_(owner),
      mouse_inside_(false),
      touch_inside_(false) {
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

DimmerView::DimmerEventFilter::~DimmerEventFilter() {
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

void DimmerView::DimmerEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_MOVED &&
      event->type() != ui::ET_MOUSE_DRAGGED)
    return;
  bool inside = owner_->GetBoundsInScreen().Contains(event->root_location());
  if (mouse_inside_ || touch_inside_ != inside || touch_inside_)
    owner_->SetHovered(inside || touch_inside_);
  mouse_inside_ = inside;
}

void DimmerView::DimmerEventFilter::OnTouchEvent(ui::TouchEvent* event) {
  bool touch_inside = false;
  if (event->type() != ui::ET_TOUCH_RELEASED &&
      event->type() != ui::ET_TOUCH_CANCELLED)
    touch_inside = owner_->GetBoundsInScreen().Contains(event->root_location());

  if (mouse_inside_ || touch_inside_ != mouse_inside_ || touch_inside)
    owner_->SetHovered(mouse_inside_ || touch_inside);
  touch_inside_ = touch_inside;
}

}  // namespace

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
  void SetDimmed(bool dimmed);
  bool GetDimmed() const;

  // Set the bounds of the widget.
  void SetWidgetBounds(const gfx::Rect bounds);

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

  // Force the shelf to be presented in an undimmed state.
  void ForceUndimming(bool force);

  // A function to test the current alpha used by the dimming bar. If there is
  // no dimmer active, the function will return -1.
  int GetDimmingAlphaForTest();

  // Disable dimming animations for running tests. This needs to be called
  // prior to the creation of of the |dimmer_|.
  void disable_dimming_animations_for_test() {
    disable_dimming_animations_for_test_ = true;
  }

 private:
  ShelfWidget* shelf_;
  scoped_ptr<views::Widget> dimmer_;
  internal::FocusCycler* focus_cycler_;
  int alpha_;

  // The view which does the dimming.
  DimmerView* dimmer_view_;

  // True if dimming animations should be turned off.
  bool disable_dimming_animations_for_test_;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

ShelfWidget::DelegateView::DelegateView(ShelfWidget* shelf)
    : shelf_(shelf),
      focus_cycler_(NULL),
      alpha_(0),
      dimmer_view_(NULL),
      disable_dimming_animations_for_test_(false) {
}

ShelfWidget::DelegateView::~DelegateView() {
}

void ShelfWidget::DelegateView::SetDimmed(bool value) {
  if (value == (dimmer_.get() != NULL))
    return;

  if (value) {
    dimmer_.reset(new views::Widget);
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.transparent = true;
    params.can_activate = false;
    params.accept_events = false;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = shelf_->GetNativeView();
    dimmer_->Init(params);
    dimmer_->GetNativeWindow()->SetName("ShelfDimmer");
    dimmer_->SetBounds(shelf_->GetWindowBoundsInScreen());
    // The launcher should not take focus when it is initially shown.
    dimmer_->set_focus_on_creation(false);
    dimmer_view_ = new DimmerView(shelf_, disable_dimming_animations_for_test_);
    dimmer_->SetContentsView(dimmer_view_);
    dimmer_->GetNativeView()->SetName("ShelfDimmerView");
    dimmer_->Show();
  } else {
    dimmer_view_ = NULL;
    dimmer_.reset(NULL);
  }
}

bool ShelfWidget::DelegateView::GetDimmed() const {
  return dimmer_.get() && dimmer_->IsVisible();
}

void ShelfWidget::DelegateView::SetWidgetBounds(const gfx::Rect bounds) {
  if (dimmer_.get())
    dimmer_->SetBounds(bounds);
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

void ShelfWidget::DelegateView::ForceUndimming(bool force) {
  if (GetDimmed())
    dimmer_view_->ForceUndimming(force);
}

int ShelfWidget::DelegateView::GetDimmingAlphaForTest() {
  if (GetDimmed())
    return dimmer_view_->get_dimming_alpha_for_test();
  return -1;
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

void ShelfWidget::ShutdownStatusAreaWidget() {
  if (status_area_widget_)
    status_area_widget_->Shutdown();
  status_area_widget_ = NULL;
}

void ShelfWidget::SetWidgetBounds(const gfx::Rect& rect) {
  Widget::SetBounds(rect);
  delegate_view_->SetWidgetBounds(rect);
}

void ShelfWidget::ForceUndimming(bool force) {
  delegate_view_->ForceUndimming(force);
}

void ShelfWidget::OnWidgetActivationChanged(views::Widget* widget,
                                            bool active) {
  activating_as_fallback_ = false;
  if (active)
    delegate_view_->SetPaneFocusAndFocusDefault();
  else
    delegate_view_->GetFocusManager()->ClearFocus();
}

int ShelfWidget::GetDimmingAlphaForTest() {
  if (delegate_view_)
    return delegate_view_->GetDimmingAlphaForTest();
  return -1;
}

void ShelfWidget::DisableDimmingAnimationsForTest() {
  DCHECK(delegate_view_);
  return delegate_view_->disable_dimming_animations_for_test();
}

}  // namespace ash

