// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item.h"

#include <algorithm>
#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/shell.h"
#include "ash/wm/overview/caption_container_view.h"
#include "ash/wm/overview/overview_animation_type.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/overview/start_animation_observer.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "base/auto_reset.h"
#include "base/metrics/user_metrics.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

// Opacity for fading out during closing a window.
constexpr float kClosingItemOpacity = 0.8f;

// Opacity for the item header.
constexpr float kHeaderOpacity = 0.1f;

// Before closing a window animate both the window and the caption to shrink by
// this fraction of size.
constexpr float kPreCloseScale = 0.02f;

constexpr int kShadowElevation = 16;

// The amount of translation an item animates by when it is closed by using
// swipe to close.
constexpr int kSwipeToCloseCloseTranslationDp = 96;

// Before dragging an overview window, the window will be scaled up
// |kPreDragScale| to indicate its selection.
constexpr float kDragWindowScale = 0.04f;

}  // namespace

// The class to cache render surface to the specified window's layer.
class OverviewItem::WindowSurfaceCacheObserver : public aura::WindowObserver {
 public:
  explicit WindowSurfaceCacheObserver(aura::Window* window) {
    StartObserving(window);
  }

  ~WindowSurfaceCacheObserver() override { StopObserving(); }

  void StartObserving(aura::Window* window) {
    // If we're already observing a window, stop observing it first.
    StopObserving();

    window_ = window;
    window_->AddObserver(this);
    for (auto* window : wm::GetTransientTreeIterator(window_))
      window->layer()->AddCacheRenderSurfaceRequest();
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override { StopObserving(); }

 private:
  void StopObserving() {
    if (window_) {
      for (auto* window : wm::GetTransientTreeIterator(window_))
        window->layer()->RemoveCacheRenderSurfaceRequest();
      window_->RemoveObserver(this);
      window_ = nullptr;
    }
  }

  aura::Window* window_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(WindowSurfaceCacheObserver);
};

OverviewItem::OverviewItem(aura::Window* window,
                           OverviewSession* overview_session,
                           OverviewGrid* overview_grid)
    : root_window_(window->GetRootWindow()),
      transform_window_(this, window),
      overview_session_(overview_session),
      overview_grid_(overview_grid) {
  CreateWindowLabel();
  for (auto* window_iter : wm::GetTransientTreeIterator(GetWindow()))
    window_iter->AddObserver(this);
  GetWindow()->SetProperty(ash::kIsShowingInOverviewKey, true);
}

OverviewItem::~OverviewItem() {
  for (auto* window_iter : wm::GetTransientTreeIterator(GetWindow()))
    window_iter->RemoveObserver(this);
  GetWindow()->ClearProperty(ash::kIsShowingInOverviewKey);
}

aura::Window* OverviewItem::GetWindow() {
  return transform_window_.window();
}

aura::Window* OverviewItem::GetWindowForStacking() {
  // If the window is minimized, stack |widget_window| above the minimized
  // window, otherwise the minimized window will cover |widget_window|. The
  // minimized is created with the same parent as the original window, just
  // like |item_widget_|, so there is no need to reparent.
  return transform_window_.minimized_widget()
             ? transform_window_.minimized_widget()->GetNativeWindow()
             : GetWindow();
}

bool OverviewItem::Contains(const aura::Window* target) const {
  return transform_window_.Contains(target);
}

void OverviewItem::RestoreWindow(bool reset_transform) {
  caption_container_view_->ResetListener();
  transform_window_.RestoreWindow(
      reset_transform, overview_session_->enter_exit_overview_type());
}

void OverviewItem::EnsureVisible() {
  transform_window_.EnsureVisible();
}

void OverviewItem::Shutdown() {
  // On swiping from the shelf, the caller handles the animation via calls to
  // UpdateYAndOpacity, so do not additional fade out or slide animation to the
  // window.
  if (overview_session_->enter_exit_overview_type() ==
      OverviewSession::EnterExitOverviewType::kSwipeFromShelf) {
    return;
  }

  // TODO(sammiequon|xdai): Close the item widget without animation to reduce
  // the load during exit animation.
  item_widget_.reset();
}

void OverviewItem::PrepareForOverview() {
  transform_window_.PrepareForOverview();
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  widget_window->parent()->StackChildBelow(widget_window,
                                           GetWindowForStacking());
}

void OverviewItem::SlideWindowIn() {
  // |transform_window_|'s |minimized_widget| is non null because this only gets
  // called if we see the home launcher on enter (all windows are minimized).
  DCHECK(transform_window_.minimized_widget());
  // The |item_widget_| and mask and shadow will be shown when animation ends.
  // Update the mask after starting the animation since starting the animation
  // lets the controller know we are in starting animation.
  FadeInWidgetAndMaybeSlideOnEnter(transform_window_.minimized_widget(),
                                   OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER,
                                   /*slide=*/true);
  UpdateMaskAndShadow();
}

void OverviewItem::UpdateYPositionAndOpacity(
    int new_grid_y,
    float opacity,
    OverviewSession::UpdateAnimationSettingsCallback callback) {
  // Animate |item_widget_| and the window itself.
  // TODO(sammiequon): Investigate if we can combine with
  // FadeInWidgetAndMaybeSlideOnEnter. Also when animating we should remove
  // shadow and rounded corners.
  std::vector<std::pair<ui::Layer*, int>> animation_layers_and_offsets = {
      {item_widget_->GetNativeWindow()->layer(), 0}};

  // Transient children may already have a y translation relative to their base
  // ancestor, so factor that in when computing their new y translation.
  base::Optional<int> base_window_y_translation = base::nullopt;
  for (auto* window : wm::GetTransientTreeIterator(GetWindowForStacking())) {
    if (!base_window_y_translation.has_value()) {
      base_window_y_translation = base::make_optional(
          window->layer()->transform().To2dTranslation().y());
    }
    const int offset = *base_window_y_translation -
                       window->layer()->transform().To2dTranslation().y();
    animation_layers_and_offsets.push_back({window->layer(), offset});
  }

  for (auto& layer_and_offset : animation_layers_and_offsets) {
    ui::Layer* layer = layer_and_offset.first;
    std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
    if (!callback.is_null()) {
      settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          layer->GetAnimator());
      callback.Run(settings.get(), /*observe=*/false);
    }
    layer->SetOpacity(opacity);

    // Alter the y-translation. Offset by the window location relative to the
    // grid.
    const int offset = target_bounds_.y() + kHeaderHeightDp + kWindowMargin -
                       layer_and_offset.second;
    gfx::Transform transform = layer->transform();
    transform.matrix().setFloat(1, 3, static_cast<float>(offset + new_grid_y));
    layer->SetTransform(transform);
  }
}

void OverviewItem::UpdateItemContentViewForMinimizedWindow() {
  transform_window_.UpdateMinimizedWidget();
}

float OverviewItem::GetItemScale(const gfx::Size& size) {
  gfx::SizeF inset_size(size.width(), size.height() - 2 * kWindowMargin);
  return ScopedOverviewTransformWindow::GetItemScale(
      GetTargetBoundsInScreen().size(), inset_size,
      transform_window_.GetTopInset(), kHeaderHeightDp);
}

gfx::RectF OverviewItem::GetTargetBoundsInScreen() const {
  return ::ash::GetTargetBoundsInScreen(transform_window_.GetOverviewWindow());
}

gfx::RectF OverviewItem::GetTransformedBounds() const {
  return transform_window_.GetTransformedBounds();
}

void OverviewItem::SetBounds(const gfx::RectF& target_bounds,
                             OverviewAnimationType animation_type) {
  if (in_bounds_update_)
    return;

  // Do not animate if the resulting bounds does not change. The original
  // window may change bounds so we still need to call SetItemBounds to update
  // the window transform.
  OverviewAnimationType new_animation_type = animation_type;
  if (target_bounds == target_bounds_)
    new_animation_type = OVERVIEW_ANIMATION_NONE;

  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  // If |target_bounds_| is empty, this is the first update. Let
  // UpdateHeaderLayout know, as we do not want |item_widget_| to be animated
  // with the window.
  const bool is_first_update = target_bounds_.IsEmpty();
  target_bounds_ = target_bounds;

  gfx::RectF inset_bounds(target_bounds);
  inset_bounds.Inset(kWindowMargin, kWindowMargin);

  // Do not animate if entering when the window is minimized, as it will be
  // faded in. We still want to animate if the position is changed after
  // entering.
  if (wm::GetWindowState(GetWindow())->IsMinimized() && is_first_update)
    new_animation_type = OVERVIEW_ANIMATION_NONE;

  // SetItemBounds is called before UpdateHeaderLayout so the header can
  // properly use the updated windows bounds.
  SetItemBounds(inset_bounds, new_animation_type);
  UpdateHeaderLayout(is_first_update ? OVERVIEW_ANIMATION_NONE
                                     : new_animation_type);

  // Shadow is normally set after an animation is finished. In the case of no
  // animations, manually set the shadow. Shadow relies on both the window
  // transform and |item_widget_|'s new bounds so set it after SetItemBounds
  // and UpdateHeaderLayout. Do not apply the shadow for drop target.
  if (new_animation_type == OVERVIEW_ANIMATION_NONE)
    UpdateMaskAndShadow();
}

void OverviewItem::SendAccessibleSelectionEvent() {
  caption_container_view_->GetListenerButton()->NotifyAccessibilityEvent(
      ax::mojom::Event::kSelection, true);
}

void OverviewItem::AnimateAndCloseWindow(bool up) {
  base::RecordAction(base::UserMetricsAction("WindowSelector_SwipeToClose"));

  animating_to_close_ = true;
  overview_session_->PositionWindows(/*animate=*/true);
  caption_container_view_->ResetListener();

  int translation_y = kSwipeToCloseCloseTranslationDp * (up ? -1 : 1);
  gfx::Transform transform;
  transform.Translate(gfx::Vector2d(0, translation_y));

  auto animate_window = [this](aura::Window* window,
                               const gfx::Transform& transform, bool observe) {
    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM, window);
    gfx::Transform original_transform = window->transform();
    original_transform.ConcatTransform(transform);
    window->SetTransform(original_transform);
    if (observe)
      settings.AddObserver(this);
  };

  AnimateOpacity(0.0, OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM);
  animate_window(item_widget_->GetNativeWindow(), transform, false);
  animate_window(GetWindowForStacking(), transform, true);
}

void OverviewItem::CloseWindow() {
  gfx::RectF inset_bounds(target_bounds_);
  inset_bounds.Inset(target_bounds_.width() * kPreCloseScale,
                     target_bounds_.height() * kPreCloseScale);
  // Scale down both the window and label.
  SetBounds(inset_bounds, OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM);
  // First animate opacity to an intermediate value concurrently with the
  // scaling animation.
  AnimateOpacity(kClosingItemOpacity, OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM);

  // Fade out the window and the label, effectively hiding them.
  AnimateOpacity(0.0, OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM);
  transform_window_.Close();
}

void OverviewItem::OnMinimizedStateChanged() {
  transform_window_.UpdateMirrorWindowForMinimizedState();
  if (window_surface_cache_observers_)
    window_surface_cache_observers_->StartObserving(GetWindowForStacking());
}

void OverviewItem::UpdateCannotSnapWarningVisibility() {
  // Windows which can snap will never show this warning. Or if the window is
  // the drop target window, also do not show this warning.
  if (CanSnapInSplitview(GetWindow()) ||
      overview_grid_->IsDropTargetWindow(GetWindow())) {
    caption_container_view_->SetCannotSnapLabelVisibility(false);
    return;
  }

  const SplitViewController::State state =
      Shell::Get()->split_view_controller()->state();
  const bool visible = state == SplitViewController::LEFT_SNAPPED ||
                       state == SplitViewController::RIGHT_SNAPPED;
  caption_container_view_->SetCannotSnapLabelVisibility(visible);
}

void OverviewItem::OnSelectorItemDragStarted(OverviewItem* item) {
  is_being_dragged_ = (item == this);
  // Disable mask and shadow for the dragged overview item during dragging.
  if (is_being_dragged_)
    UpdateMaskAndShadow();

  caption_container_view_->SetHeaderVisibility(
      is_being_dragged_
          ? CaptionContainerView::HeaderVisibility::kInvisible
          : CaptionContainerView::HeaderVisibility::kCloseButtonInvisibleOnly);

  // Start caching render surface during overview window dragging.
  window_surface_cache_observers_ =
      std::make_unique<WindowSurfaceCacheObserver>(GetWindowForStacking());
}

void OverviewItem::OnSelectorItemDragEnded() {
  // Re-show mask and shadow for the dragged overview item after drag ends.
  if (is_being_dragged_) {
    is_being_dragged_ = false;
    UpdateMaskAndShadow();
  }

  caption_container_view_->SetHeaderVisibility(
      CaptionContainerView::HeaderVisibility::kVisible);

  // Stop caching render surface after overview window dragging.
  window_surface_cache_observers_.reset();
}

ScopedOverviewTransformWindow::GridWindowFillMode
OverviewItem::GetWindowDimensionsType() const {
  return transform_window_.type();
}

void OverviewItem::UpdateWindowDimensionsType() {
  transform_window_.UpdateWindowDimensionsType();
  const bool show_backdrop =
      GetWindowDimensionsType() !=
      ScopedOverviewTransformWindow::GridWindowFillMode::kNormal;
  caption_container_view_->SetBackdropVisibility(show_backdrop);
}

gfx::Rect OverviewItem::GetBoundsOfSelectedItem() {
  gfx::RectF original_bounds = target_bounds();
  ScaleUpSelectedItem(OVERVIEW_ANIMATION_NONE);
  gfx::RectF selected_bounds = transform_window_.GetTransformedBounds();
  SetBounds(original_bounds, OVERVIEW_ANIMATION_NONE);
  return gfx::ToEnclosedRect(selected_bounds);
}

void OverviewItem::ScaleUpSelectedItem(OverviewAnimationType animation_type) {
  gfx::RectF scaled_bounds = target_bounds();
  scaled_bounds.Inset(-scaled_bounds.width() * kDragWindowScale,
                      -scaled_bounds.height() * kDragWindowScale);
  SetBounds(scaled_bounds, animation_type);
}

void OverviewItem::HandlePressEvent(const gfx::PointF& location_in_screen) {
  // We allow switching finger while dragging, but do not allow dragging two or
  // more items.
  if (overview_session_->window_drag_controller() &&
      overview_session_->window_drag_controller()->item()) {
    return;
  }

  StartDrag();
  overview_session_->InitiateDrag(this, location_in_screen);
}

void OverviewItem::HandleReleaseEvent(const gfx::PointF& location_in_screen) {
  if (!IsDragItem())
    return;

  overview_grid_->SetSelectionWidgetVisibility(true);
  overview_session_->CompleteDrag(this, location_in_screen);
}

void OverviewItem::HandleDragEvent(const gfx::PointF& location_in_screen) {
  if (!IsDragItem())
    return;

  overview_session_->Drag(this, location_in_screen);
}

void OverviewItem::HandleLongPressEvent(const gfx::PointF& location_in_screen) {
  if (!ShouldAllowSplitView())
    return;

  overview_session_->StartSplitViewDragMode(location_in_screen);
}

void OverviewItem::HandleFlingStartEvent(const gfx::PointF& location_in_screen,
                                         float velocity_x,
                                         float velocity_y) {
  overview_session_->Fling(this, location_in_screen, velocity_x, velocity_y);
}

void OverviewItem::ActivateDraggedWindow() {
  if (!IsDragItem())
    return;

  overview_session_->ActivateDraggedWindow();
}

void OverviewItem::ResetDraggedWindowGesture() {
  if (!IsDragItem())
    return;

  OnSelectorItemDragEnded();
  overview_session_->ResetDraggedWindowGesture();
}

bool OverviewItem::IsDragItem() {
  return overview_session_->window_drag_controller() &&
         overview_session_->window_drag_controller()->item() == this;
}

void OverviewItem::OnDragAnimationCompleted() {
  // This is function is called whenever the grid repositions its windows, but
  // we only need to restack the windows if an item was being dragged around
  // and then released.
  if (!should_restack_on_animation_end_)
    return;

  should_restack_on_animation_end_ = false;

  // First stack this item's window below the snapped window if split view mode
  // is active.
  aura::Window* dragged_window = GetWindowForStacking();
  aura::Window* dragged_widget_window = item_widget_->GetNativeWindow();
  aura::Window* parent_window = dragged_widget_window->parent();
  if (Shell::Get()->IsSplitViewModeActive()) {
    aura::Window* snapped_window =
        Shell::Get()->split_view_controller()->GetDefaultSnappedWindow();
    if (snapped_window->parent() == parent_window &&
        dragged_window->parent() == parent_window) {
      parent_window->StackChildBelow(dragged_window, snapped_window);
      parent_window->StackChildBelow(dragged_widget_window, dragged_window);
    }
  }

  // Then find the window which was stacked right above this overview item's
  // window before dragging and stack this overview item's window below it.
  const std::vector<std::unique_ptr<OverviewItem>>& overview_items =
      overview_grid_->window_list();
  aura::Window* stacking_target = nullptr;
  for (size_t index = 0; index < overview_items.size(); index++) {
    if (index > 0) {
      aura::Window* window = overview_items[index - 1].get()->GetWindow();
      if (window->parent() == parent_window &&
          dragged_window->parent() == parent_window) {
        stacking_target = window;
      }
    }
    if (overview_items[index].get() == this && stacking_target) {
      parent_window->StackChildBelow(dragged_window, stacking_target);
      parent_window->StackChildBelow(dragged_widget_window, dragged_window);
      break;
    }
  }
}

void OverviewItem::SetShadowBounds(base::Optional<gfx::Rect> bounds_in_screen) {
  // Shadow is normally turned off during animations and reapplied when they
  // are finished. On destruction, |shadow_| is cleaned up before
  // |transform_window_|, which may call this function, so early exit if
  // |shadow_| is nullptr.
  if (!shadow_)
    return;

  if (bounds_in_screen == base::nullopt) {
    shadow_->layer()->SetVisible(false);
    return;
  }

  shadow_->layer()->SetVisible(true);
  gfx::Rect bounds_in_item =
      gfx::Rect(item_widget_->GetNativeWindow()->GetTargetBounds().size());
  bounds_in_item.Inset(kOverviewMargin, kOverviewMargin);
  bounds_in_item.Inset(0, kHeaderHeightDp, 0, 0);
  bounds_in_item.ClampToCenteredSize(bounds_in_screen.value().size());

  shadow_->SetContentBounds(bounds_in_item);
}

void OverviewItem::UpdateMaskAndShadow() {
  // Do not show mask and shadow if:
  // 1) overview is shutting down or
  // 2) this overview item is in an overview grid that contains more than 10
  //    windows. In this case don't apply rounded corner mask because it can
  //    push the compositor memory usage to the limit. TODO(oshima): Remove
  //    this once new rounded corner impl is available. (crbug.com/903486)
  // 3) we're currently in entering overview animation or
  // 4) this overview item is being dragged or
  // 5) this overview item is the drop target window or
  // 6) this overview item is in animation.
  bool should_show = true;
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (disable_mask_ || !overview_controller ||
      !overview_controller->IsSelecting() ||
      overview_grid_->window_list().size() > 10 ||
      overview_controller->IsInStartAnimation() || is_being_dragged_ ||
      overview_grid_->IsDropTargetWindow(GetWindow()) ||
      transform_window_.GetOverviewWindow()
          ->layer()
          ->GetAnimator()
          ->is_animating()) {
    should_show = false;
  }

  if (!should_show) {
    transform_window_.UpdateMask(false);
    SetShadowBounds(base::nullopt);
    return;
  }

  transform_window_.UpdateMask(true);
  SetShadowBounds(
      gfx::ToEnclosedRect(transform_window_.GetTransformedBounds()));
}

void OverviewItem::OnStartingAnimationComplete() {
  DCHECK(item_widget_.get());
  FadeInWidgetAndMaybeSlideOnEnter(
      item_widget_.get(), OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
      /*slide=*/false);
  const bool show_backdrop =
      GetWindowDimensionsType() !=
      ScopedOverviewTransformWindow::GridWindowFillMode::kNormal;
  caption_container_view_->SetBackdropVisibility(show_backdrop);
}

void OverviewItem::SetOpacity(float opacity) {
  item_widget_->SetOpacity(opacity);
  transform_window_.SetOpacity(opacity);
}

float OverviewItem::GetOpacity() {
  return item_widget_->GetNativeWindow()->layer()->opacity();
}

OverviewAnimationType OverviewItem::GetExitOverviewAnimationType() {
  return should_animate_when_exiting_
             ? OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_EXIT
             : OVERVIEW_ANIMATION_NONE;
}

OverviewAnimationType OverviewItem::GetExitTransformAnimationType() {
  return should_animate_when_exiting_ ? OVERVIEW_ANIMATION_RESTORE_WINDOW
                                      : OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO;
}

void OverviewItem::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  if (IsSlidingOutOverviewFromShelf())
    return;

  if (sender == caption_container_view_->GetCloseButton()) {
    base::RecordAction(
        base::UserMetricsAction("WindowSelector_OverviewCloseButton"));
    if (Shell::Get()
            ->tablet_mode_controller()
            ->IsTabletModeWindowManagerEnabled()) {
      base::RecordAction(
          base::UserMetricsAction("Tablet_WindowCloseFromOverviewButton"));
    }
    CloseWindow();
    return;
  }

  CHECK_EQ(sender, caption_container_view_->GetListenerButton());

  // For other cases, the event is handled in OverviewWindowDragController.
  if (!ShouldAllowSplitView())
    overview_session_->SelectWindow(this);
}

void OverviewItem::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds,
                                         ui::PropertyChangeReason reason) {
  if (reason == ui::PropertyChangeReason::NOT_FROM_ANIMATION) {
    if (window == GetWindow()) {
      transform_window_.ResizeMinimizedWidgetIfNeeded();
    } else {
      // Transient window is repositioned. The new position within the overview
      // item needs to be recomputed.
      SetBounds(target_bounds_, OVERVIEW_ANIMATION_NONE);
    }
  }
}

void OverviewItem::OnWindowDestroying(aura::Window* window) {
  // Stops observing the window and all of its transient descendents.
  for (auto* window_iter : wm::WindowTransientDescendantIteratorRange(
           wm::WindowTransientDescendantIterator(window))) {
    window_iter->RemoveObserver(this);
  }
  if (window != GetWindow())
    return;
  transform_window_.OnWindowDestroyed();

  if (is_being_dragged_) {
    Shell::Get()->overview_controller()->UnpauseOcclusionTracker(
        kOcclusionPauseDurationForDragMs);
  }
}

void OverviewItem::OnWindowTitleChanged(aura::Window* window) {
  if (window != GetWindow())
    return;
  // TODO(flackr): Maybe add the new title to a vector of titles so that we can
  // filter any of the titles the window had while in the overview session.
  caption_container_view_->SetTitle(window->GetTitle());
}

void OverviewItem::OnImplicitAnimationsCompleted() {
  transform_window_.Close();
}

float OverviewItem::GetCloseButtonVisibilityForTesting() const {
  return caption_container_view_->GetCloseButton()->layer()->opacity();
}

float OverviewItem::GetTitlebarOpacityForTesting() const {
  return caption_container_view_->header_view()->layer()->opacity();
}

gfx::Rect OverviewItem::GetShadowBoundsForTesting() {
  if (!shadow_ || !shadow_->layer()->visible())
    return gfx::Rect();

  return shadow_->content_bounds();
}

void OverviewItem::SetItemBounds(const gfx::RectF& target_bounds,
                                 OverviewAnimationType animation_type) {
  aura::Window* window = GetWindow();
  DCHECK(root_window_ == window->GetRootWindow());
  gfx::RectF screen_rect = gfx::RectF(GetTargetBoundsInScreen());

  // Avoid division by zero by ensuring screen bounds is not empty.
  gfx::SizeF screen_size(screen_rect.size());
  screen_size.SetToMax(gfx::SizeF(1.f, 1.f));
  screen_rect.set_size(screen_size);

  const int top_view_inset = transform_window_.GetTopInset();
  gfx::RectF overview_item_bounds =
      transform_window_.ShrinkRectToFitPreservingAspectRatio(
          screen_rect, target_bounds, top_view_inset, kHeaderHeightDp);
  // Do not set transform for drop target, set bounds instead.
  if (overview_grid_->IsDropTargetWindow(window)) {
    window->layer()->SetBounds(gfx::ToEnclosedRect(overview_item_bounds));
    transform_window_.GetOverviewWindow()->SetTransform(gfx::Transform());
    return;
  }

  gfx::Transform transform = ScopedOverviewTransformWindow::GetTransformForRect(
      screen_rect, overview_item_bounds);
  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  SetTransform(transform_window_.GetOverviewWindow(), transform);
}

void OverviewItem::CreateWindowLabel() {
  views::Widget::InitParams params_label;
  params_label.type = views::Widget::InitParams::TYPE_POPUP;
  params_label.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params_label.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params_label.visible_on_all_workspaces = true;
  params_label.layer_type = ui::LAYER_NOT_DRAWN;
  params_label.name = "OverviewModeLabel";
  params_label.activatable =
      views::Widget::InitParams::Activatable::ACTIVATABLE_DEFAULT;
  params_label.accept_events = true;
  params_label.parent = transform_window_.window()->parent();

  item_widget_ = std::make_unique<views::Widget>();
  item_widget_->set_focus_on_creation(false);
  item_widget_->Init(params_label);
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  widget_window->parent()->StackChildBelow(widget_window,
                                           transform_window_.window());

  shadow_ = std::make_unique<ui::Shadow>();
  shadow_->Init(kShadowElevation);
  item_widget_->GetLayer()->Add(shadow_->layer());

  caption_container_view_ = new CaptionContainerView(this, GetWindow());
  UpdateCannotSnapWarningVisibility();
  item_widget_->SetContentsView(caption_container_view_);
  item_widget_->Show();
  item_widget_->SetOpacity(0);
  item_widget_->GetLayer()->SetMasksToBounds(false);
}

void OverviewItem::UpdateHeaderLayout(OverviewAnimationType animation_type) {
  gfx::RectF transformed_window_bounds =
      transform_window_.overview_bounds().value_or(
          transform_window_.GetTransformedBounds());
  ::wm::TranslateRectFromScreen(root_window_, &transformed_window_bounds);

  gfx::Rect label_rect(kHeaderHeightDp, kHeaderHeightDp);
  label_rect.set_width(transformed_window_bounds.width());
  // For tabbed windows the initial bounds of the caption are set such that it
  // appears to be "growing" up from the window content area.
  label_rect.set_y(-label_rect.height());

  aura::Window* widget_window = item_widget_->GetNativeWindow();
  ScopedOverviewAnimationSettings animation_settings(animation_type,
                                                     widget_window);

  // Create a start animation observer if this is an enter overview layout
  // animation.
  if (animation_type == OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER) {
    auto start_observer = std::make_unique<StartAnimationObserver>();
    animation_settings.AddObserver(start_observer.get());
    Shell::Get()->overview_controller()->AddStartAnimationObserver(
        std::move(start_observer));
  }

  // |widget_window| covers both the transformed window and the header
  // as well as the gap between the windows to prevent events from reaching
  // the window including its sizing borders.
  label_rect.set_height(kHeaderHeightDp + transformed_window_bounds.height());
  label_rect.Inset(-kOverviewMargin, -kOverviewMargin);
  widget_window->SetBounds(label_rect);
  gfx::Transform label_transform;
  label_transform.Translate(gfx::ToRoundedInt(transformed_window_bounds.x()),
                            gfx::ToRoundedInt(transformed_window_bounds.y()));
  widget_window->SetTransform(label_transform);
}

void OverviewItem::AnimateOpacity(float opacity,
                                  OverviewAnimationType animation_type) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  transform_window_.SetOpacity(opacity);

  const float header_opacity = selected_ ? 0.f : kHeaderOpacity * opacity;
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  ScopedOverviewAnimationSettings animation_settings_label(animation_type,
                                                           widget_window);
  widget_window->layer()->SetOpacity(header_opacity);
}

void OverviewItem::StartDrag() {
  overview_grid_->SetSelectionWidgetVisibility(false);

  // |transform_window_| handles hiding shadow and rounded edges mask while
  // animating, and applies them after animation is complete. Prevent the
  // shadow and rounded edges mask from showing up after dragging in the case
  // the window is pressed while still animating.
  transform_window_.CancelAnimationsListener();

  aura::Window* widget_window = item_widget_->GetNativeWindow();
  aura::Window* window = GetWindowForStacking();
  if (widget_window && widget_window->parent() == window->parent()) {
    // TODO(xdai): This might not work if there is an always on top window.
    // See crbug.com/733760.
    widget_window->parent()->StackChildAtTop(window);
    widget_window->parent()->StackChildBelow(widget_window, window);
  }
}

}  // namespace ash
