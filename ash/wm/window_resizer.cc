// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_resizer.h"

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
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

WindowResizer::Details::Details()
    : window(NULL),
      window_component(HTNOWHERE),
      bounds_change(0),
      position_change_direction(0),
      size_change_direction(0),
      is_resizable(false) {
}

WindowResizer::Details::Details(aura::Window* window,
                                const gfx::Point& location,
                                int window_component)
  : window(window),
    initial_bounds(window->bounds()),
    restore_bounds(gfx::Rect()),
    initial_location_in_parent(location),
    initial_opacity(window->layer()->opacity()),
    window_component(window_component),
    bounds_change(GetBoundsChangeForWindowComponent(window_component)),
    position_change_direction(
        GetPositionChangeDirectionForWindowComponent(window_component)),
    size_change_direction(
        GetSizeChangeDirectionForWindowComponent(window_component)),
    is_resizable(bounds_change != kBoundsChangeDirection_None) {
  if (wm::IsWindowNormal(window) &&
      GetRestoreBoundsInScreen(window) &&
      window_component == HTCAPTION)
    restore_bounds = *GetRestoreBoundsInScreen(window);
}

WindowResizer::Details::~Details() {
}

WindowResizer::WindowResizer() {
}

WindowResizer::~WindowResizer() {
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
gfx::Rect WindowResizer::CalculateBoundsForDrag(
    const Details& details,
    const gfx::Point& location) {
  if (!details.is_resizable)
    return details.initial_bounds;

  int delta_x = location.x() - details.initial_location_in_parent.x();
  int delta_y = location.y() - details.initial_location_in_parent.y();

  // The minimize size constraint may limit how much we change the window
  // position.  For example, dragging the left edge to the right should stop
  // repositioning the window when the minimize size is reached.
  gfx::Size size = GetSizeForDrag(details, &delta_x, &delta_y);
  gfx::Point origin = GetOriginForDrag(details, delta_x, delta_y);

  // When we might want to reposition a window which is also restored to its
  // previous size, to keep the cursor within the dragged window.
  if (!details.restore_bounds.IsEmpty() &&
      details.bounds_change & kBoundsChange_Repositions) {
    // However - it is not desirable to change the origin if the window would
    // be still hit by the cursor.
    if (details.initial_location_in_parent.x() >
            details.initial_bounds.x() + details.restore_bounds.width())
      origin.set_x(location.x() - details.restore_bounds.width() / 2);
  }

  gfx::Rect new_bounds(origin, size);
  // Update bottom edge to stay in the work area when we are resizing
  // by dragging the bottome edge or corners.
  if (details.window_component == HTBOTTOM ||
      details.window_component == HTBOTTOMRIGHT ||
      details.window_component == HTBOTTOMLEFT) {
    gfx::Rect work_area =
        ScreenAsh::GetDisplayWorkAreaBoundsInParent(details.window);
    if (new_bounds.bottom() > work_area.bottom())
      new_bounds.Inset(0, 0, 0,
                       new_bounds.bottom() - work_area.bottom());
  }
  if (details.bounds_change & kBoundsChange_Resizes &&
      details.bounds_change & kBoundsChange_Repositions && new_bounds.y() < 0) {
    int delta = new_bounds.y();
    new_bounds.set_y(0);
    new_bounds.set_height(new_bounds.height() + delta);
  }
  return new_bounds;
}

// static
bool WindowResizer::IsBottomEdge(int window_component) {
  return window_component == HTBOTTOMLEFT ||
      window_component == HTBOTTOM ||
      window_component == HTBOTTOMRIGHT ||
      window_component == HTGROWBOX;
}

// static
gfx::Point WindowResizer::GetOriginForDrag(const Details& details,
                                           int delta_x,
                                           int delta_y) {
  gfx::Point origin = details.initial_bounds.origin();
  if (details.bounds_change & kBoundsChange_Repositions) {
    int pos_change_direction =
        GetPositionChangeDirectionForWindowComponent(details.window_component);
    if (pos_change_direction & kBoundsChangeDirection_Horizontal)
      origin.Offset(delta_x, 0);
    if (pos_change_direction & kBoundsChangeDirection_Vertical)
      origin.Offset(0, delta_y);
  }
  return origin;
}

// static
gfx::Size WindowResizer::GetSizeForDrag(const Details& details,
                                        int* delta_x,
                                        int* delta_y) {
  gfx::Size size = details.initial_bounds.size();
  if (details.bounds_change & kBoundsChange_Resizes) {
    gfx::Size min_size = details.window->delegate()->GetMinimumSize();
    size.SetSize(GetWidthForDrag(details, min_size.width(), delta_x),
                 GetHeightForDrag(details, min_size.height(), delta_y));
  } else if (!details.restore_bounds.IsEmpty()) {
    size = details.restore_bounds.size();
  }
  return size;
}

// static
int WindowResizer::GetWidthForDrag(const Details& details,
                                   int min_width,
                                   int* delta_x) {
  int width = details.initial_bounds.width();
  if (details.size_change_direction & kBoundsChangeDirection_Horizontal) {
    // Along the right edge, positive delta_x increases the window size.
    int x_multiplier = IsRightEdge(details.window_component) ? 1 : -1;
    width += x_multiplier * (*delta_x);

    // Ensure we don't shrink past the minimum width and clamp delta_x
    // for the window origin computation.
    if (width < min_width) {
      width = min_width;
      *delta_x = -x_multiplier * (details.initial_bounds.width() - min_width);
    }

    // And don't let the window go bigger than the display.
    int max_width =
        gfx::Screen::GetDisplayNearestWindow(details.window).bounds().width();
    if (width > max_width) {
      width = max_width;
      *delta_x = -x_multiplier * (details.initial_bounds.width() - max_width);
    }
  }
  return width;
}

// static
int WindowResizer::GetHeightForDrag(const Details& details,
                                    int min_height,
                                    int* delta_y) {
  int height = details.initial_bounds.height();
  if (details.size_change_direction & kBoundsChangeDirection_Vertical) {
    // Along the bottom edge, positive delta_y increases the window size.
    int y_multiplier = IsBottomEdge(details.window_component) ? 1 : -1;
    height += y_multiplier * (*delta_y);

    // Ensure we don't shrink past the minimum height and clamp delta_y
    // for the window origin computation.
    if (height < min_height) {
      height = min_height;
      *delta_y = -y_multiplier * (details.initial_bounds.height() - min_height);
    }

    // And don't let the window go bigger than the display.
    int max_height =
        gfx::Screen::GetDisplayNearestWindow(details.window).bounds().height();
    if (height > max_height) {
      height = max_height;
      *delta_y = -y_multiplier * (details.initial_bounds.height() - max_height);
    }
  }
  return height;
}

}  // namespace aura
