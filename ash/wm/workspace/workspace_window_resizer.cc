// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include <algorithm>
#include <cmath>

#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/wm/root_window_event_filter.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"

namespace ash {
namespace internal {

namespace {

// Duration of the animation when snapping the window into place.
const int kSnapDurationMS = 100;

// Returns true if should snap to the edge.
bool ShouldSnapToEdge(int distance_from_edge, int grid_size) {
  return distance_from_edge <= grid_size / 2 &&
      distance_from_edge > -grid_size * 2;
}

}  // namespace

// static
const int WorkspaceWindowResizer::kMinOnscreenSize = 20;

// static
const int WorkspaceWindowResizer::kMinOnscreenHeight = 32;

WorkspaceWindowResizer::~WorkspaceWindowResizer() {
  if (root_filter_)
    root_filter_->UnlockCursor();
}

// static
WorkspaceWindowResizer* WorkspaceWindowResizer::Create(
    aura::Window* window,
    const gfx::Point& location,
    int window_component,
    const std::vector<aura::Window*>& attached_windows) {
  Details details(window, location, window_component);
  return details.is_resizable ?
      new WorkspaceWindowResizer(details, attached_windows) : NULL;
}

void WorkspaceWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  int grid_size = event_flags & ui::EF_CONTROL_DOWN ?
                  0 : ash::Shell::GetInstance()->GetGridSize();
  gfx::Rect bounds = CalculateBoundsForDrag(details_, location, grid_size);

  if (wm::IsWindowNormal(details_.window))
    AdjustBoundsForMainWindow(&bounds, grid_size);
  if (bounds != details_.window->bounds()) {
    if (!did_move_or_resize_)
      RestackWindows();
    did_move_or_resize_ = true;
  }
  UpdatePhantomWindow(location, bounds, grid_size);
  if (!attached_windows_.empty())
    LayoutAttachedWindows(bounds, grid_size);
  if (bounds != details_.window->bounds())
    details_.window->SetBounds(bounds);
  // WARNING: we may have been deleted.
}

void WorkspaceWindowResizer::CompleteDrag(int event_flags) {
  phantom_window_controller_.reset();
  if (!did_move_or_resize_ || details_.window_component != HTCAPTION)
    return;

  if (snap_type_ == SNAP_LEFT_EDGE || snap_type_ == SNAP_RIGHT_EDGE) {
    if (!GetRestoreBounds(details_.window))
      SetRestoreBounds(details_.window, details_.initial_bounds);
    details_.window->SetBounds(snap_sizer_->target_bounds());
    return;
  }

  int grid_size = event_flags & ui::EF_CONTROL_DOWN ?
                  0 : ash::Shell::GetInstance()->GetGridSize();
  if (grid_size <= 1)
    return;

  gfx::Rect bounds(GetFinalBounds(details_.window->bounds(), grid_size));
  if (bounds == details_.window->bounds())
    return;

  if (bounds.size() != details_.window->bounds().size()) {
    // Don't attempt to animate a size change.
    details_.window->SetBounds(bounds);
    return;
  }

  ui::ScopedLayerAnimationSettings scoped_setter(
      details_.window->layer()->GetAnimator());
  // Use a small duration since the grid is small.
  scoped_setter.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kSnapDurationMS));
  details_.window->SetBounds(bounds);
}

void WorkspaceWindowResizer::RevertDrag() {
  phantom_window_controller_.reset();

  if (!did_move_or_resize_)
    return;

  details_.window->SetBounds(details_.initial_bounds);
  if (details_.window_component == HTRIGHT) {
    int last_x = details_.initial_bounds.right();
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      gfx::Rect bounds(attached_windows_[i]->bounds());
      bounds.set_x(last_x);
      bounds.set_width(initial_size_[i]);
      attached_windows_[i]->SetBounds(bounds);
      last_x = attached_windows_[i]->bounds().right();
    }
  } else {
    int last_y = details_.initial_bounds.bottom();
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      gfx::Rect bounds(attached_windows_[i]->bounds());
      bounds.set_y(last_y);
      bounds.set_height(initial_size_[i]);
      attached_windows_[i]->SetBounds(bounds);
      last_y = attached_windows_[i]->bounds().bottom();
    }
  }
}

WorkspaceWindowResizer::WorkspaceWindowResizer(
    const Details& details,
    const std::vector<aura::Window*>& attached_windows)
    : details_(details),
      attached_windows_(attached_windows),
      did_move_or_resize_(false),
      root_filter_(NULL),
      total_min_(0),
      total_initial_size_(0),
      snap_type_(SNAP_NONE),
      num_mouse_moves_since_bounds_change_(0) {
  DCHECK(details_.is_resizable);
  root_filter_ = Shell::GetInstance()->root_filter();
  if (root_filter_)
    root_filter_->LockCursor();

  // Only support attaching to the right/bottom.
  DCHECK(attached_windows_.empty() ||
         (details.window_component == HTRIGHT ||
          details.window_component == HTBOTTOM));

  // TODO: figure out how to deal with window going off the edge.

  // Calculate sizes so that we can maintain the ratios if we need to resize.
  int total_available = 0;
  int grid_size = ash::Shell::GetInstance()->GetGridSize();
  for (size_t i = 0; i < attached_windows_.size(); ++i) {
    gfx::Size min(attached_windows_[i]->delegate()->GetMinimumSize());
    int initial_size = PrimaryAxisSize(attached_windows_[i]->bounds().size());
    initial_size_.push_back(initial_size);
    // If current size is smaller than the min, use the current size as the min.
    // This way we don't snap on resize.
    int min_size = std::min(initial_size,
                            std::max(PrimaryAxisSize(min), kMinOnscreenSize));
    // Make sure the min size falls on the grid.
    if (grid_size > 1 && min_size % grid_size != 0)
      min_size = (min_size / grid_size + 1) * grid_size;
    min_size_.push_back(min_size);
    total_min_ += min_size;
    total_initial_size_ += initial_size;
    total_available += std::max(min_size, initial_size) - min_size;
  }

  for (size_t i = 0; i < attached_windows_.size(); ++i) {
    expand_fraction_.push_back(
          static_cast<float>(initial_size_[i]) /
          static_cast<float>(total_initial_size_));
    if (total_initial_size_ != total_min_) {
      compress_fraction_.push_back(
          static_cast<float>(initial_size_[i] - min_size_[i]) /
          static_cast<float>(total_available));
    } else {
      compress_fraction_.push_back(0.0f);
    }
  }
}

gfx::Rect WorkspaceWindowResizer::GetFinalBounds(
    const gfx::Rect& bounds,
    int grid_size) const {
  if (phantom_window_controller_.get() &&
      phantom_window_controller_->IsShowing()) {
    return phantom_window_controller_->bounds();
  }
  return AdjustBoundsToGrid(bounds, grid_size);
}

void WorkspaceWindowResizer::LayoutAttachedWindows(
    const gfx::Rect& bounds,
    int grid_size) {
  gfx::Rect work_area(
      gfx::Screen::GetMonitorNearestWindow(window()).work_area());
  std::vector<int> sizes;
  CalculateAttachedSizes(
      PrimaryAxisSize(details_.initial_bounds.size()),
      PrimaryAxisSize(bounds.size()),
      PrimaryAxisCoordinate(bounds.right(), bounds.bottom()),
      PrimaryAxisCoordinate(work_area.right(), work_area.bottom()),
      grid_size,
      &sizes);
  DCHECK_EQ(attached_windows_.size(), sizes.size());
  int last = PrimaryAxisCoordinate(bounds.right(), bounds.bottom());
  for (size_t i = 0; i < attached_windows_.size(); ++i) {
    gfx::Rect attached_bounds(attached_windows_[i]->bounds());
    if (details_.window_component == HTRIGHT) {
      attached_bounds.set_x(last);
      attached_bounds.set_width(sizes[i]);
    } else {
      attached_bounds.set_y(last);
      attached_bounds.set_height(sizes[i]);
    }
    attached_windows_[i]->SetBounds(attached_bounds);
    last += sizes[i];
  }
}

void WorkspaceWindowResizer::CalculateAttachedSizes(
    int initial_size,
    int current_size,
    int start,
    int end,
    int grid_size,
    std::vector<int>* sizes) const {
  sizes->clear();
  if (current_size < initial_size) {
    // If the primary window is sized smaller, resize the attached windows.
    int current = start;
    int delta = initial_size - current_size;
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      int next = AlignToGrid(
          current + initial_size_[i] + expand_fraction_[i] * delta,
          grid_size);
      if (i == attached_windows_.size())
        next = end;
      sizes->push_back(next - current);
      current = next;
    }
  } else if (start <= end - total_initial_size_) {
    // All the windows fit at their initial size; tile them horizontally.
    for (size_t i = 0; i < attached_windows_.size(); ++i)
      sizes->push_back(initial_size_[i]);
  } else {
    DCHECK_NE(total_initial_size_, total_min_);
    int delta = total_initial_size_ - (end - start);
    int current = start;
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      int size = initial_size_[i] -
          static_cast<int>(compress_fraction_[i] * delta);
      size = AlignToGrid(size, grid_size);
      if (i == attached_windows_.size())
        size = end - current;
      current += size;
      sizes->push_back(size);
    }
  }
}

void WorkspaceWindowResizer::AdjustBoundsForMainWindow(
    gfx::Rect* bounds, int grid_size) const {
  // Always keep kMinOnscreenHeight on the bottom.
  gfx::Rect work_area(
      gfx::Screen::GetMonitorNearestWindow(window()).work_area());
  int max_y = AlignToGridRoundUp(work_area.bottom() - kMinOnscreenHeight,
                                 grid_size);
  if (bounds->y() > max_y)
    bounds->set_y(max_y);

  // Don't allow dragging above the top of the monitor.
  if (bounds->y() <= work_area.y())
    bounds->set_y(work_area.y());

  if (grid_size >= 0 && details_.window_component == HTCAPTION)
    SnapToWorkAreaEdges(work_area, bounds, grid_size);

  if (attached_windows_.empty())
    return;

  if (details_.window_component == HTRIGHT) {
    bounds->set_width(std::min(bounds->width(),
                               work_area.right() - total_min_ - bounds->x()));
  } else {
    DCHECK_EQ(HTBOTTOM, details_.window_component);
    bounds->set_height(std::min(bounds->height(),
                                work_area.bottom() - total_min_ - bounds->y()));
  }
}

void WorkspaceWindowResizer::SnapToWorkAreaEdges(
    const gfx::Rect& work_area,
    gfx::Rect* bounds,
    int grid_size) const {
  int left_edge = AlignToGridRoundUp(work_area.x(), grid_size);
  int right_edge = AlignToGridRoundDown(work_area.right(), grid_size);
  int top_edge = AlignToGridRoundUp(work_area.y(), grid_size);
  int bottom_edge = AlignToGridRoundDown(work_area.bottom(),
                                         grid_size);
  if (ShouldSnapToEdge(bounds->x() - left_edge, grid_size)) {
    bounds->set_x(left_edge);
  } else if (ShouldSnapToEdge(right_edge - bounds->right(),
                              grid_size)) {
    bounds->set_x(right_edge - bounds->width());
  }
  if (ShouldSnapToEdge(bounds->y() - top_edge, grid_size)) {
    bounds->set_y(top_edge);
  } else if (ShouldSnapToEdge(bottom_edge - bounds->bottom(), grid_size) &&
             bounds->height() < (bottom_edge - top_edge)) {
    // Only snap to the bottom if the window is smaller than the work area.
    // Doing otherwise can lead to window snapping in weird ways as it bounces
    // between snapping to top then bottom.
    bounds->set_y(bottom_edge - bounds->height());
  }
}

bool WorkspaceWindowResizer::TouchesBottomOfScreen() const {
  gfx::Rect work_area(
      gfx::Screen::GetMonitorNearestWindow(details_.window).work_area());
  return (attached_windows_.empty() &&
          details_.window->bounds().bottom() == work_area.bottom()) ||
      (!attached_windows_.empty() &&
       attached_windows_.back()->bounds().bottom() == work_area.bottom());
}

int WorkspaceWindowResizer::PrimaryAxisSize(const gfx::Size& size) const {
  return PrimaryAxisCoordinate(size.width(), size.height());
}

int WorkspaceWindowResizer::PrimaryAxisCoordinate(int x, int y) const {
  switch (details_.window_component) {
    case HTRIGHT:
      return x;
    case HTBOTTOM:
      return y;
    default:
      NOTREACHED();
  }
  return 0;
}

void WorkspaceWindowResizer::UpdatePhantomWindow(const gfx::Point& location,
                                                 const gfx::Rect& bounds,
                                                 int grid_size) {
  if (!did_move_or_resize_ || details_.window_component != HTCAPTION)
    return;

  SnapType last_type = snap_type_;
  snap_type_ = GetSnapType(location);
  if (snap_type_ == SNAP_NONE || snap_type_ != last_type) {
    phantom_window_controller_.reset();
    snap_sizer_.reset();
    if (snap_type_ == SNAP_NONE)
      return;
  }
  if (!snap_sizer_.get()) {
    SnapSizer::Edge edge = (snap_type_ == SNAP_LEFT_EDGE) ?
        SnapSizer::LEFT_EDGE : SnapSizer::RIGHT_EDGE;
    snap_sizer_.reset(
        new SnapSizer(details_.window, location, edge, grid_size));
  } else {
    snap_sizer_->Update(location);
  }
  if (!phantom_window_controller_.get()) {
    phantom_window_controller_.reset(
        new PhantomWindowController(details_.window));
  }
  phantom_window_controller_->Show(snap_sizer_->target_bounds());
}

void WorkspaceWindowResizer::RestackWindows() {
  if (attached_windows_.empty())
    return;
  // Build a map from index in children to window, returning if there is a
  // window with a different parent.
  typedef std::map<size_t, aura::Window*> IndexToWindowMap;
  IndexToWindowMap map;
  aura::Window* parent = details_.window->parent();
  const aura::Window::Windows& windows(parent->children());
  map[std::find(windows.begin(), windows.end(), details_.window) -
      windows.begin()] = details_.window;
  for (std::vector<aura::Window*>::const_iterator i =
           attached_windows_.begin(); i != attached_windows_.end(); ++i) {
    if ((*i)->parent() != parent)
      return;
    size_t index =
        std::find(windows.begin(), windows.end(), *i) - windows.begin();
    map[index] = *i;
  }

  // Reorder the windows starting at the topmost.
  parent->StackChildAtTop(map.rbegin()->second);
  for (IndexToWindowMap::const_reverse_iterator i = map.rbegin();
       i != map.rend(); ) {
    aura::Window* window = i->second;
    ++i;
    if (i != map.rend())
      parent->StackChildBelow(i->second, window);
  }
}

WorkspaceWindowResizer::SnapType WorkspaceWindowResizer::GetSnapType(
    const gfx::Point& location) const {
  // TODO: this likely only wants total monitor area, not the area of a single
  // monitor.
  gfx::Rect area(
      gfx::Screen::GetMonitorNearestWindow(details_.window).bounds());
  if (location.x() <= area.x())
    return SNAP_LEFT_EDGE;
  if (location.x() >= area.right() - 1)
    return SNAP_RIGHT_EDGE;
  return SNAP_NONE;
}

}  // namespace internal
}  // namespace ash
