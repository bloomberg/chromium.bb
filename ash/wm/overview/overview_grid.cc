// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_grid.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/overview/cleanup_animation_observer.h"
#include "ash/wm/overview/drop_target_view.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_delegate.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/i18n/string_search.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

// The color and opacity of the overview selector.
constexpr SkColor kWindowSelectionColor = SkColorSetARGB(36, 255, 255, 255);

// Corner radius and shadow applied to the overview selector border.
constexpr int kWindowSelectionRadius = 9;
constexpr int kWindowSelectionShadowElevation = 24;

// Windows are not allowed to get taller than this.
constexpr int kMaxHeight = 512;

// Margins reserved in the overview mode.
constexpr float kOverviewInsetRatio = 0.05f;

// Additional vertical inset reserved for windows in overview mode.
constexpr float kOverviewVerticalInset = 0.1f;

// Values for the no items indicator which appears when opening overview mode
// with no opened windows.
constexpr int kNoItemsIndicatorHeightDp = 32;
constexpr int kNoItemsIndicatorHorizontalPaddingDp = 16;
constexpr int kNoItemsIndicatorRoundingDp = 16;
constexpr int kNoItemsIndicatorVerticalPaddingDp = 8;
constexpr SkColor kNoItemsIndicatorBackgroundColor = SK_ColorBLACK;
constexpr SkColor kNoItemsIndicatorTextColor = SK_ColorWHITE;
constexpr float kNoItemsIndicatorBackgroundOpacity = 0.8f;

// Returns the vector for the fade in animation.
gfx::Vector2d GetSlideVectorForFadeIn(OverviewSession::Direction direction,
                                      const gfx::Rect& bounds) {
  gfx::Vector2d vector;
  switch (direction) {
    case OverviewSession::UP:
    case OverviewSession::LEFT:
      vector.set_x(-bounds.width());
      break;
    case OverviewSession::DOWN:
    case OverviewSession::RIGHT:
      vector.set_x(bounds.width());
      break;
  }
  return vector;
}

// Creates |drop_target_widget_|. It's created when a window (not from overview)
// is dragged around and destroyed when the drag ends. If |animate| is true, do
// the opacity animation for the drop target.
std::unique_ptr<views::Widget> CreateDropTargetWidget(
    aura::Window* dragged_window,
    bool animate) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  params.parent = dragged_window->parent();
  params.bounds = dragged_window->bounds();
  auto widget = std::make_unique<views::Widget>();
  widget->set_focus_on_creation(false);
  widget->Init(params);

  // Show plus icon if drag a tab from a multi-tab window.
  widget->SetContentsView(new DropTargetView(
      dragged_window->GetProperty(ash::kTabDraggingSourceWindowKey)));
  widget->Show();

  if (animate) {
    widget->SetOpacity(0.f);
    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_DROP_TARGET_FADE_IN, widget->GetNativeWindow());
    widget->SetOpacity(1.f);
  } else {
    widget->SetOpacity(1.f);
  }
  return widget;
}

// Gets the expected grid bounds according the current |indicator_state| during
// window dragging.
gfx::Rect GetGridBoundsInScreenDuringDragging(aura::Window* dragged_window,
                                              IndicatorState indicator_state) {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  switch (indicator_state) {
    case IndicatorState::kPreviewAreaLeft:
      return split_view_controller->GetSnappedWindowBoundsInScreen(
          dragged_window, SplitViewController::RIGHT);
    case IndicatorState::kPreviewAreaRight:
      return split_view_controller->GetSnappedWindowBoundsInScreen(
          dragged_window, SplitViewController::LEFT);
    default:
      return screen_util::GetDisplayWorkAreaBoundsInScreenForDefaultContainer(
          dragged_window);
  }
}

// Gets the expected grid bounds according to current splitview state.
gfx::Rect GetGridBoundsInScreenAfterDragging(aura::Window* dragged_window) {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  switch (split_view_controller->state()) {
    case SplitViewController::LEFT_SNAPPED:
      return split_view_controller->GetSnappedWindowBoundsInScreen(
          dragged_window, SplitViewController::RIGHT);
    case SplitViewController::RIGHT_SNAPPED:
      return split_view_controller->GetSnappedWindowBoundsInScreen(
          dragged_window, SplitViewController::LEFT);
    default:
      return screen_util::GetDisplayWorkAreaBoundsInScreenForDefaultContainer(
          dragged_window);
  }
}

}  // namespace

// ShieldView contains the background for overview mode. It also contains text
// which is shown if there are no windows to be displayed.
// This view also takes care of disabling overview mode on:
//   - Gesture tap.
//   - Mouse release.
class OverviewGrid::ShieldView : public views::View {
 public:
  ShieldView() {
    label_ = new views::Label(
        l10n_util::GetStringUTF16(IDS_ASH_OVERVIEW_NO_RECENT_ITEMS),
        views::style::CONTEXT_LABEL);
    label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    label_->SetEnabledColor(kNoItemsIndicatorTextColor);
    label_->SetBackgroundColor(kNoItemsIndicatorBackgroundColor);

    // |label_container_| is the parent of |label_| which allows the text to
    // have padding and rounded edges.
    label_container_ = new RoundedRectView(kNoItemsIndicatorRoundingDp,
                                           kNoItemsIndicatorBackgroundColor);
    label_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical,
        gfx::Insets(kNoItemsIndicatorVerticalPaddingDp,
                    kNoItemsIndicatorHorizontalPaddingDp)));
    label_container_->AddChildView(label_);
    label_container_->SetPaintToLayer();
    label_container_->layer()->SetFillsBoundsOpaquely(false);
    label_container_->layer()->SetOpacity(kNoItemsIndicatorBackgroundOpacity);
    label_container_->SetVisible(false);

    AddChildView(label_container_);
  }

  ~ShieldView() override = default;

  const DesksBarView* desks_bar_view() const { return desks_bar_view_; }

  // Call this only after this view has been added to a widget. This is needed
  // because the desks mini views need to access the widget to get the
  // root window in order to know how to layout themselves.
  void MaybeInitVirtualDesksBar() {
    if (!features::IsVirtualDesksEnabled())
      return;

    desks_bar_view_ = new DesksBarView;
    AddChildView(desks_bar_view_);
    desks_bar_view_->Init();
  }

  void SetLabelVisibility(bool visible) {
    label_container_->SetVisible(visible);
  }

  gfx::Rect GetLabelBounds() const {
    return label_container_->GetBoundsInScreen();
  }

  // ShieldView takes up the whole workspace since it changes opacity of the
  // whole wallpaper. The bounds of the grid may be smaller in some cases of
  // splitview. The label should be centered in the bounds of the grid.
  void SetGridBounds(const gfx::Rect& bounds) {
    const int label_width = label_->GetPreferredSize().width() +
                            2 * kNoItemsIndicatorHorizontalPaddingDp;
    gfx::Rect label_container_bounds = bounds;
    label_container_bounds.ClampToCenteredSize(
        gfx::Size(label_width, kNoItemsIndicatorHeightDp));
    label_container_->SetBoundsRect(label_container_bounds);

    UpdateDesksBarBounds();
  }

  bool IsLabelVisible() const { return label_container_->visible(); }

 protected:
  // views::View:
  void Layout() override {
    UpdateDesksBarBounds();
  }

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_PRESSED) {
      // In order to receive subsequent mouse release events in this view, we
      // must mark the event as handled in this view.
      event->SetHandled();
      return;
    }

    HandleClickReleaseOrTap(event);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    HandleClickReleaseOrTap(event);
  }

  void HandleClickReleaseOrTap(ui::Event* event) {
    if (event->type() != ui::ET_MOUSE_RELEASED &&
        event->type() != ui::ET_GESTURE_TAP) {
      return;
    }

    OverviewController* controller = Shell::Get()->overview_controller();
    if (!controller->IsSelecting())
      return;

    // Events that happen while app list is sliding out during overview should
    // be ignored to prevent overview from disappearing out from under the user.
    if (!IsSlidingOutOverviewFromShelf())
      controller->ToggleOverview();

    event->StopPropagation();
  }

  void UpdateDesksBarBounds() {
    if (!desks_bar_view_)
      return;

    // TODO: Make the ShieldView's bounds match the overview grid bounds rather
    // than the entire screen?
    const auto bar_bounds =
        gfx::Rect{bounds().width(), DesksBarView::GetBarHeight()};
    desks_bar_view_->SetBoundsRect(bar_bounds);
  }

  // Owned by views heirarchy.
  RoundedRectView* label_container_ = nullptr;
  views::Label* label_ = nullptr;
  DesksBarView* desks_bar_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ShieldView);
};

// The class to observe the overview window that the dragged tabs will merge
// into. After the dragged tabs merge into the overview window, and if the
// overview window represents a minimized window, we need to update the
// overview minimized widget's content view so that it reflects the merge.
class OverviewGrid::TargetWindowObserver : public aura::WindowObserver {
 public:
  TargetWindowObserver() = default;
  ~TargetWindowObserver() override { StopObserving(); }

  void StartObserving(aura::Window* window) {
    if (target_window_)
      StopObserving();

    target_window_ = window;
    target_window_->AddObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK_EQ(window, target_window_);
    // When the property is cleared, the dragged window should have been merged
    // into |target_window_|, update the corresponding window item in overview.
    if (key == ash::kIsDeferredTabDraggingTargetWindowKey &&
        !window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey)) {
      UpdateWindowItemInOverviewContaining(window);
      StopObserving();
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window, target_window_);
    StopObserving();
  }

 private:
  void UpdateWindowItemInOverviewContaining(aura::Window* window) {
    OverviewController* overview_controller =
        Shell::Get()->overview_controller();
    if (!overview_controller->IsSelecting())
      return;

    OverviewGrid* grid =
        overview_controller->overview_session()->GetGridWithRootWindow(
            window->GetRootWindow());
    if (!grid)
      return;

    OverviewItem* item = grid->GetOverviewItemContaining(window);
    if (!item)
      return;

    item->UpdateItemContentViewForMinimizedWindow();
  }

  void StopObserving() {
    if (target_window_)
      target_window_->RemoveObserver(this);
    target_window_ = nullptr;
  }

  aura::Window* target_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TargetWindowObserver);
};

OverviewGrid::OverviewGrid(aura::Window* root_window,
                           const std::vector<aura::Window*>& windows,
                           OverviewSession* overview_session,
                           const gfx::Rect& bounds_in_screen)
    : root_window_(root_window),
      overview_session_(overview_session),
      window_observer_(this),
      window_state_observer_(this),
      bounds_(bounds_in_screen) {
  for (auto* window : windows) {
    if (window->GetRootWindow() != root_window)
      continue;

    // Stop ongoing animations before entering overview mode. Because we are
    // deferring SetTransform of the windows beneath the window covering the
    // available workspace, we need to set the correct transforms of these
    // windows before entering overview mode again in the
    // OnImplicitAnimationsCompleted() of the observer of the
    // available-workspace-covering window's animation.
    auto* animator = window->layer()->GetAnimator();
    if (animator->is_animating())
      window->layer()->GetAnimator()->StopAnimating();
    window_observer_.Add(window);
    window_state_observer_.Add(wm::GetWindowState(window));
    window_list_.push_back(
        std::make_unique<OverviewItem>(window, overview_session_, this));
  }
}

OverviewGrid::~OverviewGrid() = default;

void OverviewGrid::Shutdown() {
  ScreenRotationAnimator::GetForRootWindow(root_window_)->RemoveObserver(this);

  for (const auto& window : window_list_)
    window->Shutdown();

  // Shutdown() implies |overview_session_| is about to be deleted, so reset it.
  auto exit_overview_session_type =
      overview_session_->enter_exit_overview_type();
  overview_session_ = nullptr;

  // HomeLauncherGestureHandler will handle fading/sliding |shield_widget_| in
  // this exit mode.
  if (exit_overview_session_type ==
      OverviewSession::EnterExitOverviewType::kSwipeFromShelf) {
    return;
  }

  if (shield_widget_) {
    // Fade out the shield widget. This animation continues past the lifetime
    // of |this|.
    FadeOutWidgetAndMaybeSlideOnExit(std::move(shield_widget_),
                                     OVERVIEW_ANIMATION_RESTORE_WINDOW);
  }
}

void OverviewGrid::PrepareForOverview() {
  if (!ShouldAnimateWallpaper())
    InitShieldWidget(/*animate=*/false);

  for (const auto& window : window_list_)
    window->PrepareForOverview();
  prepared_for_overview_ = true;
  if (Shell::Get()
          ->tablet_mode_controller()
          ->IsTabletModeWindowManagerEnabled()) {
    ScreenRotationAnimator::GetForRootWindow(root_window_)->AddObserver(this);
  }
}

void OverviewGrid::PositionWindows(
    bool animate,
    OverviewItem* ignored_item,
    OverviewSession::OverviewTransition transition) {
  if (!overview_session_ || suspend_reposition_)
    return;

  DCHECK_NE(transition, OverviewSession::OverviewTransition::kExit);
  // Keep the background shield widget covering the whole screen. A grid without
  // any windows still needs the shield widget bounds updated.
  if (shield_widget_) {
    aura::Window* widget_window = shield_widget_->GetNativeWindow();
    const gfx::Rect bounds = widget_window->parent()->bounds();
    widget_window->SetBounds(bounds);
    ShowNoRecentsWindowMessage(window_list_.empty());
  }

  if (window_list_.empty())
    return;

  std::vector<gfx::RectF> rects = GetWindowRects(ignored_item);

  // Position the windows centering the left-aligned rows vertically. Do not
  // position |ignored_item| if it is not nullptr and matches a item in
  // |window_list_|.
  OverviewAnimationType animation_type =
      transition == OverviewSession::OverviewTransition::kEnter
          ? OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER
          : OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW;
  for (size_t i = 0; i < window_list_.size(); ++i) {
    OverviewItem* window_item = window_list_[i].get();
    if (window_item->animating_to_close() ||
        (ignored_item != nullptr && window_item == ignored_item)) {
      continue;
    }

    // Calculate if each window item needs animation.
    bool should_animate_item = animate;
    // If we're in entering overview process, not all window items in the grid
    // might need animation even if the grid needs animation.
    if (animate && transition == OverviewSession::OverviewTransition::kEnter)
      should_animate_item = window_item->should_animate_when_entering();
    // Do not do the bounds animation for the drop target. We'll do the opacity
    // animation by ourselves.
    if (IsDropTargetWindow(window_item->GetWindow()))
      should_animate_item = false;

    window_item->SetBounds(rects[i], should_animate_item
                                         ? animation_type
                                         : OVERVIEW_ANIMATION_NONE);
  }

  // If the selection widget is active, reposition it without any animation.
  if (selection_widget_)
    MoveSelectionWidgetToTarget(animate);
}

bool OverviewGrid::Move(OverviewSession::Direction direction, bool animate) {
  if (empty())
    return true;

  bool recreate_selection_widget = false;
  bool out_of_bounds = false;
  bool changed_selection_index = false;
  gfx::RectF old_bounds;
  if (SelectedWindow()) {
    old_bounds = SelectedWindow()->target_bounds();
    // Make the old selected window header non-transparent first.
    SelectedWindow()->set_selected(false);
  }

  // [up] key is equivalent to [left] key and [down] key is equivalent to
  // [right] key.
  if (!selection_widget_) {
    switch (direction) {
      case OverviewSession::UP:
      case OverviewSession::LEFT:
        selected_index_ = window_list_.size() - 1;
        break;
      case OverviewSession::DOWN:
      case OverviewSession::RIGHT:
        selected_index_ = 0;
        break;
    }
    changed_selection_index = true;
  }
  while (!changed_selection_index) {
    switch (direction) {
      case OverviewSession::UP:
      case OverviewSession::LEFT:
        if (selected_index_ == 0)
          out_of_bounds = true;
        selected_index_--;
        break;
      case OverviewSession::DOWN:
      case OverviewSession::RIGHT:
        if (selected_index_ >= window_list_.size() - 1)
          out_of_bounds = true;
        selected_index_++;
        break;
    }
    if (!out_of_bounds && SelectedWindow()) {
      if (SelectedWindow()->target_bounds().y() != old_bounds.y())
        recreate_selection_widget = true;
    }
    changed_selection_index = true;
  }
  MoveSelectionWidget(direction, recreate_selection_widget, out_of_bounds,
                      animate);

  // Make the new selected window header fully transparent.
  if (SelectedWindow())
    SelectedWindow()->set_selected(true);
  return out_of_bounds;
}

OverviewItem* OverviewGrid::SelectedWindow() const {
  if (!selection_widget_)
    return nullptr;
  CHECK(selected_index_ < window_list_.size());
  return window_list_[selected_index_].get();
}

OverviewItem* OverviewGrid::GetOverviewItemContaining(
    const aura::Window* window) const {
  for (const auto& window_item : window_list_) {
    if (window_item && window_item->Contains(window))
      return window_item.get();
  }
  return nullptr;
}

void OverviewGrid::AddItem(aura::Window* window,
                           bool reposition,
                           bool animate) {
  DCHECK(!GetOverviewItemContaining(window));

  window_observer_.Add(window);
  window_state_observer_.Add(wm::GetWindowState(window));
  window_list_.insert(
      window_list_.begin(),
      std::make_unique<OverviewItem>(window, overview_session_, this));
  window_list_.front()->PrepareForOverview();
  // The item is added after overview enter animation is complete, so
  // just call OnStartingAnimationComplete.
  window_list_.front()->OnStartingAnimationComplete();

  if (reposition)
    PositionWindows(animate);
}

void OverviewGrid::RemoveItem(OverviewItem* overview_item, bool reposition) {
  auto iter = GetOverviewItemIterContainingWindow(overview_item->GetWindow());
  if (iter != window_list_.end()) {
    window_observer_.Remove(overview_item->GetWindow());
    window_state_observer_.Remove(
        wm::GetWindowState(overview_item->GetWindow()));
    // Erase from the list first because deleting OverviewItem can lead to
    // iterating through the |window_list_|.
    std::unique_ptr<OverviewItem> tmp = std::move(*iter);
    window_list_.erase(iter);
  }

  if (reposition)
    PositionWindows(/*animate=*/true);
}

void OverviewGrid::SetBoundsAndUpdatePositions(const gfx::Rect& bounds) {
  SetBoundsAndUpdatePositionsIgnoringWindow(bounds, nullptr);
}

void OverviewGrid::SetBoundsAndUpdatePositionsIgnoringWindow(
    const gfx::Rect& bounds,
    OverviewItem* ignored_item) {
  bounds_ = bounds;
  if (shield_view_)
    shield_view_->SetGridBounds(bounds_);
  PositionWindows(/*animate=*/true, ignored_item);
}

void OverviewGrid::SetSelectionWidgetVisibility(bool visible) {
  if (!selection_widget_)
    return;

  if (visible)
    selection_widget_->Show();
  else
    selection_widget_->Hide();
}

void OverviewGrid::ShowNoRecentsWindowMessage(bool visible) {
  // Only show the warning on the grid associated with primary root.
  if (root_window_ != Shell::GetPrimaryRootWindow())
    return;

  if (shield_view_)
    shield_view_->SetLabelVisibility(visible);
}

void OverviewGrid::UpdateCannotSnapWarningVisibility() {
  for (auto& overview_mode_item : window_list_)
    overview_mode_item->UpdateCannotSnapWarningVisibility();
}

void OverviewGrid::OnSelectorItemDragStarted(OverviewItem* item) {
  for (auto& overview_mode_item : window_list_)
    overview_mode_item->OnSelectorItemDragStarted(item);
}

void OverviewGrid::OnSelectorItemDragEnded() {
  for (auto& overview_mode_item : window_list_)
    overview_mode_item->OnSelectorItemDragEnded();
}

void OverviewGrid::OnWindowDragStarted(aura::Window* dragged_window,
                                       bool animate) {
  DCHECK_EQ(dragged_window->GetRootWindow(), root_window_);
  DCHECK(!drop_target_widget_);
  drop_target_widget_ = CreateDropTargetWidget(dragged_window, animate);
  overview_session_->AddItem(drop_target_widget_->GetNativeWindow(),
                             /*reposition=*/true, animate);

  // Stack the |dragged_window| at top during drag.
  dragged_window->parent()->StackChildAtTop(dragged_window);

  // Called to set caption and title visibility during dragging.
  OnSelectorItemDragStarted(/*item=*/nullptr);
}

void OverviewGrid::OnWindowDragContinued(aura::Window* dragged_window,
                                         const gfx::Point& location_in_screen,
                                         IndicatorState indicator_state) {
  DCHECK_EQ(dragged_window->GetRootWindow(), root_window_);

  // Adjust the window grid's bounds and the drop target's visibility
  // according to |indicator_state| if split view is not active at the moment.
  if (!Shell::Get()->split_view_controller()->IsSplitViewModeActive()) {
    OverviewItem* drop_target = GetDropTarget();
    const bool should_visible =
        (indicator_state != IndicatorState::kPreviewAreaLeft &&
         indicator_state != IndicatorState::kPreviewAreaRight);
    if (drop_target) {
      const bool visible = drop_target_widget_->IsVisible();
      if (should_visible != visible) {
        drop_target_widget_->GetLayer()->SetVisible(should_visible);
        drop_target->SetOpacity(should_visible ? 1.f : 0.f);
      }
    }

    // Update the grid's bounds.
    const gfx::Rect expected_bounds =
        GetGridBoundsInScreenDuringDragging(dragged_window, indicator_state);
    if (bounds_ != expected_bounds) {
      SetBoundsAndUpdatePositionsIgnoringWindow(
          expected_bounds, should_visible ? nullptr : drop_target);
    }
  }

  aura::Window* target_window = GetTargetWindowOnLocation(location_in_screen);
  DropTargetView* drop_target_view =
      static_cast<DropTargetView*>(drop_target_widget_->GetContentsView());
  DCHECK(drop_target_view);
  drop_target_view->UpdateBackgroundVisibility(
      target_window && IsDropTargetWindow(target_window));

  if (indicator_state == IndicatorState::kPreviewAreaLeft ||
      indicator_state == IndicatorState::kPreviewAreaRight) {
    // If the dragged window is currently dragged into preview window area,
    // clear the selection widget.
    if (SelectedWindow()) {
      SelectedWindow()->set_selected(false);
      selection_widget_.reset();
    }

    // Also clear ash::kIsDeferredTabDraggingTargetWindowKey key on the target
    // overview item so that it can't merge into this overview item if the
    // dragged window is currently in preview window area.
    if (target_window && !IsDropTargetWindow(target_window))
      target_window->ClearProperty(ash::kIsDeferredTabDraggingTargetWindowKey);

    return;
  }

  // Show the selection widget if |location_in_screen| is contained by the
  // browser windows' overview item in overview.
  if (target_window &&
      target_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey)) {
    size_t previous_selected_index = selected_index_;
    selected_index_ = GetOverviewItemIterContainingWindow(target_window) -
                      window_list_.begin();
    if (previous_selected_index == selected_index_ && selection_widget_)
      return;

    if (previous_selected_index != selected_index_)
      selection_widget_.reset();

    const OverviewSession::Direction direction =
        (selected_index_ - previous_selected_index > 0) ? OverviewSession::RIGHT
                                                        : OverviewSession::LEFT;
    MoveSelectionWidget(direction,
                        /*recreate_selection_widget=*/true,
                        /*out_of_bounds=*/false,
                        /*animate=*/false);
    return;
  }

  if (SelectedWindow()) {
    SelectedWindow()->set_selected(false);
    selection_widget_.reset();
  }
}

void OverviewGrid::OnWindowDragEnded(aura::Window* dragged_window,
                                     const gfx::Point& location_in_screen,
                                     bool should_drop_window_into_overview) {
  DCHECK_EQ(dragged_window->GetRootWindow(), root_window_);
  DCHECK(drop_target_widget_.get());

  // Add the dragged window into drop target in overview if
  // |should_drop_window_into_overview| is true. Only consider add the dragged
  // window into drop target if SelectedWindow is false since drop target will
  // not be selected and tab dragging might drag a tab window to merge it into a
  // browser window in overview.
  if (SelectedWindow()) {
    SelectedWindow()->set_selected(false);
    selection_widget_.reset();
  } else if (should_drop_window_into_overview) {
    AddDraggedWindowIntoOverviewOnDragEnd(dragged_window);
  }

  OverviewItem* drop_target_item =
      GetOverviewItemContaining(drop_target_widget_->GetNativeWindow());
  // TODO(http://crbug.com/916856): Disable tab and app dragging that doesn't
  // happen in the primary display.
  // The |drop_target_widget_| may not in the same display as
  // |dragged_window|, which will cause |drop_target_item| to be null.
  if (drop_target_item) {
    overview_session_->RemoveOverviewItem(drop_target_item,
                                          /*reposition=*/false);
  }
  drop_target_widget_.reset();

  // Called to reset caption and title visibility after dragging.
  OnSelectorItemDragEnded();

  // After drag ends, if the dragged window needs to merge into another window
  // |target_window|, and we may need to update |minimized_widget_| that holds
  // the contents of |target_window| if |target_window| is a minimized window
  // in overview.
  aura::Window* target_window = GetTargetWindowOnLocation(location_in_screen);
  if (target_window &&
      target_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey)) {
    // Create an window observer and update the minimized window widget after
    // the dragged window merges into |target_window|.
    if (!target_window_observer_)
      target_window_observer_ = std::make_unique<TargetWindowObserver>();
    target_window_observer_->StartObserving(target_window);
  }

  // Update the grid bounds and reposition windows. Since the grid bounds might
  // be updated based on the preview area during drag, but the window finally
  // didn't be snapped to the preview area.
  SetBoundsAndUpdatePositions(
      GetGridBoundsInScreenAfterDragging(dragged_window));
}

bool OverviewGrid::IsDropTargetWindow(aura::Window* window) const {
  return drop_target_widget_ &&
         drop_target_widget_->GetNativeWindow() == window;
}

OverviewItem* OverviewGrid::GetDropTarget() {
  if (!drop_target_widget_ || window_list_.empty())
    return nullptr;

  OverviewItem* first_item = window_list_.front().get();
  return IsDropTargetWindow(first_item->GetWindow()) ? first_item : nullptr;
}

void OverviewGrid::OnWindowDestroying(aura::Window* window) {
  window_observer_.Remove(window);
  window_state_observer_.Remove(wm::GetWindowState(window));
  auto iter = GetOverviewItemIterContainingWindow(window);
  DCHECK(iter != window_list_.end());

  // Windows that are animating to a close state already call PositionWindows,
  // no need to call it twice.
  const bool needs_repositioning = !((*iter)->animating_to_close());

  size_t removed_index = iter - window_list_.begin();
  // Erase from the list first because deleting OverviewItem can lead to
  // iterating through the |window_list_|.
  std::unique_ptr<OverviewItem> tmp = std::move(*iter);
  window_list_.erase(iter);
  tmp.reset();

  if (empty()) {
    selection_widget_.reset();
    // If the grid is now empty, notify |overview_session_| so that it erases us
    // from its grid list.
    if (overview_session_)
      overview_session_->OnGridEmpty(this);
    return;
  }

  // If selecting, update the selection index.
  if (selection_widget_) {
    bool send_focus_alert = selected_index_ == removed_index;
    if (selected_index_ >= removed_index && selected_index_ != 0)
      selected_index_--;
    SelectedWindow()->set_selected(true);
    if (send_focus_alert)
      SelectedWindow()->SendAccessibleSelectionEvent();
  }

  if (needs_repositioning)
    PositionWindows(true);
}

void OverviewGrid::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds,
                                         ui::PropertyChangeReason reason) {
  // During preparation, window bounds can change. Ignore bounds
  // change notifications in this case; we'll reposition soon.
  if (!prepared_for_overview_)
    return;

  auto iter = GetOverviewItemIterContainingWindow(window);
  DCHECK(iter != window_list_.end());

  // Immediately finish any active bounds animation.
  window->layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS);
  (*iter)->UpdateWindowDimensionsType();
  PositionWindows(false);
}

void OverviewGrid::OnWindowPropertyChanged(aura::Window* window,
                                           const void* key,
                                           intptr_t old) {
  if (prepared_for_overview_ && key == aura::client::kTopViewInset &&
      window->GetProperty(aura::client::kTopViewInset) !=
          static_cast<int>(old)) {
    PositionWindows(/*animate=*/false);
  }
}

void OverviewGrid::OnPostWindowStateTypeChange(
    wm::WindowState* window_state,
    mojom::WindowStateType old_type) {
  // During preparation, window state can change, e.g. updating shelf
  // visibility may show the temporarily hidden (minimized) panels.
  if (!prepared_for_overview_)
    return;

  // When swiping away overview mode via shelf, windows will get minimized, but
  // we do not want to create minimized widgets in their place.
  if (overview_session_->enter_exit_overview_type() ==
      OverviewSession::EnterExitOverviewType::kSwipeFromShelf) {
    return;
  }

  mojom::WindowStateType new_type = window_state->GetStateType();
  if (IsMinimizedWindowStateType(old_type) ==
      IsMinimizedWindowStateType(new_type)) {
    return;
  }

  auto iter = std::find_if(window_list_.begin(), window_list_.end(),
                           [window_state](std::unique_ptr<OverviewItem>& item) {
                             return item->Contains(window_state->window());
                           });
  if (iter != window_list_.end()) {
    (*iter)->OnMinimizedStateChanged();
    PositionWindows(/*animate=*/false);
  }
}

void OverviewGrid::OnScreenCopiedBeforeRotation() {
  for (auto& window : window_list()) {
    window->set_disable_mask(true);
    window->UpdateMaskAndShadow();
  }
}

void OverviewGrid::OnScreenRotationAnimationFinished(
    ScreenRotationAnimator* animator,
    bool canceled) {
  for (auto& window : window_list())
    window->set_disable_mask(false);
  Shell::Get()->overview_controller()->DelayedUpdateMaskAndShadow();
}

void OverviewGrid::OnStartingAnimationComplete() {
  if (!shield_widget_) {
    InitShieldWidget(/*animate=*/true);
    ShowNoRecentsWindowMessage(window_list_.empty());
  }

  for (auto& window : window_list())
    window->OnStartingAnimationComplete();
}

bool OverviewGrid::ShouldAnimateWallpaper() const {
  // Never animate when doing app dragging.
  if (overview_session_->enter_exit_overview_type() ==
      OverviewSession::EnterExitOverviewType::kWindowDragged) {
    return false;
  }

  // If one of the windows covers the workspace, we do not need to animate.
  for (const auto& overview_item : window_list_) {
    if (CanCoverAvailableWorkspace(overview_item->GetWindow()))
      return false;
  }

  return true;
}

bool OverviewGrid::IsNoItemsIndicatorLabelVisibleForTesting() {
  return shield_view_ && shield_view_->IsLabelVisible();
}

gfx::Rect OverviewGrid::GetNoItemsIndicatorLabelBoundsForTesting() const {
  if (!shield_view_)
    return gfx::Rect();

  return shield_view_->GetLabelBounds();
}

void OverviewGrid::CalculateWindowListAnimationStates(
    OverviewItem* selected_item,
    OverviewSession::OverviewTransition transition) {
  // |selected_item| is nullptr during entering animation.
  DCHECK(transition == OverviewSession::OverviewTransition::kExit ||
         selected_item == nullptr);

  bool has_covered_available_workspace = false;
  bool has_checked_selected_item = false;
  if (!selected_item ||
      !wm::GetWindowState(selected_item->GetWindow())->IsFullscreen()) {
    // Check the always on top window first if |selected_item| is nullptr or the
    // |selected_item|'s window is not fullscreen. Because always on top windows
    // are visible and may have a window which can cover available workspace.
    // If the |selected_item| is fullscreen, we will depromote all always on top
    // windows.
    aura::Window* always_on_top_container =
        RootWindowController::ForWindow(root_window_)
            ->GetContainer(kShellWindowId_AlwaysOnTopContainer);
    aura::Window::Windows top_windows = always_on_top_container->children();
    for (aura::Window::Windows::const_reverse_iterator
             it = top_windows.rbegin(),
             rend = top_windows.rend();
         it != rend; ++it) {
      aura::Window* top_window = *it;
      OverviewItem* container_item = GetOverviewItemContaining(top_window);
      if (!container_item)
        continue;

      const bool is_selected_item = (selected_item == container_item);
      if (!has_checked_selected_item && is_selected_item)
        has_checked_selected_item = true;
      CalculateOverviewItemAnimationState(
          container_item, &has_covered_available_workspace,
          /*selected=*/is_selected_item, transition);
    }
  }

  if (!has_checked_selected_item) {
    CalculateOverviewItemAnimationState(selected_item,
                                        &has_covered_available_workspace,
                                        /*selected=*/true, transition);
  }
  for (const auto& item : window_list_) {
    // Has checked the |selected_item|.
    if (selected_item == item.get())
      continue;
    // Has checked all always on top windows.
    if (item->GetWindow()->GetProperty(aura::client::kAlwaysOnTopKey))
      continue;
    CalculateOverviewItemAnimationState(item.get(),
                                        &has_covered_available_workspace,
                                        /*selected=*/false, transition);
  }
}

void OverviewGrid::SetWindowListNotAnimatedWhenExiting() {
  should_animate_when_exiting_ = false;
  for (const auto& item : window_list_)
    item->set_should_animate_when_exiting(false);
}

void OverviewGrid::StartNudge(OverviewItem* item) {
  // When there is one window left, there is no need to nudge.
  if (window_list_.size() <= 1) {
    nudge_data_.clear();
    return;
  }

  // If any of the items are being animated to close, do not nudge any windows
  // otherwise we have to deal with potential items getting removed from
  // |window_list_| midway through a nudge.
  for (const auto& window_item : window_list_) {
    if (window_item->animating_to_close()) {
      nudge_data_.clear();
      return;
    }
  }

  DCHECK(item);

  // Get the bounds of the windows currently, and the bounds if |item| were to
  // be removed.
  std::vector<gfx::RectF> src_rects;
  for (const auto& window_item : window_list_)
    src_rects.push_back(window_item->target_bounds());

  std::vector<gfx::RectF> dst_rects = GetWindowRects(item);

  // Get the index of |item|.
  size_t index =
      std::find_if(window_list_.begin(), window_list_.end(),
                   [&item](const std::unique_ptr<OverviewItem>& item_ptr) {
                     return item == item_ptr.get();
                   }) -
      window_list_.begin();
  DCHECK_LT(index, window_list_.size());

  // Returns a vector of integers indicating which row the item is in. |index|
  // is the index of the element which is going to be deleted and should not
  // factor into calculations. The call site should mark |index| as -1 if it
  // should not be used. The item at |index| is marked with a 0. The heights of
  // items are all set to the same value so a new row is determined if the y
  // value has changed from the previous item.
  auto get_rows = [](const std::vector<gfx::RectF>& bounds_list, size_t index) {
    std::vector<int> row_numbers;
    int current_row = 1;
    float last_y = 0;
    for (size_t i = 0; i < bounds_list.size(); ++i) {
      if (i == index) {
        row_numbers.push_back(0);
        continue;
      }

      // Update |current_row| if the y position has changed (heights are all
      // equal in overview, so a new y position indicates a new row).
      if (last_y != 0 && last_y != bounds_list[i].y())
        ++current_row;

      row_numbers.push_back(current_row);
      last_y = bounds_list[i].y();
    }

    return row_numbers;
  };

  std::vector<int> src_rows = get_rows(src_rects, -1);
  std::vector<int> dst_rows = get_rows(dst_rects, index);

  // Do nothing if the number of rows change.
  if (dst_rows.back() != 0 && src_rows.back() != dst_rows.back())
    return;
  size_t second_last_index = src_rows.size() - 2;
  if (dst_rows.back() == 0 &&
      src_rows[second_last_index] != dst_rows[second_last_index]) {
    return;
  }

  // Do nothing if the last item from the previous row will drop onto the
  // current row, this will cause the items in the current row to shift to the
  // right while the previous item stays in the previous row, which looks weird.
  if (src_rows[index] > 1) {
    // Find the last item from the previous row.
    size_t previous_row_last_index = index;
    while (src_rows[previous_row_last_index] == src_rows[index]) {
      --previous_row_last_index;
    }

    // Early return if the last item in the previous row changes rows.
    if (src_rows[previous_row_last_index] != dst_rows[previous_row_last_index])
      return;
  }

  // Helper to check whether the item at |item_index| will be nudged.
  auto should_nudge = [&src_rows, &dst_rows, &index](size_t item_index) {
    // Out of bounds.
    if (item_index >= src_rows.size())
      return false;

    // Nudging happens when the item stays on the same row and is also on the
    // same row as the item to be deleted was.
    if (dst_rows[item_index] == src_rows[index] &&
        dst_rows[item_index] == src_rows[item_index]) {
      return true;
    }

    return false;
  };

  // Starting from |index| go up and down while the nudge condition returns
  // true.
  std::vector<int> affected_indexes;
  size_t loop_index;

  if (index > 0) {
    loop_index = index - 1;
    while (should_nudge(loop_index)) {
      affected_indexes.push_back(loop_index);
      --loop_index;
    }
  }

  loop_index = index + 1;
  while (should_nudge(loop_index)) {
    affected_indexes.push_back(loop_index);
    ++loop_index;
  }

  // Populate |nudge_data_| with the indexes in |affected_indexes| and their
  // respective source and destination bounds.
  nudge_data_.resize(affected_indexes.size());
  for (size_t i = 0; i < affected_indexes.size(); ++i) {
    NudgeData data;
    data.index = affected_indexes[i];
    data.src = src_rects[data.index];
    data.dst = dst_rects[data.index];
    nudge_data_[i] = data;
  }
}

void OverviewGrid::UpdateNudge(OverviewItem* item, double value) {
  for (const auto& data : nudge_data_) {
    DCHECK_LT(data.index, window_list_.size());

    OverviewItem* nudged_item = window_list_[data.index].get();
    double nudge_param = value * value / 30.0;
    nudge_param = base::ClampToRange(nudge_param, 0.0, 1.0);
    gfx::RectF bounds =
        gfx::Tween::RectFValueBetween(nudge_param, data.src, data.dst);
    nudged_item->SetBounds(bounds, OVERVIEW_ANIMATION_NONE);
  }
}

void OverviewGrid::EndNudge() {
  nudge_data_.clear();
}

void OverviewGrid::SlideWindowsIn() {
  for (const auto& window_item : window_list_)
    window_item->SlideWindowIn();
}

void OverviewGrid::UpdateYPositionAndOpacity(
    int new_y,
    float opacity,
    const gfx::Rect& work_area,
    OverviewSession::UpdateAnimationSettingsCallback callback) {
  // Translate |shield_widget_| to |new_y|. The shield widget covers the shelf
  // so scale it down while moving it, so that it does not cover the launcher,
  // which is showing as this is disappearing.
  aura::Window* shield_window = shield_widget_->GetNativeWindow();
  float height_ratio = 1.f;
  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
  if (!callback.is_null()) {
    settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        shield_window->layer()->GetAnimator());
    callback.Run(settings.get(), /*observe=*/true);
  } else {
    height_ratio = static_cast<float>(work_area.height()) /
                   static_cast<float>(shield_window->bounds().height());
  }
  shield_window->SetTransform(gfx::Transform(1.f, 0.f, 0.f, height_ratio, 0.f,
                                             static_cast<float>(new_y)));
  shield_window->layer()->SetOpacity(opacity);

  // Apply the same translation and opacity change to the windows in the grid.
  for (const auto& window_item : window_list_)
    window_item->UpdateYPositionAndOpacity(new_y, opacity, callback);
}

aura::Window* OverviewGrid::GetTargetWindowOnLocation(
    const gfx::Point& location_in_screen) {
  // Find the overview item that contains |location_in_screen|.
  auto iter = std::find_if(
      window_list_.begin(), window_list_.end(),
      [&location_in_screen](std::unique_ptr<OverviewItem>& item) {
        return item->target_bounds().Contains(gfx::PointF(location_in_screen));
      });

  return (iter != window_list_.end()) ? (*iter)->GetWindow() : nullptr;
}

const DesksBarView* OverviewGrid::GetDesksBarViewForTesting() const {
  return shield_view_->desks_bar_view();
}

bool OverviewGrid::IsDesksBarViewActive() const {
  DCHECK(features::IsVirtualDesksEnabled());

  // The desk bar view is not active if there is only a single desk when
  // overview is started. Once there are more than one desk, it should stay
  // active even if the 2nd to last desk is deleted.
  return DesksController::Get()->desks().size() > 1 ||
         (shield_view_ &&
          !shield_view_->desks_bar_view()->mini_views().empty());
}

void OverviewGrid::InitShieldWidget(bool animate) {
  // TODO(varkha): The code assumes that SHELF_BACKGROUND_MAXIMIZED is
  // synonymous with a black shelf background. Update this code if that
  // assumption is no longer valid.
  const float initial_opacity =
      (Shelf::ForWindow(root_window_)->GetBackgroundType() ==
       SHELF_BACKGROUND_MAXIMIZED)
          ? 1.f
          : 0.f;
  shield_widget_ = CreateBackgroundWidget(
      root_window_, ui::LAYER_NOT_DRAWN, SK_ColorTRANSPARENT, 0, 0,
      SK_ColorTRANSPARENT, initial_opacity, /*parent=*/nullptr,
      /*stack_on_top=*/true, /*accept_events=*/true);
  aura::Window* widget_window = shield_widget_->GetNativeWindow();
  aura::Window* parent_window = widget_window->parent();
  const gfx::Rect bounds = ash::screen_util::SnapBoundsToDisplayEdge(
      parent_window->bounds(), parent_window);
  widget_window->SetBounds(bounds);
  widget_window->SetName("OverviewModeShield");

  // Create |shield_view_| and animate its background and label if needed.
  shield_view_ = new ShieldView();
  shield_widget_->SetContentsView(shield_view_);
  shield_view_->MaybeInitVirtualDesksBar();
  shield_view_->SetGridBounds(bounds_);

  // TODO(sammiequon): The shield widget is not anymore in most cases. Remove
  // the shield widget and put the cannot snap widget and desks bar into their
  // own widgets.
  if (animate) {
    shield_widget_->SetOpacity(initial_opacity);
    ScopedOverviewAnimationSettings settings(OVERVIEW_ANIMATION_SHIELD_FADE,
                                             shield_widget_->GetNativeWindow());
    shield_widget_->SetOpacity(1.f);
  } else {
    shield_widget_->SetOpacity(1.f);
  }
}

void OverviewGrid::InitSelectionWidget(OverviewSession::Direction direction) {
  selection_widget_ = CreateBackgroundWidget(
      root_window_, ui::LAYER_TEXTURED, kWindowSelectionColor, 0,
      kWindowSelectionRadius, SK_ColorTRANSPARENT, 0.f, /*parent=*/nullptr,
      /*stack_on_top=*/true, /*accept_events=*/false);
  aura::Window* widget_window = selection_widget_->GetNativeWindow();
  gfx::Rect target_bounds =
      gfx::ToEnclosedRect(SelectedWindow()->target_bounds());
  ::wm::ConvertRectFromScreen(root_window_, &target_bounds);
  gfx::Vector2d fade_out_direction =
      GetSlideVectorForFadeIn(direction, target_bounds);
  widget_window->SetBounds(target_bounds - fade_out_direction);
  widget_window->SetName("OverviewModeSelector");

  selector_shadow_ = std::make_unique<ui::Shadow>();
  selector_shadow_->Init(kWindowSelectionShadowElevation);
  selector_shadow_->layer()->SetVisible(true);
  selection_widget_->GetLayer()->SetMasksToBounds(false);
  selection_widget_->GetLayer()->Add(selector_shadow_->layer());
  selector_shadow_->SetContentBounds(gfx::Rect(target_bounds.size()));
}

void OverviewGrid::MoveSelectionWidget(OverviewSession::Direction direction,
                                       bool recreate_selection_widget,
                                       bool out_of_bounds,
                                       bool animate) {
  // If the selection widget is already active, fade it out in the selection
  // direction.
  if (selection_widget_ && (recreate_selection_widget || out_of_bounds)) {
    // Animate the old selection widget and then destroy it.
    views::Widget* old_selection = selection_widget_.get();
    aura::Window* old_selection_window = old_selection->GetNativeWindow();
    gfx::Vector2d fade_out_direction =
        GetSlideVectorForFadeIn(direction, old_selection_window->bounds());

    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_SELECTION_WINDOW, old_selection_window);
    // CleanupAnimationObserver will delete itself (and the widget) when the
    // motion animation is complete. Ownership over the observer is passed to
    // the overview_session_->delegate() which has longer lifetime so that
    // animations can continue even after the overview session is shut down.
    std::unique_ptr<CleanupAnimationObserver> observer(
        new CleanupAnimationObserver(std::move(selection_widget_)));
    settings.AddObserver(observer.get());
    overview_session_->delegate()->AddDelayedAnimationObserver(
        std::move(observer));
    old_selection->SetOpacity(0.f);
    old_selection_window->SetBounds(old_selection_window->bounds() +
                                    fade_out_direction);
    old_selection->Hide();
  }
  if (out_of_bounds)
    return;

  if (!selection_widget_)
    InitSelectionWidget(direction);
  // Send an a11y alert so that if ChromeVox is enabled, the item label is
  // read.
  SelectedWindow()->SendAccessibleSelectionEvent();
  // The selection widget is moved to the newly selected item in the same
  // grid.
  MoveSelectionWidgetToTarget(animate);
}

void OverviewGrid::MoveSelectionWidgetToTarget(bool animate) {
  gfx::Rect bounds = gfx::ToEnclosingRect(SelectedWindow()->target_bounds());
  ::wm::ConvertRectFromScreen(root_window_, &bounds);
  if (animate) {
    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_SELECTION_WINDOW,
        selection_widget_->GetNativeWindow());
    selection_widget_->SetBounds(bounds);
    selection_widget_->SetOpacity(1.f);

    if (selector_shadow_) {
      ScopedOverviewAnimationSettings settings(
          OVERVIEW_ANIMATION_SELECTION_WINDOW_SHADOW,
          selector_shadow_->shadow_layer()->GetAnimator());
      bounds.Inset(1, 1);
      selector_shadow_->SetContentBounds(
          gfx::Rect(gfx::Point(1, 1), bounds.size()));
    }
    return;
  }
  selection_widget_->SetBounds(bounds);
  selection_widget_->SetOpacity(1.f);
  if (selector_shadow_) {
    bounds.Inset(1, 1);
    selector_shadow_->SetContentBounds(
        gfx::Rect(gfx::Point(1, 1), bounds.size()));
  }
}

std::vector<gfx::RectF> OverviewGrid::GetWindowRects(
    OverviewItem* ignored_item) {
  gfx::Rect total_bounds = bounds_;

  if (features::IsVirtualDesksEnabled()) {
    const int desks_bar_height = DesksBarView::GetBarHeight();

    // Always reduce the grid's height, even if the desks bar is not active. We
    // do this to avoid changing the overview windows sizes once the desks bar
    // becomes active, and we shift the grid downwards.
    total_bounds.set_height(total_bounds.height() - desks_bar_height);

    if (IsDesksBarViewActive())
      total_bounds.Offset(0, desks_bar_height);
  }

  // Windows occupy vertically centered area with additional vertical insets.
  int horizontal_inset =
      gfx::ToFlooredInt(std::min(kOverviewInsetRatio * total_bounds.width(),
                                 kOverviewInsetRatio * total_bounds.height()));
  int vertical_inset =
      horizontal_inset +
      kOverviewVerticalInset * (total_bounds.height() - 2 * horizontal_inset);
  total_bounds.Inset(std::max(0, horizontal_inset - kWindowMargin),
                     std::max(0, vertical_inset - kWindowMargin));
  std::vector<gfx::RectF> rects;

  // Keep track of the lowest coordinate.
  int max_bottom = total_bounds.y();

  // Right bound of the narrowest row.
  int min_right = total_bounds.right();
  // Right bound of the widest row.
  int max_right = total_bounds.x();

  // Keep track of the difference between the narrowest and the widest row.
  // Initially this is set to the worst it can ever be assuming the windows fit.
  int width_diff = total_bounds.width();

  // Initially allow the windows to occupy all available width. Shrink this
  // available space horizontally to find the breakdown into rows that achieves
  // the minimal |width_diff|.
  int right_bound = total_bounds.right();

  // Determine the optimal height bisecting between |low_height| and
  // |high_height|. Once this optimal height is known, |height_fixed| is set to
  // true and the rows are balanced by repeatedly squeezing the widest row to
  // cause windows to overflow to the subsequent rows.
  int low_height = 2 * kWindowMargin;
  int high_height = std::max(low_height, total_bounds.height() + 1);
  int height = 0.5 * (low_height + high_height);
  bool height_fixed = false;

  // Repeatedly try to fit the windows |rects| within |right_bound|.
  // If a maximum |height| is found such that all window |rects| fit, this
  // fitting continues while shrinking the |right_bound| in order to balance the
  // rows. If the windows fit the |right_bound| would have been decremented at
  // least once so it needs to be incremented once before getting out of this
  // loop and one additional pass made to actually fit the |rects|.
  // If the |rects| cannot fit (e.g. there are too many windows) the bisection
  // will still finish and we might increment the |right_bound| once pixel extra
  // which is acceptable since there is an unused margin on the right.
  bool make_last_adjustment = false;
  while (true) {
    gfx::Rect overview_mode_bounds(total_bounds);
    overview_mode_bounds.set_width(right_bound - total_bounds.x());
    bool windows_fit = FitWindowRectsInBounds(
        overview_mode_bounds, std::min(kMaxHeight + 2 * kWindowMargin, height),
        ignored_item, &rects, &max_bottom, &min_right, &max_right);

    if (height_fixed) {
      if (!windows_fit) {
        // Revert the previous change to |right_bound| and do one last pass.
        right_bound++;
        make_last_adjustment = true;
        break;
      }
      // Break if all the windows are zero-width at the current scale.
      if (max_right <= total_bounds.x())
        break;
    } else {
      // Find the optimal row height bisecting between |low_height| and
      // |high_height|.
      if (windows_fit)
        low_height = height;
      else
        high_height = height;
      height = 0.5 * (low_height + high_height);
      // When height can no longer be improved, start balancing the rows.
      if (height == low_height)
        height_fixed = true;
    }

    if (windows_fit && height_fixed) {
      if (max_right - min_right <= width_diff) {
        // Row alignment is getting better. Try to shrink the |right_bound| in
        // order to squeeze the widest row.
        right_bound = max_right - 1;
        width_diff = max_right - min_right;
      } else {
        // Row alignment is getting worse.
        // Revert the previous change to |right_bound| and do one last pass.
        right_bound++;
        make_last_adjustment = true;
        break;
      }
    }
  }
  // Once the windows in |window_list_| no longer fit, the change to
  // |right_bound| was reverted. Perform one last pass to position the |rects|.
  if (make_last_adjustment) {
    gfx::Rect overview_mode_bounds(total_bounds);
    overview_mode_bounds.set_width(right_bound - total_bounds.x());
    FitWindowRectsInBounds(
        overview_mode_bounds, std::min(kMaxHeight + 2 * kWindowMargin, height),
        ignored_item, &rects, &max_bottom, &min_right, &max_right);
  }

  gfx::Vector2dF offset(0, (total_bounds.bottom() - max_bottom) / 2.f);
  for (size_t i = 0; i < rects.size(); ++i)
    rects[i] += offset;
  return rects;
}

bool OverviewGrid::FitWindowRectsInBounds(const gfx::Rect& bounds,
                                          int height,
                                          OverviewItem* ignored_item,
                                          std::vector<gfx::RectF>* out_rects,
                                          int* out_max_bottom,
                                          int* out_min_right,
                                          int* out_max_right) {
  out_rects->resize(window_list_.size());
  bool windows_fit = true;

  // Start in the top-left corner of |bounds|.
  int left = bounds.x();
  int top = bounds.y();

  // Keep track of the lowest coordinate.
  *out_max_bottom = bounds.y();

  // Right bound of the narrowest row.
  *out_min_right = bounds.right();
  // Right bound of the widest row.
  *out_max_right = bounds.x();

  // All elements are of same height and only the height is necessary to
  // determine each item's scale.
  const gfx::Size item_size(0, height);
  size_t i = 0;
  for (const auto& window : window_list_) {
    if (window->animating_to_close() ||
        (ignored_item && ignored_item == window.get())) {
      // Increment the index anyways. PositionWindows will handle skipping this
      // entry.
      ++i;
      continue;
    }

    const gfx::RectF target_bounds = window->GetTargetBoundsInScreen();
    int width = std::max(1, gfx::ToFlooredInt(target_bounds.width() *
                                              window->GetItemScale(item_size)) +
                                2 * kWindowMargin);
    switch (window->GetWindowDimensionsType()) {
      case ScopedOverviewTransformWindow::GridWindowFillMode::kLetterBoxed:
        width = ScopedOverviewTransformWindow::kExtremeWindowRatioThreshold *
                height;
        break;
      case ScopedOverviewTransformWindow::GridWindowFillMode::kPillarBoxed:
        width = height /
                ScopedOverviewTransformWindow::kExtremeWindowRatioThreshold;
        break;
      default:
        break;
    }

    if (left + width > bounds.right()) {
      // Move to the next row if possible.
      if (*out_min_right > left)
        *out_min_right = left;
      if (*out_max_right < left)
        *out_max_right = left;
      top += height;

      // Check if the new row reaches the bottom or if the first item in the new
      // row does not fit within the available width.
      if (top + height > bounds.bottom() ||
          bounds.x() + width > bounds.right()) {
        windows_fit = false;
        // If the |ignored_item| is the last item, update |out_max_bottom|
        // before breaking the loop, but no need to add the height, as the last
        // item does not contribute to the grid bounds.
        if (window_list_.back()->animating_to_close() ||
            (ignored_item && ignored_item == window_list_.back().get())) {
          *out_max_bottom = top;
        }
        break;
      }
      left = bounds.x();
    }

    // Position the current rect.
    (*out_rects)[i] = gfx::RectF(gfx::Rect(left, top, width, height));

    // Increment horizontal position using sanitized positive |width()|.
    left += gfx::ToRoundedInt((*out_rects)[i].width());

    if (++i == out_rects->size()) {
      // Update the narrowest and widest row width for the last row.
      if (*out_min_right > left)
        *out_min_right = left;
      if (*out_max_right < left)
        *out_max_right = left;
    }
    *out_max_bottom = top + height;
  }
  return windows_fit;
}

void OverviewGrid::CalculateOverviewItemAnimationState(
    OverviewItem* overview_item,
    bool* has_covered_available_workspace,
    bool selected,
    OverviewSession::OverviewTransition transition) {
  if (!overview_item)
    return;

  aura::Window* window = overview_item->GetWindow();
  // |overview_item| should be contained in the |window_list_|.
  DCHECK(GetOverviewItemContaining(window));

  bool can_cover_available_workspace = CanCoverAvailableWorkspace(window);
  const bool should_animate = selected || !(*has_covered_available_workspace);
  if (transition == OverviewSession::OverviewTransition::kEnter)
    overview_item->set_should_animate_when_entering(should_animate);
  if (transition == OverviewSession::OverviewTransition::kExit)
    overview_item->set_should_animate_when_exiting(should_animate);

  if (!(*has_covered_available_workspace) && can_cover_available_workspace)
    *has_covered_available_workspace = true;
}

std::vector<std::unique_ptr<OverviewItem>>::iterator
OverviewGrid::GetOverviewItemIterContainingWindow(aura::Window* window) {
  return std::find_if(window_list_.begin(), window_list_.end(),
                      [window](std::unique_ptr<OverviewItem>& item) {
                        return item->GetWindow() == window;
                      });
}

void OverviewGrid::AddDraggedWindowIntoOverviewOnDragEnd(
    aura::Window* dragged_window) {
  DCHECK(overview_session_);
  if (overview_session_->IsWindowInOverview(dragged_window))
    return;

  // Update the dragged window's bounds before adding it to overview. The
  // dragged window might have resized to a smaller size if the drag
  // happens on tab(s).
  if (wm::IsDraggingTabs(dragged_window)) {
    const gfx::Rect old_bounds = dragged_window->bounds();
    // We need to temporarily disable the dragged window's ability to merge
    // into another window when changing the dragged window's bounds, so
    // that the dragged window doesn't merge into another window because of
    // its changed bounds.
    dragged_window->SetProperty(ash::kCanAttachToAnotherWindowKey, false);
    TabletModeWindowState::UpdateWindowPosition(
        wm::GetWindowState(dragged_window), /*animate=*/false);
    const gfx::Rect new_bounds = dragged_window->bounds();
    if (old_bounds != new_bounds) {
      // It's for smoother animation.
      gfx::Transform transform =
          ScopedOverviewTransformWindow::GetTransformForRect(
              gfx::RectF(new_bounds), gfx::RectF(old_bounds));
      dragged_window->SetTransform(transform);
    }
    dragged_window->ClearProperty(ash::kCanAttachToAnotherWindowKey);
  }

  overview_session_->AddItem(dragged_window, /*reposition=*/false,
                             /*animate=*/false);
}

}  // namespace ash
