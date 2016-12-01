// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_widget.h"

#include "ash/common/focus_cycler.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/app_list_button.h"
#include "ash/common/shelf/shelf_background_animator_observer.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/shelf/shelf_layout_manager.h"
#include "ash/common/shelf/shelf_view.h"
#include "ash/common/shelf/wm_dimmer_view.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/status_area_layout_manager.h"
#include "ash/common/system/status_area_widget.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "base/memory/ptr_util.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Size of black border at bottom (or side) of shelf.
const int kNumBlackPixels = 3;

}  // namespace

// The contents view of the Shelf. This view contains ShelfView and
// sizes it to the width of the shelf minus the size of the status area.
class ShelfWidget::DelegateView : public views::WidgetDelegate,
                                  public views::AccessiblePaneView,
                                  public ShelfBackgroundAnimatorObserver {
 public:
  DelegateView(WmShelf* wm_shelf, ShelfWidget* shelf);
  ~DelegateView() override;

  void set_focus_cycler(FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }
  FocusCycler* focus_cycler() { return focus_cycler_; }

  ui::Layer* opaque_background() { return &opaque_background_; }
  ui::Layer* opaque_foreground() { return &opaque_foreground_; }

  // Set if the shelf area is dimmed (eg when a window is maximized).
  void SetDimmed(bool dimmed);
  bool GetDimmed() const;

  void SetParentLayer(ui::Layer* layer);

  // views::View overrides:
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // views::WidgetDelegateView overrides:
  views::Widget* GetWidget() override { return View::GetWidget(); }
  const views::Widget* GetWidget() const override { return View::GetWidget(); }

  bool CanActivate() const override;
  void ReorderChildLayers(ui::Layer* parent_layer) override;
  // This will be called when the parent local bounds change.
  void OnBoundsChanged(const gfx::Rect& old_bounds) override;

  // ShelfBackgroundAnimatorObserver:
  void UpdateShelfOpaqueBackground(int alpha) override;
  void UpdateShelfAssetBackground(int alpha) override;

  // Force the shelf to be presented in an undimmed state.
  void ForceUndimming(bool force);

  // A function to test the current alpha used by the dimming bar. If there is
  // no dimmer active, the function will return -1.
  int GetDimmingAlphaForTest();

  // A function to test the bounds of the dimming bar. Returns gfx::Rect() if
  // the dimmer is inactive.
  gfx::Rect GetDimmerBoundsForTest();

  // Disable dimming animations for running tests. This needs to be called
  // prior to the creation of of the dimmer.
  void disable_dimming_animations_for_test() {
    disable_dimming_animations_for_test_ = true;
  }

 private:
  WmShelf* wm_shelf_;
  ShelfWidget* shelf_widget_;
  FocusCycler* focus_cycler_;
  int asset_background_alpha_;
  // TODO(bruthig): Remove opaque_background_ (see https://crbug.com/621551).
  // A black background layer which is shown when a maximized window is visible.
  ui::Layer opaque_background_;
  // A black foreground layer which is shown while transitioning between users.
  // Note: Since the back- and foreground layers have different functions they
  // can be used simultaneously - so no repurposing possible.
  ui::Layer opaque_foreground_;

  // The interface for the view which does the dimming. Null if the shelf is not
  // being dimmed, or if dimming is not supported (e.g. for mus).
  WmDimmerView* dimmer_view_;

  // True if dimming animations should be turned off.
  bool disable_dimming_animations_for_test_;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

ShelfWidget::DelegateView::DelegateView(WmShelf* wm_shelf,
                                        ShelfWidget* shelf_widget)
    : wm_shelf_(wm_shelf),
      shelf_widget_(shelf_widget),
      focus_cycler_(nullptr),
      asset_background_alpha_(0),
      opaque_background_(ui::LAYER_SOLID_COLOR),
      opaque_foreground_(ui::LAYER_SOLID_COLOR),
      dimmer_view_(nullptr),
      disable_dimming_animations_for_test_(false) {
  DCHECK(wm_shelf_);
  DCHECK(shelf_widget_);
  SetLayoutManager(new views::FillLayout());
  set_allow_deactivate_on_esc(true);
  opaque_background_.SetColor(SK_ColorBLACK);
  opaque_background_.SetBounds(GetLocalBounds());
  opaque_background_.SetOpacity(0.0f);
  opaque_foreground_.SetColor(SK_ColorBLACK);
  opaque_foreground_.SetBounds(GetLocalBounds());
  opaque_foreground_.SetOpacity(0.0f);
}

ShelfWidget::DelegateView::~DelegateView() {
  // Make sure that the dimmer goes away since it might have set an observer.
  SetDimmed(false);
}

void ShelfWidget::DelegateView::SetDimmed(bool dimmed) {
  // When starting dimming, attempt to create a dimmer view.
  if (dimmed) {
    if (!dimmer_view_) {
      dimmer_view_ =
          wm_shelf_->CreateDimmerView(disable_dimming_animations_for_test_);
    }
    return;
  }

  // Close the dimmer widget when stopping dimming.
  if (dimmer_view_) {
    dimmer_view_->GetDimmerWidget()->CloseNow();
    dimmer_view_ = nullptr;
  }
}

bool ShelfWidget::DelegateView::GetDimmed() const {
  return dimmer_view_ && dimmer_view_->GetDimmerWidget()->IsVisible();
}

void ShelfWidget::DelegateView::SetParentLayer(ui::Layer* layer) {
  layer->Add(&opaque_background_);
  layer->Add(&opaque_foreground_);
  ReorderLayers();
}

void ShelfWidget::DelegateView::OnPaintBackground(gfx::Canvas* canvas) {
  if (MaterialDesignController::IsShelfMaterial())
    return;

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia shelf_background =
      *rb->GetImageSkiaNamed(IDR_ASH_SHELF_BACKGROUND);
  const bool horizontal = wm_shelf_->IsHorizontalAlignment();
  if (!horizontal) {
    shelf_background = gfx::ImageSkiaOperations::CreateRotatedImage(
        shelf_background, wm_shelf_->GetAlignment() == SHELF_ALIGNMENT_LEFT
                              ? SkBitmapOperations::ROTATION_90_CW
                              : SkBitmapOperations::ROTATION_270_CW);
  }
  const gfx::Rect dock_bounds(
      shelf_widget_->shelf_layout_manager()->dock_bounds());
  SkPaint paint;
  paint.setAlpha(asset_background_alpha_);
  canvas->DrawImageInt(
      shelf_background, 0, 0, shelf_background.width(),
      shelf_background.height(),
      (horizontal && dock_bounds.x() == 0 && dock_bounds.width() > 0)
          ? dock_bounds.width()
          : 0,
      0, horizontal ? width() - dock_bounds.width() : width(), height(), false,
      paint);
  if (horizontal && dock_bounds.width() > 0) {
    // The part of the shelf background that is in the corner below the docked
    // windows close to the work area is an arched gradient that blends
    // vertically oriented docked background and horizontal shelf.
    gfx::ImageSkia shelf_corner = *rb->GetImageSkiaNamed(IDR_ASH_SHELF_CORNER);
    if (dock_bounds.x() == 0) {
      shelf_corner = gfx::ImageSkiaOperations::CreateRotatedImage(
          shelf_corner, SkBitmapOperations::ROTATION_90_CW);
    }
    canvas->DrawImageInt(
        shelf_corner, 0, 0, shelf_corner.width(), shelf_corner.height(),
        dock_bounds.x() > 0 ? dock_bounds.x() : dock_bounds.width() - height(),
        0, height(), height(), false, paint);
    // The part of the shelf background that is just below the docked windows
    // is drawn using the last (lowest) 1-pixel tall strip of the image asset.
    // This avoids showing the border 3D shadow between the shelf and the
    // dock.
    canvas->DrawImageInt(shelf_background, 0, shelf_background.height() - 1,
                         shelf_background.width(), 1,
                         dock_bounds.x() > 0 ? dock_bounds.x() + height() : 0,
                         0, dock_bounds.width() - height(), height(), false,
                         paint);
  }
  gfx::Rect black_rect =
      shelf_widget_->shelf_layout_manager()->SelectValueForShelfAlignment(
          gfx::Rect(0, height() - kNumBlackPixels, width(), kNumBlackPixels),
          gfx::Rect(0, 0, kNumBlackPixels, height()),
          gfx::Rect(width() - kNumBlackPixels, 0, kNumBlackPixels, height()));
  canvas->FillRect(black_rect, SK_ColorBLACK);
}

bool ShelfWidget::DelegateView::CanActivate() const {
  // Allow to activate as fallback.
  if (shelf_widget_->activating_as_fallback_)
    return true;
  // Allow to activate from the focus cycler.
  if (focus_cycler_ && focus_cycler_->widget_activating() == GetWidget())
    return true;
  // Disallow activating in other cases, especially when using mouse.
  return false;
}

void ShelfWidget::DelegateView::ReorderChildLayers(ui::Layer* parent_layer) {
  views::View::ReorderChildLayers(parent_layer);
  parent_layer->StackAtBottom(&opaque_background_);
  parent_layer->StackAtTop(&opaque_foreground_);
}

void ShelfWidget::DelegateView::OnBoundsChanged(const gfx::Rect& old_bounds) {
  opaque_background_.SetBounds(GetLocalBounds());
  opaque_foreground_.SetBounds(GetLocalBounds());
  if (dimmer_view_)
    dimmer_view_->GetDimmerWidget()->SetBounds(GetBoundsInScreen());
}

void ShelfWidget::DelegateView::ForceUndimming(bool force) {
  if (GetDimmed())
    dimmer_view_->ForceUndimming(force);
}

int ShelfWidget::DelegateView::GetDimmingAlphaForTest() {
  if (GetDimmed())
    return dimmer_view_->GetDimmingAlphaForTest();
  return -1;
}

gfx::Rect ShelfWidget::DelegateView::GetDimmerBoundsForTest() {
  if (GetDimmed())
    return dimmer_view_->GetDimmerWidget()->GetWindowBoundsInScreen();
  return gfx::Rect();
}

void ShelfWidget::DelegateView::UpdateShelfOpaqueBackground(int alpha) {
  const float kMaxAlpha = 255.0f;
  opaque_background_.SetOpacity(alpha / kMaxAlpha);
}

void ShelfWidget::DelegateView::UpdateShelfAssetBackground(int alpha) {
  asset_background_alpha_ = alpha;
  SchedulePaint();
}

ShelfWidget::ShelfWidget(WmWindow* shelf_container, WmShelf* wm_shelf)
    : wm_shelf_(wm_shelf),
      status_area_widget_(nullptr),
      delegate_view_(new DelegateView(wm_shelf, this)),
      shelf_view_(nullptr),
      background_animator_(SHELF_BACKGROUND_DEFAULT, wm_shelf_),
      activating_as_fallback_(false) {
  background_animator_.AddObserver(this);
  background_animator_.AddObserver(delegate_view_);

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "ShelfWidget";
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = delegate_view_;
  shelf_container->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          this, shelf_container->GetShellWindowId(), &params);
  Init(params);

  // The shelf should not take focus when initially shown.
  set_focus_on_creation(false);
  SetContentsView(delegate_view_);
  delegate_view_->SetParentLayer(GetLayer());

  shelf_layout_manager_ = new ShelfLayoutManager(this, wm_shelf_);
  shelf_layout_manager_->AddObserver(this);
  shelf_container->SetLayoutManager(base::WrapUnique(shelf_layout_manager_));
  background_animator_.PaintBackground(
      shelf_layout_manager_->GetShelfBackgroundType(),
      BACKGROUND_CHANGE_IMMEDIATE);

  views::Widget::AddObserver(this);
}

ShelfWidget::~ShelfWidget() {
  // Must call Shutdown() before destruction.
  DCHECK(!status_area_widget_);
  WmShell::Get()->focus_cycler()->RemoveWidget(this);
  SetFocusCycler(nullptr);
  RemoveObserver(this);
  background_animator_.RemoveObserver(delegate_view_);
  background_animator_.RemoveObserver(this);
}

void ShelfWidget::CreateStatusAreaWidget(WmWindow* status_container) {
  DCHECK(status_container);
  DCHECK(!status_area_widget_);
  // TODO(jamescook): Move ownership to RootWindowController.
  status_area_widget_ = new StatusAreaWidget(status_container, wm_shelf_);
  status_area_widget_->CreateTrayViews();
  if (WmShell::Get()->GetSessionStateDelegate()->IsActiveUserSessionStarted())
    status_area_widget_->Show();
  WmShell::Get()->focus_cycler()->AddWidget(status_area_widget_);
  background_animator_.AddObserver(status_area_widget_);
  status_container->SetLayoutManager(
      base::MakeUnique<StatusAreaLayoutManager>(this));
}

void ShelfWidget::SetPaintsBackground(
    ShelfBackgroundType background_type,
    BackgroundAnimatorChangeType change_type) {
  background_animator_.PaintBackground(background_type, change_type);
}

ShelfBackgroundType ShelfWidget::GetBackgroundType() const {
  return background_animator_.target_background_type();
}

void ShelfWidget::HideShelfBehindBlackBar(bool hide, int animation_time_ms) {
  if (IsShelfHiddenBehindBlackBar() == hide)
    return;

  ui::Layer* opaque_foreground = delegate_view_->opaque_foreground();
  float target_opacity = hide ? 1.0f : 0.0f;
  std::unique_ptr<ui::ScopedLayerAnimationSettings> opaque_foreground_animation;
  opaque_foreground_animation.reset(
      new ui::ScopedLayerAnimationSettings(opaque_foreground->GetAnimator()));
  opaque_foreground_animation->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(animation_time_ms));
  opaque_foreground_animation->SetPreemptionStrategy(
      ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);

  opaque_foreground->SetOpacity(target_opacity);
}

bool ShelfWidget::IsShelfHiddenBehindBlackBar() const {
  return delegate_view_->opaque_foreground()->GetTargetOpacity() != 0.0f;
}

ShelfAlignment ShelfWidget::GetAlignment() const {
  return wm_shelf_->GetAlignment();
}

void ShelfWidget::OnShelfAlignmentChanged() {
  shelf_view_->OnShelfAlignmentChanged();
  status_area_widget_->SetShelfAlignment(GetAlignment());
  delegate_view_->SchedulePaint();
}

void ShelfWidget::SetDimsShelf(bool dimming) {
  delegate_view_->SetDimmed(dimming);
  // Repaint all children, allowing updates to reflect dimmed state eg:
  // status area background, app list button and overflow button.
  if (shelf_view_)
    shelf_view_->SchedulePaintForAllButtons();
  status_area_widget_->SchedulePaint();
}

bool ShelfWidget::GetDimsShelf() const {
  return delegate_view_->GetDimmed();
}

ShelfView* ShelfWidget::CreateShelfView() {
  DCHECK(!shelf_view_);

  shelf_view_ =
      new ShelfView(WmShell::Get()->shelf_model(),
                    WmShell::Get()->shelf_delegate(), wm_shelf_, this);
  shelf_view_->Init();
  GetContentsView()->AddChildView(shelf_view_);
  return shelf_view_;
}

void ShelfWidget::PostCreateShelf() {
  SetFocusCycler(WmShell::Get()->focus_cycler());

  // Ensure the newly created |shelf_| gets current values.
  background_animator_.Initialize(this);

  shelf_view_->SetVisible(
      WmShell::Get()->GetSessionStateDelegate()->IsActiveUserSessionStarted());
  shelf_layout_manager_->LayoutShelf();
  shelf_layout_manager_->UpdateAutoHideState();
  Show();
}

bool ShelfWidget::IsShelfVisible() const {
  return shelf_view_ && shelf_view_->visible();
}

void ShelfWidget::SetShelfVisibility(bool visible) {
  if (shelf_view_)
    shelf_view_->SetVisible(visible);
}

bool ShelfWidget::IsShowingAppList() const {
  return GetAppListButton() && GetAppListButton()->is_showing_app_list();
}

bool ShelfWidget::IsShowingContextMenu() const {
  return shelf_view_ && shelf_view_->IsShowingMenu();
}

bool ShelfWidget::IsShowingOverflowBubble() const {
  return shelf_view_ && shelf_view_->IsShowingOverflowBubble();
}

void ShelfWidget::SetFocusCycler(FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
  if (focus_cycler)
    focus_cycler->AddWidget(this);
}

FocusCycler* ShelfWidget::GetFocusCycler() {
  return delegate_view_->focus_cycler();
}

void ShelfWidget::Shutdown() {
  // Tear down the dimmer before |shelf_layout_manager_|, since the dimmer uses
  // |shelf_layout_manager_| to get the shelf's WmWindow, via WmShelf.
  delegate_view_->SetDimmed(false);

  // Shutting down the status area widget may cause some widgets (e.g. bubbles)
  // to close, so uninstall the ShelfLayoutManager event filters first. Don't
  // reset the pointer until later because other widgets (e.g. app list) may
  // access it later in shutdown.
  if (shelf_layout_manager_)
    shelf_layout_manager_->PrepareForShutdown();

  if (status_area_widget_) {
    background_animator_.RemoveObserver(status_area_widget_);
    WmShell::Get()->focus_cycler()->RemoveWidget(status_area_widget_);
    status_area_widget_->Shutdown();
    status_area_widget_ = nullptr;
  }

  CloseNow();
}

void ShelfWidget::ForceUndimming(bool force) {
  delegate_view_->ForceUndimming(force);
}

void ShelfWidget::UpdateIconPositionForPanel(WmWindow* panel) {
  WmWindow* shelf_window = WmLookup::Get()->GetWindowForWidget(this);
  shelf_view_->UpdatePanelIconPosition(
      panel->GetIntProperty(WmWindowProperty::SHELF_ID),
      shelf_window->ConvertRectFromScreen(panel->GetBoundsInScreen())
          .CenterPoint());
}

gfx::Rect ShelfWidget::GetScreenBoundsOfItemIconForWindow(WmWindow* window) {
  ShelfID id = window->GetIntProperty(WmWindowProperty::SHELF_ID);
  gfx::Rect bounds(shelf_view_->GetIdealBoundsOfItemIcon(id));
  gfx::Point screen_origin;
  views::View::ConvertPointToScreen(shelf_view_, &screen_origin);
  return gfx::Rect(screen_origin.x() + bounds.x(),
                   screen_origin.y() + bounds.y(), bounds.width(),
                   bounds.height());
}

AppListButton* ShelfWidget::GetAppListButton() const {
  return shelf_view_ ? shelf_view_->GetAppListButton() : nullptr;
}

app_list::ApplicationDragAndDropHost*
ShelfWidget::GetDragAndDropHostForAppList() {
  return shelf_view_;
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

gfx::Rect ShelfWidget::GetDimmerBoundsForTest() {
  if (delegate_view_)
    return delegate_view_->GetDimmerBoundsForTest();
  return gfx::Rect();
}

void ShelfWidget::DisableDimmingAnimationsForTest() {
  DCHECK(delegate_view_);
  delegate_view_->disable_dimming_animations_for_test();
}

void ShelfWidget::UpdateShelfItemBackground(int alpha) {
  if (shelf_view_)
    shelf_view_->UpdateShelfItemBackground(alpha);
}

void ShelfWidget::WillDeleteShelfLayoutManager() {
  shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = nullptr;
}

}  // namespace ash
