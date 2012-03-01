// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_resizer.h"

#include "ash/shell.h"
#include "ash/wm/root_window_event_filter.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"

namespace ash {

namespace {

int GetPositionChangeDirectionForWindowComponent(int window_component) {
  int pos_change_direction = WindowResizer::kBoundsChangeDirection_None;
  switch (window_component) {
    case HTTOPLEFT:
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
    case HTCAPTION:
      pos_change_direction |=
          WindowResizer::kBoundsChangeDirection_Horizontal |
          WindowResizer::kBoundsChangeDirection_Vertical;
      break;
    case HTTOP:
    case HTTOPRIGHT:
    case HTBOTTOM:
      pos_change_direction |= WindowResizer::kBoundsChangeDirection_Vertical;
      break;
    case HTBOTTOMLEFT:
    case HTRIGHT:
    case HTLEFT:
      pos_change_direction |= WindowResizer::kBoundsChangeDirection_Horizontal;
      break;
    default:
      break;
  }
  return pos_change_direction;
}

int GetSizeChangeDirectionForWindowComponent(int window_component) {
  int size_change_direction = WindowResizer::kBoundsChangeDirection_None;
  switch (window_component) {
    case HTTOPLEFT:
    case HTTOPRIGHT:
    case HTBOTTOMLEFT:
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
    case HTCAPTION:
      size_change_direction |=
          WindowResizer::kBoundsChangeDirection_Horizontal |
          WindowResizer::kBoundsChangeDirection_Vertical;
      break;
    case HTTOP:
    case HTBOTTOM:
      size_change_direction |= WindowResizer::kBoundsChangeDirection_Vertical;
      break;
    case HTRIGHT:
    case HTLEFT:
      size_change_direction |= WindowResizer::kBoundsChangeDirection_Horizontal;
      break;
    default:
      break;
  }
  return size_change_direction;
}

// Returns true for resize components along the right edge, where a drag in
// positive x will make the window larger.
bool IsRightEdge(int window_component) {
  return window_component == HTTOPRIGHT ||
      window_component == HTRIGHT ||
      window_component == HTBOTTOMRIGHT ||
      window_component == HTGROWBOX;
}

// Returns true for resize components in along the bottom edge, where a drag
// in positive y will make the window larger.
bool IsBottomEdge(int window_component) {
  return window_component == HTBOTTOMLEFT ||
      window_component == HTBOTTOM ||
      window_component == HTBOTTOMRIGHT ||
      window_component == HTGROWBOX;
}

// Returns the closest location to |location| that is aligned to fall on
// increments of |grid_size|.
int AlignToGridRoundUp(int location, int grid_size) {
  if (grid_size <= 1 || location % grid_size == 0)
    return location;
  return (location / grid_size + 1) * grid_size;
}

gfx::Point ConvertPointToParent(aura::Window* window,
                                const gfx::Point& point) {
  gfx::Point result(point);
  aura::Window::ConvertPointToWindow(window, window->parent(), &result);
  return result;
}

}  // namespace

// static
const int WindowResizer::kBoundsChange_None = 0;
// static
const int WindowResizer::kBoundsChange_Repositions = 1;
// static
const int WindowResizer::kBoundsChange_Resizes = 2;

// static
const int WindowResizer::kBoundsChangeDirection_None = 0;
// static
const int WindowResizer::kBoundsChangeDirection_Horizontal = 1;
// static
const int WindowResizer::kBoundsChangeDirection_Vertical = 2;

WindowResizer::WindowResizer(aura::Window* window,
                             const gfx::Point& location,
                             int window_component,
                             int grid_size)
    : window_(window),
      initial_bounds_(window->bounds()),
      initial_location_in_parent_(ConvertPointToParent(window, location)),
      window_component_(window_component),
      bounds_change_(GetBoundsChangeForWindowComponent(window_component_)),
      position_change_direction_(
          GetPositionChangeDirectionForWindowComponent(window_component_)),
      size_change_direction_(
          GetSizeChangeDirectionForWindowComponent(window_component_)),
      is_resizable_(bounds_change_ != kBoundsChangeDirection_None),
      grid_size_(grid_size),
      did_move_or_resize_(false),
      root_filter_(NULL) {
  if (is_resizable_) {
    root_filter_ = Shell::GetInstance()->root_filter();
    if (root_filter_)
      root_filter_->LockCursor();
  }
}

WindowResizer::~WindowResizer() {
  if (root_filter_)
    root_filter_->UnlockCursor();
}

// static
int WindowResizer::GetBoundsChangeForWindowComponent(int component) {
  int bounds_change = WindowResizer::kBoundsChange_None;
  switch (component) {
    case HTTOPLEFT:
    case HTTOP:
    case HTTOPRIGHT:
    case HTLEFT:
    case HTBOTTOMLEFT:
      bounds_change |= WindowResizer::kBoundsChange_Repositions |
                      WindowResizer::kBoundsChange_Resizes;
      break;
    case HTCAPTION:
      bounds_change |= WindowResizer::kBoundsChange_Repositions;
      break;
    case HTRIGHT:
    case HTBOTTOMRIGHT:
    case HTBOTTOM:
    case HTGROWBOX:
      bounds_change |= WindowResizer::kBoundsChange_Resizes;
      break;
    default:
      break;
  }
  return bounds_change;
}

// static
int WindowResizer::AlignToGrid(int location, int grid_size) {
  if (grid_size <= 1 || location % grid_size == 0)
    return location;
  return floor(static_cast<float>(location) / static_cast<float>(grid_size) +
               .5f) * grid_size;
}

void WindowResizer::Drag(const gfx::Point& location) {
  gfx::Rect bounds = GetBoundsForDrag(location);
  if (bounds != window_->bounds()) {
    did_move_or_resize_ = true;
    window_->SetBounds(bounds);
  }
}

void WindowResizer::CompleteDrag() {
  if (grid_size_ <= 1 || !did_move_or_resize_)
    return;
  gfx::Rect new_bounds(GetFinalBounds());
  if (new_bounds == window_->bounds())
    return;

  if (new_bounds.size() != window_->bounds().size()) {
    // Don't attempt to animate a size change.
    window_->SetBounds(new_bounds);
    return;
  }

  ui::ScopedLayerAnimationSettings scoped_setter(
      window_->layer()->GetAnimator());
  // Use a small duration since the grid is small.
  scoped_setter.SetTransitionDuration(base::TimeDelta::FromMilliseconds(100));
  window_->SetBounds(new_bounds);
}

void WindowResizer::RevertDrag() {
  if (!did_move_or_resize_)
    return;

  window_->SetBounds(initial_bounds_);
}

gfx::Rect WindowResizer::GetBoundsForDrag(const gfx::Point& location) {
  if (!is_resizable())
    return window_->bounds();

  // Dragging a window moves the local coordinate frame, so do arithmetic
  // in the parent coordinate frame.
  gfx::Point event_location_in_parent(location);
  aura::Window::ConvertPointToWindow(window_, window_->parent(),
                                     &event_location_in_parent);
  int delta_x = event_location_in_parent.x() - initial_location_in_parent_.x();
  int delta_y = event_location_in_parent.y() - initial_location_in_parent_.y();

  // The minimize size constraint may limit how much we change the window
  // position.  For example, dragging the left edge to the right should stop
  // repositioning the window when the minimize size is reached.
  gfx::Size size = GetSizeForDrag(&delta_x, &delta_y);
  gfx::Point origin = GetOriginForDrag(delta_x, delta_y);

  gfx::Rect new_bounds(origin, size);
  // Update bottom edge to stay in the work area when we are resizing
  // by dragging the bottome edge or corners.
  if (bounds_change_ & kBoundsChange_Resizes &&
      origin.y() == window_->bounds().y()) {
    gfx::Rect work_area = gfx::Screen::GetMonitorWorkAreaNearestWindow(window_);
    if (new_bounds.bottom() > work_area.bottom())
      new_bounds.Inset(0, 0, 0,
                       new_bounds.bottom() - work_area.bottom());
  }
  if (bounds_change_ & kBoundsChange_Resizes &&
      bounds_change_ & kBoundsChange_Repositions && new_bounds.y() < 0) {
    int delta = new_bounds.y();
    new_bounds.set_y(0);
    new_bounds.set_height(new_bounds.height() + delta);
  }
  return new_bounds;
}

gfx::Rect WindowResizer::GetFinalBounds() {
  const gfx::Rect& bounds(window_->bounds());
  int x = AlignToGrid(bounds.x(), grid_size_);
  int y = AlignToGrid(bounds.y(), grid_size_);
  return gfx::Rect(x, y, bounds.width(), bounds.height());
}

gfx::Point WindowResizer::GetOriginForDrag(
    int delta_x,
    int delta_y) const {
  gfx::Point origin = initial_bounds_.origin();
  if (bounds_change_ & kBoundsChange_Repositions) {
    int pos_change_direction =
        GetPositionChangeDirectionForWindowComponent(window_component_);
    if (pos_change_direction & kBoundsChangeDirection_Horizontal)
      origin.Offset(delta_x, 0);
    if (pos_change_direction & kBoundsChangeDirection_Vertical)
      origin.Offset(0, delta_y);
  }
  return origin;
}

gfx::Size WindowResizer::GetSizeForDrag(
    int* delta_x,
    int* delta_y) const {
  gfx::Size size = initial_bounds_.size();
  if (bounds_change_ & kBoundsChange_Resizes) {
    gfx::Size min_size = window_->delegate()->GetMinimumSize();
    min_size.set_width(AlignToGridRoundUp(min_size.width(), grid_size_));
    min_size.set_height(AlignToGridRoundUp(min_size.height(), grid_size_));
    size.SetSize(GetWidthForDrag(min_size.width(), delta_x),
                 GetHeightForDrag(min_size.height(), delta_y));
  }
  return size;
}

int WindowResizer::GetWidthForDrag(int min_width, int* delta_x) const {
  int width = initial_bounds_.width();
  if (size_change_direction_ & kBoundsChangeDirection_Horizontal) {
    // Along the right edge, positive delta_x increases the window size.
    int x_multiplier = IsRightEdge(window_component_) ? 1 : -1;
    width += x_multiplier * (*delta_x);
    int adjusted_width = AlignToGrid(width, grid_size_);
    if (adjusted_width != width) {
      *delta_x += -x_multiplier * (width - adjusted_width);
      width = adjusted_width;
    }

    // Ensure we don't shrink past the minimum width and clamp delta_x
    // for the window origin computation.
    if (width < min_width) {
      width = min_width;
      *delta_x = -x_multiplier * (initial_bounds_.width() - min_width);
    }

    // And don't let the window go bigger than the monitor.
    int max_width =
        gfx::Screen::GetMonitorAreaNearestWindow(window_).width();
    if (width > max_width) {
      width = max_width;
      *delta_x = -x_multiplier * (initial_bounds_.width() - max_width);
    }
  }
  return width;
}

int WindowResizer::GetHeightForDrag(int min_height, int* delta_y) const {
  int height = initial_bounds_.height();
  if (size_change_direction_ & kBoundsChangeDirection_Vertical) {
    // Along the bottom edge, positive delta_y increases the window size.
    int y_multiplier = IsBottomEdge(window_component_) ? 1 : -1;
    height += y_multiplier * (*delta_y);
    int adjusted_height = AlignToGrid(height, grid_size_);
    if (height != adjusted_height) {
      *delta_y += -y_multiplier * (height - adjusted_height);
      height = adjusted_height;
    }

    // Ensure we don't shrink past the minimum height and clamp delta_y
    // for the window origin computation.
    if (height < min_height) {
      height = min_height;
      *delta_y = -y_multiplier * (initial_bounds_.height() - min_height);
    }

    // And don't let the window go bigger than the monitor.
    int max_height = gfx::Screen::GetMonitorAreaNearestWindow(window_).height();
    if (height > max_height) {
      height = max_height;
      *delta_y = -y_multiplier * (initial_bounds_.height() - max_height);
    }
  }
  return height;
}

}  // namespace aura
