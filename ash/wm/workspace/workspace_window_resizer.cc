// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/wm/root_window_event_filter.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"

DECLARE_WINDOW_PROPERTY_TYPE(int)

namespace ash {
namespace internal {

namespace {

// Duration of the animation when snapping the window into place.
const int kSnapDurationMS = 100;

// Delay before the phantom window is shown (in milliseconds).
const int kPhantomDelayMS = 400;

const aura::WindowProperty<int> kHeightBeforeObscuredProp = {0};
const aura::WindowProperty<int>* const kHeightBeforeObscuredKey =
    &kHeightBeforeObscuredProp;

const aura::WindowProperty<int> kWidthBeforeObscuredProp = {0};
const aura::WindowProperty<int>* const kWidthBeforeObscuredKey =
    &kWidthBeforeObscuredProp;

void SetHeightBeforeObscured(aura::Window* window, int height) {
  window->SetProperty(kHeightBeforeObscuredKey, height);
}

int GetHeightBeforeObscured(aura::Window* window) {
  return window->GetProperty(kHeightBeforeObscuredKey);
}

void ClearHeightBeforeObscured(aura::Window* window) {
  window->SetProperty(kHeightBeforeObscuredKey, 0);
}

void SetWidthBeforeObscured(aura::Window* window, int width) {
  window->SetProperty(kWidthBeforeObscuredKey, width);
}

int GetWidthBeforeObscured(aura::Window* window) {
  return window->GetProperty(kWidthBeforeObscuredKey);
}

void ClearWidthBeforeObscured(aura::Window* window) {
  window->SetProperty(kWidthBeforeObscuredKey, 0);
}

}  // namespace

// static
const int WorkspaceWindowResizer::kMinOnscreenSize = 20;

WorkspaceWindowResizer::~WorkspaceWindowResizer() {
  if (root_filter_)
    root_filter_->UnlockCursor();
}

// static
WorkspaceWindowResizer* WorkspaceWindowResizer::Create(
    aura::Window* window,
    const gfx::Point& location,
    int window_component,
    int grid_size,
    const std::vector<aura::Window*>& attached_windows) {
  Details details(window, location, window_component, grid_size);
  return details.is_resizable ?
      new WorkspaceWindowResizer(details, attached_windows) : NULL;
}

void WorkspaceWindowResizer::Drag(const gfx::Point& location) {
  gfx::Rect bounds = CalculateBoundsForDrag(details_, location);
  if (constrain_size_)
    AdjustBoundsForMainWindow(&bounds);
  if (bounds != details_.window->bounds())
    did_move_or_resize_ = true;
  UpdatePhantomWindow(location);
  if (!attached_windows_.empty()) {
    if (details_.window_component == HTRIGHT)
      LayoutAttachedWindowsHorizontally(bounds);
    else
      LayoutAttachedWindowsVertically(bounds);
  }
  if (bounds != details_.window->bounds())
    details_.window->SetBounds(bounds);
  // WARNING: we may have been deleted.
}

void WorkspaceWindowResizer::CompleteDrag() {
  if (phantom_window_controller_.get()) {
    if (snap_type_ == SNAP_DESTINATION)
      phantom_window_controller_->DelayedClose(kSnapDurationMS);
    phantom_window_controller_.reset();
  }
  if (!did_move_or_resize_ || details_.window_component != HTCAPTION)
    return;

  if (snap_type_ == SNAP_LEFT_EDGE || snap_type_ == SNAP_RIGHT_EDGE) {
    if (!GetRestoreBounds(details_.window))
      SetRestoreBounds(details_.window, details_.initial_bounds);
    details_.window->SetBounds(snap_sizer_->target_bounds());
    return;
  }

  if (details_.grid_size <= 1)
    return;

  gfx::Rect bounds(GetFinalBounds());
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
      constrain_size_(wm::IsWindowNormal(details.window)),
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

  // We should never be in a situation where we have an attached window and not
  // constrain the size. The only case we don't constrain size is for dragging
  // tabs, which should never have an attached window.
  DCHECK(attached_windows_.empty() || constrain_size_);
  // Only support attaching to the right/bottom.
  DCHECK(attached_windows_.empty() ||
         (details.window_component == HTRIGHT ||
          details.window_component == HTBOTTOM));

  // TODO: figure out how to deal with window going off the edge.

  // Calculate sizes so that we can maintain the ratios if we need to resize.
  int total_available = 0;
  for (size_t i = 0; i < attached_windows_.size(); ++i) {
    gfx::Size min(attached_windows_[i]->delegate()->GetMinimumSize());
    int initial_size = PrimaryAxisSize(attached_windows_[i]->bounds().size());
    int cached_size = PrimaryAxisCoordinate(
        GetWidthBeforeObscured(attached_windows_[i]),
        GetHeightBeforeObscured(attached_windows_[i]));
    if (initial_size > cached_size) {
      if (details.window_component == HTRIGHT)
        ClearWidthBeforeObscured(attached_windows_[i]);
      else
        ClearHeightBeforeObscured(attached_windows_[i]);
    } else if (cached_size) {
      initial_size = cached_size;
    }
    initial_size_.push_back(initial_size);
    // If current size is smaller than the min, use the current size as the min.
    // This way we don't snap on resize.
    int min_size = std::min(initial_size,
                            std::max(PrimaryAxisSize(min), kMinOnscreenSize));
    // Make sure the min size falls on the grid.
    if (details_.grid_size > 1 && min_size % details_.grid_size != 0)
      min_size = (min_size / details_.grid_size + 1) * details_.grid_size;
    min_size_.push_back(min_size);
    total_min_ += min_size;
    total_initial_size_ += initial_size;
    total_available += std::max(min_size, initial_size) - min_size;
  }

  for (size_t i = 0; i < attached_windows_.size(); ++i) {
    if (total_initial_size_ != total_min_) {
      compress_fraction_.push_back(
          static_cast<float>(initial_size_[i] - min_size_[i]) /
          static_cast<float>(total_available));
    } else {
      compress_fraction_.push_back(0.0f);
    }
  }

  if (is_resizable() && constrain_size_ &&
      (!TouchesBottomOfScreen() ||
       details_.bounds_change != kBoundsChange_Repositions)) {
    ClearCachedHeights();
  }

  if (is_resizable() && constrain_size_ &&
      (!TouchesRightSideOfScreen() ||
       details_.bounds_change != kBoundsChange_Repositions)) {
    ClearCachedWidths();
  }
}

gfx::Rect WorkspaceWindowResizer::GetFinalBounds() const {
  if (phantom_window_controller_.get() &&
      phantom_window_controller_->IsShowing() &&
      phantom_window_controller_->type() ==
      PhantomWindowController::TYPE_EDGE) {
    return phantom_window_controller_->bounds();
  }
  gfx::Rect bounds(AdjustBoundsToGrid(details_));
  if (GetHeightBeforeObscured(window()) || constrain_size_) {
    // Two things can happen:
    // . We'll snap to the grid, which may result in different bounds. When
    //   dragging we only snap on release.
    // . If the bounds are different, and the windows height was truncated
    //   because it touched the bottom, than snapping to the grid may cause the
    //   window to no longer touch the bottom. Push it back up.
    gfx::Rect initial_bounds(window()->bounds());
    bool at_bottom = TouchesBottomOfScreen();
    if (at_bottom && bounds.y() != initial_bounds.y()) {
      if (bounds.y() < initial_bounds.y()) {
        bounds.set_height(bounds.height() + details_.grid_size -
                              (initial_bounds.y() - bounds.y()));
      }
      AdjustBoundsForMainWindow(&bounds);
    }
  }
  return bounds;
}

void WorkspaceWindowResizer::LayoutAttachedWindowsHorizontally(
    const gfx::Rect& bounds) {
  gfx::Rect work_area(gfx::Screen::GetMonitorWorkAreaNearestWindow(window()));
  int last_x = bounds.right();
  if (bounds.right() <= work_area.right() - total_initial_size_) {
    ClearCachedWidths();
    // All the windows fit at their initial size; tile them horizontally.
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      gfx::Rect attached_bounds(attached_windows_[i]->bounds());
      attached_bounds.set_x(last_x);
      attached_bounds.set_width(initial_size_[i]);
      attached_windows_[i]->SetBounds(attached_bounds);
      last_x = attached_bounds.right();
    }
  } else {
    DCHECK_NE(total_initial_size_, total_min_);
    int delta = total_initial_size_ - (work_area.right() - bounds.right());
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      gfx::Rect attached_bounds(attached_windows_[i]->bounds());
      int size = initial_size_[i] -
          static_cast<int>(compress_fraction_[i] * delta);
      size = AlignToGrid(size, details_.grid_size);
      if (!GetWidthBeforeObscured(attached_windows_[i]))
        SetWidthBeforeObscured(attached_windows_[i], attached_bounds.width());
      attached_bounds.set_x(last_x);
      if (i == attached_windows_.size())
        size = work_area.right() - last_x;
      attached_bounds.set_width(size);
      attached_windows_[i]->SetBounds(attached_bounds);
      last_x = attached_bounds.right();
    }
  }
}

void WorkspaceWindowResizer::LayoutAttachedWindowsVertically(
    const gfx::Rect& bounds) {
  gfx::Rect work_area(gfx::Screen::GetMonitorWorkAreaNearestWindow(window()));
  int last_y = bounds.bottom();
  if (bounds.bottom() <= work_area.bottom() - total_initial_size_) {
    ClearCachedHeights();
    // All the windows fit at their initial size; tile them vertically.
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      gfx::Rect attached_bounds(attached_windows_[i]->bounds());
      attached_bounds.set_y(last_y);
      attached_bounds.set_height(initial_size_[i]);
      attached_windows_[i]->SetBounds(attached_bounds);
      last_y = attached_bounds.bottom();
    }
  } else {
    DCHECK_NE(total_initial_size_, total_min_);
    int delta = total_initial_size_ - (work_area.bottom() - bounds.bottom());
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      gfx::Rect attached_bounds(attached_windows_[i]->bounds());
      int size = initial_size_[i] -
          static_cast<int>(compress_fraction_[i] * delta);
      size = AlignToGrid(size, details_.grid_size);
      if (i == attached_windows_.size())
        size = work_area.bottom() - last_y;
      if (!GetHeightBeforeObscured(attached_windows_[i]))
        SetHeightBeforeObscured(attached_windows_[i], attached_bounds.height());
      attached_bounds.set_height(size);
      attached_bounds.set_y(last_y);
      attached_windows_[i]->SetBounds(attached_bounds);
      last_y = attached_bounds.bottom();
    }
  }
}

void WorkspaceWindowResizer::AdjustBoundsForMainWindow(
    gfx::Rect* bounds) const {
  gfx::Rect work_area(gfx::Screen::GetMonitorWorkAreaNearestWindow(window()));
  if (!attached_windows_.empty() && details_.window_component == HTBOTTOM)
    work_area.set_height(work_area.height() - total_min_);
  AdjustBoundsForWindow(work_area, window(), bounds);

  if (!attached_windows_.empty() && details_.window_component == HTRIGHT) {
    bounds->set_width(std::min(bounds->width(),
                               work_area.right() - total_min_ - bounds->x()));
  }
}

void WorkspaceWindowResizer::AdjustBoundsForWindow(
    const gfx::Rect& work_area,
    aura::Window* window,
    gfx::Rect* bounds) const {
  if (bounds->bottom() < work_area.bottom()) {
    int height = GetHeightBeforeObscured(window);
    if (!height)
      return;
    height = std::max(bounds->height(), height);
    bounds->set_height(std::min(work_area.bottom() - bounds->y(), height));
    return;
  }

  if (bounds->bottom() == work_area.bottom())
    return;

  if (!GetHeightBeforeObscured(window))
    SetHeightBeforeObscured(window, window->bounds().height());

  gfx::Size min_size = window->delegate()->GetMinimumSize();
  bounds->set_height(
      std::max(0, work_area.bottom() - bounds->y()));
  if (bounds->height() < min_size.height()) {
    bounds->set_height(min_size.height());
    bounds->set_y(work_area.bottom() - min_size.height());
  }
}

void WorkspaceWindowResizer::ClearCachedHeights() {
  ClearHeightBeforeObscured(details_.window);
  for (size_t i = 0; i < attached_windows_.size(); ++i)
    ClearHeightBeforeObscured(attached_windows_[i]);
}

void WorkspaceWindowResizer::ClearCachedWidths() {
  ClearWidthBeforeObscured(details_.window);
  for (size_t i = 0; i < attached_windows_.size(); ++i)
    ClearWidthBeforeObscured(attached_windows_[i]);
}

bool WorkspaceWindowResizer::TouchesBottomOfScreen() const {
  gfx::Rect work_area(
      gfx::Screen::GetMonitorWorkAreaNearestWindow(details_.window));
  return (attached_windows_.empty() &&
          details_.window->bounds().bottom() == work_area.bottom()) ||
      (!attached_windows_.empty() &&
       attached_windows_.back()->bounds().bottom() == work_area.bottom());
}

bool WorkspaceWindowResizer::TouchesRightSideOfScreen() const {
  gfx::Rect work_area(
      gfx::Screen::GetMonitorWorkAreaNearestWindow(details_.window));
  return (attached_windows_.empty() &&
          details_.window->bounds().right() == work_area.right()) ||
      (!attached_windows_.empty() &&
       attached_windows_.back()->bounds().right() == work_area.right());
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

void WorkspaceWindowResizer::UpdatePhantomWindow(const gfx::Point& location) {
  if (!did_move_or_resize_ || details_.window_component != HTCAPTION)
    return;

  snap_type_ = GetSnapType(location);
  if (snap_type_ == SNAP_NONE) {
    phantom_window_controller_.reset();
    snap_sizer_.reset();
    return;
  }
  PhantomWindowController::Type phantom_type;
  if (snap_type_ == SNAP_LEFT_EDGE || snap_type_ == SNAP_RIGHT_EDGE)
    phantom_type = PhantomWindowController::TYPE_EDGE;
  else
    phantom_type = PhantomWindowController::TYPE_DESTINATION;
  if (phantom_window_controller_.get() &&
      phantom_window_controller_->type() != phantom_type) {
    phantom_window_controller_.reset();
    snap_sizer_.reset();
  }
  if (phantom_type == PhantomWindowController::TYPE_EDGE) {
    if (!snap_sizer_.get()) {
      SnapSizer::Edge edge = (snap_type_ == SNAP_LEFT_EDGE) ?
          SnapSizer::LEFT_EDGE : SnapSizer::RIGHT_EDGE;
      snap_sizer_.reset(
          new SnapSizer(details_.window, location, edge, details_.grid_size));
    } else {
      snap_sizer_->Update(location);
    }
  }
  if (!phantom_window_controller_.get()) {
    phantom_window_controller_.reset(
        new PhantomWindowController(details_.window, phantom_type,
                                    kPhantomDelayMS));
  }
  gfx::Rect bounds;
  if (snap_sizer_.get())
    bounds = snap_sizer_->target_bounds();
  else
    bounds = GetFinalBounds();
  phantom_window_controller_->Show(bounds);
}

WorkspaceWindowResizer::SnapType WorkspaceWindowResizer::GetSnapType(
    const gfx::Point& location) const {
  // TODO: this likely only wants total monitor area, not the area of a single
  // monitor.
  gfx::Rect area(gfx::Screen::GetMonitorAreaNearestWindow(details_.window));
  if (location.x() <= area.x())
    return SNAP_LEFT_EDGE;
  if (location.x() >= area.right() - 1)
    return SNAP_RIGHT_EDGE;
  if (details_.grid_size > 1 && wm::IsWindowNormal(details_.window))
    return SNAP_DESTINATION;
  return SNAP_NONE;
}

}  // namespace internal
}  // namespace ash
