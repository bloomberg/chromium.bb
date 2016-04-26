// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_grid.h"

#include <algorithm>
#include <functional>
#include <set>
#include <utility>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/window_state_aura.h"
#include "base/command_line.h"
#include "base/i18n/string_search.h"
#include "base/memory/scoped_vector.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

typedef std::vector<aura::Window*> Windows;

// An observer which holds onto the passed widget until the animation is
// complete.
class CleanupWidgetAfterAnimationObserver
    : public ui::ImplicitAnimationObserver {
 public:
  explicit CleanupWidgetAfterAnimationObserver(
      std::unique_ptr<views::Widget> widget);
  ~CleanupWidgetAfterAnimationObserver() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

 private:
  std::unique_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(CleanupWidgetAfterAnimationObserver);
};

CleanupWidgetAfterAnimationObserver::CleanupWidgetAfterAnimationObserver(
    std::unique_ptr<views::Widget> widget)
    : widget_(std::move(widget)) {}

CleanupWidgetAfterAnimationObserver::~CleanupWidgetAfterAnimationObserver() {
}

void CleanupWidgetAfterAnimationObserver::OnImplicitAnimationsCompleted() {
  delete this;
}

// A comparator for locating a given target window.
struct WindowSelectorItemComparator {
  explicit WindowSelectorItemComparator(const aura::Window* target_window)
      : target(target_window) {
  }

  bool operator()(WindowSelectorItem* window) const {
    return window->GetWindow() == target;
  }

  const aura::Window* target;
};

// Conceptually the window overview is a table or grid of cells having this
// fixed aspect ratio. The number of columns is determined by maximizing the
// area of them based on the number of window_list.
const float kCardAspectRatio = 4.0f / 3.0f;

// The minimum number of cards along the major axis (i.e. horizontally on a
// landscape orientation).
const int kMinCardsMajor = 3;

const int kOverviewSelectorTransitionMilliseconds = 100;

// The color and opacity of the overview selector.
const SkColor kWindowSelectionColor = SkColorSetARGB(128, 0, 0, 0);
const SkColor kWindowSelectionBorderColor = SkColorSetARGB(38, 255, 255, 255);
const int kWindowSelectionBorderThickness = 2;

// The minimum amount of spacing between the bottom of the text filtering
// text field and the top of the selection widget on the first row of items.
const int kTextFilterBottomMargin = 5;

// Returns the vector for the fade in animation.
gfx::Vector2d GetSlideVectorForFadeIn(WindowSelector::Direction direction,
                                      const gfx::Rect& bounds) {
  gfx::Vector2d vector;
  switch (direction) {
    case WindowSelector::DOWN:
      vector.set_y(bounds.width());
      break;
    case WindowSelector::RIGHT:
      vector.set_x(bounds.height());
      break;
    case WindowSelector::UP:
      vector.set_y(-bounds.width());
      break;
    case WindowSelector::LEFT:
      vector.set_x(-bounds.height());
      break;
  }
  return vector;
}

// Given |root_window|, calculates the item size necessary to fit |items|
// items in the window selection. |bounding_rect| is set to the centered
// rectangle containing the grid and |item_size| is set to the size of each
// individual item.
void CalculateOverviewSizes(aura::Window* root_window,
                            size_t items,
                            gfx::Rect* bounding_rect,
                            gfx::Size* item_size) {
  gfx::Rect total_bounds = ScreenUtil::ConvertRectToScreen(
      root_window,
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(
          Shell::GetContainer(root_window, kShellWindowId_DefaultContainer)));

  // Reserve space at the top for the text filtering textbox to appear.
  total_bounds.Inset(
      0, WindowSelector::kTextFilterBottomEdge + kTextFilterBottomMargin, 0, 0);

  // Find the minimum number of windows per row that will fit all of the
  // windows on screen.
  int num_columns = std::max(
      total_bounds.width() > total_bounds.height() ? kMinCardsMajor : 1,
      static_cast<int>(ceil(sqrt(total_bounds.width() * items /
                                 (kCardAspectRatio * total_bounds.height())))));
  int num_rows = ((items + num_columns - 1) / num_columns);
  item_size->set_width(std::min(
      static_cast<int>(total_bounds.width() / num_columns),
      static_cast<int>(total_bounds.height() * kCardAspectRatio / num_rows)));
  item_size->set_height(
      static_cast<int>(item_size->width() / kCardAspectRatio));
  item_size->SetToMax(gfx::Size(1, 1));

  bounding_rect->set_width(std::min(static_cast<int>(items), num_columns) *
                           item_size->width());
  bounding_rect->set_height(num_rows * item_size->height());
  // Calculate the X and Y offsets necessary to center the grid.
  bounding_rect->set_x(total_bounds.x() +
                       (total_bounds.width() - bounding_rect->width()) / 2);
  bounding_rect->set_y(total_bounds.y() +
                       (total_bounds.height() - bounding_rect->height()) / 2);
}

// Reorders the list of windows |items| in |root_window| in an attempt to
// minimize the distance each window will travel to enter overview. For
// equidistant windows preserves a stable order between overview sessions
// by comparing window pointers.
void ReorderItemsGreedyLeastMovement(std::vector<aura::Window*>* items,
                                     aura::Window* root_window) {
  if (items->empty())
    return;
  gfx::Rect bounding_rect;
  gfx::Size item_size;
  CalculateOverviewSizes(root_window, items->size(), &bounding_rect,
                         &item_size);
  int num_columns = std::min(static_cast<int>(items->size()),
                             bounding_rect.width() / item_size.width());
  for (size_t i = 0; i < items->size(); ++i) {
    int column = i % num_columns;
    int row = i / num_columns;
    gfx::Point overview_item_center(
        bounding_rect.x() + column * item_size.width() + item_size.width() / 2,
        bounding_rect.y() + row * item_size.height() + item_size.height() / 2);
    // Find the nearest window for this position.
    size_t swap_index = i;
    int64_t shortest_distance = std::numeric_limits<int64_t>::max();
    for (size_t j = i; j < items->size(); ++j) {
      aura::Window* window = (*items)[j];
      int64_t distance =
          (ScreenUtil::ConvertRectToScreen(window, window->GetTargetBounds())
               .CenterPoint() -
           overview_item_center)
              .LengthSquared();
      // We compare raw pointers to create a stable ordering given two windows
      // with the same center point.
      if (distance < shortest_distance ||
          (distance == shortest_distance && window < (*items)[swap_index])) {
        shortest_distance = distance;
        swap_index = j;
      }
    }
    if (swap_index > i)
      std::swap((*items)[i], (*items)[swap_index]);
  }
}

}  // namespace

WindowGrid::WindowGrid(aura::Window* root_window,
                       const std::vector<aura::Window*>& windows,
                       WindowSelector* window_selector)
    : root_window_(root_window),
      window_selector_(window_selector) {
  std::vector<aura::Window*> windows_in_root;
  for (auto window : windows) {
    if (window->GetRootWindow() == root_window)
      windows_in_root.push_back(window);
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableStableOverviewOrder)) {
    // Reorder windows to try to minimize movement to target overview positions.
    // This also creates a stable window ordering.
    ReorderItemsGreedyLeastMovement(&windows_in_root, root_window_);
  }
  for (auto window : windows_in_root) {
    window->AddObserver(this);
    observed_windows_.insert(window);
    window_list_.push_back(new WindowSelectorItem(window, window_selector_));
  }
}

WindowGrid::~WindowGrid() {
  for (std::set<aura::Window*>::iterator iter = observed_windows_.begin();
       iter != observed_windows_.end(); iter++) {
    (*iter)->RemoveObserver(this);
  }
}

void WindowGrid::PrepareForOverview() {
  for (ScopedVector<WindowSelectorItem>::iterator iter = window_list_.begin();
       iter != window_list_.end(); ++iter) {
    (*iter)->PrepareForOverview();
  }
}

void WindowGrid::PositionWindows(bool animate) {
  CHECK(!window_list_.empty());
  gfx::Rect bounding_rect;
  gfx::Size item_size;
  CalculateOverviewSizes(root_window_, window_list_.size(), &bounding_rect,
                         &item_size);
  num_columns_ = std::min(static_cast<int>(window_list_.size()),
                          bounding_rect.width() / item_size.width());
  for (size_t i = 0; i < window_list_.size(); ++i) {
    gfx::Transform transform;
    int column = i % num_columns_;
    int row = i / num_columns_;
    gfx::Rect target_bounds(item_size.width() * column + bounding_rect.x(),
                            item_size.height() * row + bounding_rect.y(),
                            item_size.width(), item_size.height());
    window_list_[i]->SetBounds(target_bounds, animate ?
        OverviewAnimationType::OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS :
        OverviewAnimationType::OVERVIEW_ANIMATION_NONE);
  }

  // If the selection widget is active, reposition it without any animation.
  if (selection_widget_)
    MoveSelectionWidgetToTarget(animate);
}

bool WindowGrid::Move(WindowSelector::Direction direction, bool animate) {
  bool recreate_selection_widget = false;
  bool out_of_bounds = false;
  bool changed_selection_index = false;
  if (!selection_widget_) {
    switch (direction) {
     case WindowSelector::LEFT:
       selected_index_ = window_list_.size() - 1;
       break;
     case WindowSelector::UP:
       selected_index_ =
           (window_list_.size() / num_columns_) * num_columns_ - 1;
       break;
     case WindowSelector::RIGHT:
     case WindowSelector::DOWN:
       selected_index_ = 0;
       break;
    }
    changed_selection_index = true;
  }
  while (!changed_selection_index ||
         (!out_of_bounds && window_list_[selected_index_]->dimmed())) {
    switch (direction) {
      case WindowSelector::RIGHT:
        if (selected_index_ >= window_list_.size() - 1)
          out_of_bounds = true;
        selected_index_++;
        if (selected_index_ % num_columns_ == 0)
          recreate_selection_widget = true;
        break;
      case WindowSelector::LEFT:
        if (selected_index_ == 0)
          out_of_bounds = true;
        selected_index_--;
        if ((selected_index_ + 1) % num_columns_ == 0)
          recreate_selection_widget = true;
        break;
      case WindowSelector::DOWN:
        selected_index_ += num_columns_;
        if (selected_index_ >= window_list_.size()) {
          selected_index_ = (selected_index_ + 1) % num_columns_;
          if (selected_index_ == 0)
            out_of_bounds = true;
          recreate_selection_widget = true;
        }
        break;
      case WindowSelector::UP:
        if (selected_index_ == 0)
          out_of_bounds = true;
        if (selected_index_ < num_columns_) {
          selected_index_ += num_columns_ *
              ((window_list_.size() - selected_index_) / num_columns_) - 1;
          recreate_selection_widget = true;
        } else {
          selected_index_ -= num_columns_;
        }
        break;
    }
    changed_selection_index = true;
  }

  MoveSelectionWidget(direction, recreate_selection_widget,
                      out_of_bounds, animate);
  return out_of_bounds;
}

WindowSelectorItem* WindowGrid::SelectedWindow() const {
  if (!selection_widget_)
    return nullptr;
  CHECK(selected_index_ < window_list_.size());
  return window_list_[selected_index_];
}

bool WindowGrid::Contains(const aura::Window* window) const {
  for (const WindowSelectorItem* window_item : window_list_) {
    if (window_item->Contains(window))
      return true;
  }
  return false;
}

void WindowGrid::FilterItems(const base::string16& pattern) {
  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents finder(pattern);
  for (ScopedVector<WindowSelectorItem>::iterator iter = window_list_.begin();
       iter != window_list_.end(); iter++) {
    if (finder.Search((*iter)->GetWindow()->title(), nullptr, nullptr)) {
      (*iter)->SetDimmed(false);
    } else {
      (*iter)->SetDimmed(true);
      if (selection_widget_ && SelectedWindow() == *iter)
        selection_widget_.reset();
    }
  }
}

void WindowGrid::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  observed_windows_.erase(window);
  ScopedVector<WindowSelectorItem>::iterator iter =
      std::find_if(window_list_.begin(), window_list_.end(),
                   WindowSelectorItemComparator(window));

  DCHECK(iter != window_list_.end());

  size_t removed_index = iter - window_list_.begin();
  window_list_.erase(iter);

  if (empty()) {
    // If the grid is now empty, notify the window selector so that it erases us
    // from its grid list.
    window_selector_->OnGridEmpty(this);
    return;
  }

  // If selecting, update the selection index.
  if (selection_widget_) {
    bool send_focus_alert = selected_index_ == removed_index;
    if (selected_index_ >= removed_index && selected_index_ != 0)
      selected_index_--;
    if (send_focus_alert)
      SelectedWindow()->SendAccessibleSelectionEvent();
  }

  PositionWindows(true);
}

void WindowGrid::OnWindowBoundsChanged(aura::Window* window,
                                       const gfx::Rect& old_bounds,
                                       const gfx::Rect& new_bounds) {
  ScopedVector<WindowSelectorItem>::const_iterator iter =
      std::find_if(window_list_.begin(), window_list_.end(),
                   WindowSelectorItemComparator(window));
  DCHECK(iter != window_list_.end());

  // Immediately finish any active bounds animation.
  window->layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS);

  // Recompute the transform for the window.
  (*iter)->RecomputeWindowTransforms();
}

void WindowGrid::InitSelectionWidget(WindowSelector::Direction direction) {
  selection_widget_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.keep_on_top = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = Shell::GetContainer(root_window_,
                                      kShellWindowId_DefaultContainer);
  params.accept_events = false;
  selection_widget_->set_focus_on_creation(false);
  selection_widget_->Init(params);
  // Disable the "bounce in" animation when showing the window.
  ::wm::SetWindowVisibilityAnimationTransition(
      selection_widget_->GetNativeWindow(), ::wm::ANIMATE_NONE);
  // The selection widget should not activate the shelf when passing under it.
  wm::GetWindowState(selection_widget_->GetNativeWindow())->
      set_ignored_by_shelf(true);

  views::View* content_view = new views::View;
  content_view->set_background(
      views::Background::CreateSolidBackground(kWindowSelectionColor));
  content_view->SetBorder(views::Border::CreateSolidBorder(
      kWindowSelectionBorderThickness, kWindowSelectionBorderColor));
  selection_widget_->SetContentsView(content_view);
  selection_widget_->GetNativeWindow()->parent()->StackChildAtBottom(
      selection_widget_->GetNativeWindow());
  selection_widget_->Show();
  // New selection widget starts with 0 opacity and then fades in.
  selection_widget_->GetNativeWindow()->layer()->SetOpacity(0);

  const gfx::Rect target_bounds = SelectedWindow()->target_bounds();
  gfx::Vector2d fade_out_direction =
          GetSlideVectorForFadeIn(direction, target_bounds);
  gfx::Display dst_display =
      gfx::Screen::GetScreen()->GetDisplayMatching(target_bounds);
  selection_widget_->GetNativeWindow()->SetBoundsInScreen(
      target_bounds - fade_out_direction, dst_display);
}

void WindowGrid::MoveSelectionWidget(WindowSelector::Direction direction,
                                     bool recreate_selection_widget,
                                     bool out_of_bounds,
                                     bool animate) {
  // If the selection widget is already active, fade it out in the selection
  // direction.
  if (selection_widget_ && (recreate_selection_widget || out_of_bounds)) {
    // Animate the old selection widget and then destroy it.
    views::Widget* old_selection = selection_widget_.get();
    gfx::Vector2d fade_out_direction =
        GetSlideVectorForFadeIn(
            direction, old_selection->GetNativeWindow()->bounds());

    ui::ScopedLayerAnimationSettings animation_settings(
        old_selection->GetNativeWindow()->layer()->GetAnimator());
    animation_settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(
            kOverviewSelectorTransitionMilliseconds));
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    animation_settings.SetTweenType(gfx::Tween::FAST_OUT_LINEAR_IN);
    // CleanupWidgetAfterAnimationObserver will delete itself (and the
    // widget) when the movement animation is complete.
    animation_settings.AddObserver(
        new CleanupWidgetAfterAnimationObserver(std::move(selection_widget_)));
    old_selection->SetOpacity(0);
    old_selection->GetNativeWindow()->SetBounds(
        old_selection->GetNativeWindow()->bounds() + fade_out_direction);
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

void WindowGrid::MoveSelectionWidgetToTarget(bool animate) {
  if (animate) {
    ui::ScopedLayerAnimationSettings animation_settings(
        selection_widget_->GetNativeWindow()->layer()->GetAnimator());
    animation_settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        kOverviewSelectorTransitionMilliseconds));
    animation_settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    selection_widget_->SetBounds(SelectedWindow()->target_bounds());
    selection_widget_->SetOpacity(255);
    return;
  }
  selection_widget_->SetBounds(SelectedWindow()->target_bounds());
  selection_widget_->SetOpacity(255);
}

}  // namespace ash
