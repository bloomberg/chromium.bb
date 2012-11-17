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
#include "ui/gfx/display.h"
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
      initial_bounds_in_parent(window->bounds()),
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
    const gfx::Point& passed_location) {
  if (!details.is_resizable)
    return details.initial_bounds_in_parent;

  gfx::Point location = passed_location;
  gfx::Rect work_area =
      ScreenAsh::GetDisplayWorkAreaBoundsInParent(details.window);

  int delta_x = location.x() - details.initial_location_in_parent.x();
  int delta_y = location.y() - details.initial_location_in_parent.y();

  // The minimize size constraint may limit how much we change the window
  // position.  For example, dragging the left edge to the right should stop
  // repositioning the window when the minimize size is reached.
  gfx::Size size = GetSizeForDrag(details, &delta_x, &delta_y);
  gfx::Point origin = GetOriginForDrag(details, delta_x, delta_y);
  gfx::Rect new_bounds(origin, size);

  // Sizing has to keep the result on the screen. Note that this correction
  // has to come first since it might have an impact on the origin as well as
  // on the size.
  if (details.bounds_change & kBoundsChange_Resizes) {
    if (details.size_change_direction & kBoundsChangeDirection_Horizontal) {
      if (IsRightEdge(details.window_component) &&
          new_bounds.right() < work_area.x() + kMinimumOnScreenArea) {
        int delta = work_area.x() + kMinimumOnScreenArea - new_bounds.right();
        new_bounds.set_width(new_bounds.width() + delta);
      } else if (new_bounds.x() > work_area.right() - kMinimumOnScreenArea) {
        int width = new_bounds.right() - work_area.right() +
                    kMinimumOnScreenArea;
        new_bounds.set_x(work_area.right() - kMinimumOnScreenArea);
        new_bounds.set_width(width);
      }
    }
    if (details.size_change_direction & kBoundsChangeDirection_Vertical) {
      if (!IsBottomEdge(details.window_component) &&
          new_bounds.y() > work_area.bottom() - kMinimumOnScreenArea) {
        int height = new_bounds.bottom() - work_area.bottom() +
                     kMinimumOnScreenArea;
        new_bounds.set_y(work_area.bottom() - kMinimumOnScreenArea);
        new_bounds.set_height(height);
      } else if (details.window_component == HTBOTTOM ||
                 details.window_component == HTBOTTOMRIGHT ||
                 details.window_component == HTBOTTOMLEFT) {
        // Update bottom edge to stay in the work area when we are resizing
        // by dragging the bottom edge or corners.
        if (new_bounds.bottom() > work_area.bottom())
          new_bounds.Inset(0, 0, 0,
                           new_bounds.bottom() - work_area.bottom());
      }
    }
    if (details.bounds_change & kBoundsChange_Repositions &&
        new_bounds.y() < 0) {
      int delta = new_bounds.y();
      new_bounds.set_y(0);
      new_bounds.set_height(new_bounds.height() + delta);
    }
  }

  if (details.bounds_change & kBoundsChange_Repositions) {
    // When we might want to reposition a window which is also restored to its
    // previous size, to keep the cursor within the dragged window.
    if (!details.restore_bounds.IsEmpty()) {
      // However - it is not desirable to change the origin if the window would
      // be still hit by the cursor.
      if (details.initial_location_in_parent.x() >
          details.initial_bounds_in_parent.x() + details.restore_bounds.width())
        new_bounds.set_x(location.x() - details.restore_bounds.width() / 2);
    }

    // Make sure that |new_bounds| doesn't leave any of the displays.  Note that
    // the |work_area| above isn't good for this check since it is the work area
    // for the current display but the window can move to a different one.
    aura::Window* parent = details.window->parent();
    gfx::Rect new_bounds_in_screen =
        ScreenAsh::ConvertRectToScreen(parent, new_bounds);
    const gfx::Display& display =
        Shell::GetScreen()->GetDisplayMatching(new_bounds_in_screen);
    gfx::Rect screen_work_area = display.work_area();
    screen_work_area.Inset(kMinimumOnScreenArea, 0);
    if (!screen_work_area.Intersects(new_bounds_in_screen)) {
      // Make sure that the x origin does not leave the current display.
      new_bounds_in_screen.set_x(
          std::max(screen_work_area.x() - new_bounds.width(),
                   std::min(screen_work_area.right(),
                            new_bounds_in_screen.x())));
      new_bounds =
          ScreenAsh::ConvertRectFromScreen(parent, new_bounds_in_screen);
    }
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
  gfx::Point origin = details.initial_bounds_in_parent.origin();
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
  gfx::Size size = details.initial_bounds_in_parent.size();
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
  int width = details.initial_bounds_in_parent.width();
  if (details.size_change_direction & kBoundsChangeDirection_Horizontal) {
    // Along the right edge, positive delta_x increases the window size.
    int x_multiplier = IsRightEdge(details.window_component) ? 1 : -1;
    width += x_multiplier * (*delta_x);

    // Ensure we don't shrink past the minimum width and clamp delta_x
    // for the window origin computation.
    if (width < min_width) {
      width = min_width;
      *delta_x = -x_multiplier * (details.initial_bounds_in_parent.width() -
                                  min_width);
    }

    // And don't let the window go bigger than the display.
    int max_width = Shell::GetScreen()->GetDisplayNearestWindow(
        details.window).bounds().width();
    gfx::Size max_size = details.window->delegate()->GetMaximumSize();
    if (max_size.width() != 0)
      max_width = std::min(max_width, max_size.width());
    if (width > max_width) {
      width = max_width;
      *delta_x = -x_multiplier * (details.initial_bounds_in_parent.width() -
                                  max_width);
    }
  }
  return width;
}

// static
int WindowResizer::GetHeightForDrag(const Details& details,
                                    int min_height,
                                    int* delta_y) {
  int height = details.initial_bounds_in_parent.height();
  if (details.size_change_direction & kBoundsChangeDirection_Vertical) {
    // Along the bottom edge, positive delta_y increases the window size.
    int y_multiplier = IsBottomEdge(details.window_component) ? 1 : -1;
    height += y_multiplier * (*delta_y);

    // Ensure we don't shrink past the minimum height and clamp delta_y
    // for the window origin computation.
    if (height < min_height) {
      height = min_height;
      *delta_y = -y_multiplier * (details.initial_bounds_in_parent.height() -
                                  min_height);
    }

    // And don't let the window go bigger than the display.
    int max_height = Shell::GetScreen()->GetDisplayNearestWindow(
        details.window).bounds().height();
    gfx::Size max_size = details.window->delegate()->GetMaximumSize();
    if (max_size.height() != 0)
      max_height = std::min(max_height, max_size.height());
    if (height > max_height) {
      height = max_height;
      *delta_y = -y_multiplier * (details.initial_bounds_in_parent.height() -
                                  max_height);
    }
  }
  return height;
}

}  // namespace aura
