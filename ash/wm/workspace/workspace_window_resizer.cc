// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "ash/display/display_controller.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/cursor_manager.h"
#include "ash/wm/default_window_resizer.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"

namespace ash {

scoped_ptr<WindowResizer> CreateWindowResizer(aura::Window* window,
                                              const gfx::Point& point_in_parent,
                                              int window_component) {
  DCHECK(window);
  // No need to return a resizer when the window cannot get resized.
  if (!wm::CanResizeWindow(window) && window_component != HTCAPTION)
    return scoped_ptr<WindowResizer>();

  if (window->parent() &&
      window->parent()->id() == internal::kShellWindowId_WorkspaceContainer) {
    // Allow dragging maximized windows if it's not tracked by workspace. This
    // is set by tab dragging code.
    if (!wm::IsWindowNormal(window) &&
        (window_component != HTCAPTION || GetTrackedByWorkspace(window)))
      return scoped_ptr<WindowResizer>();
    return make_scoped_ptr<WindowResizer>(
        internal::WorkspaceWindowResizer::Create(window,
                                                 point_in_parent,
                                                 window_component,
                                                 std::vector<aura::Window*>()));
  } else if (wm::IsWindowNormal(window)) {
    return make_scoped_ptr<WindowResizer>(DefaultWindowResizer::Create(
        window,
        point_in_parent,
        window_component));
  } else {
    return scoped_ptr<WindowResizer>();
  }
}

namespace internal {

namespace {

// Duration of the animation when snapping the window into place.
const int kSnapDurationMS = 100;

// The maximum opacity of the drag phantom window.
const float kMaxOpacity = 0.8f;

// Returns true if should snap to the edge.
bool ShouldSnapToEdge(int distance_from_edge, int grid_size) {
  return distance_from_edge < grid_size &&
      distance_from_edge > -grid_size * 2;
}

// Returns true if Ash has more than one root window.
bool HasSecondaryRootWindow() {
  return Shell::GetAllRootWindows().size() > 1;
}

// When there are two root windows, returns one of the root windows which is not
// |root_window|. Returns NULL if only one root window exists.
aura::RootWindow* GetAnotherRootWindow(aura::RootWindow* root_window) {
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  if (root_windows.size() < 2)
    return NULL;
  DCHECK_EQ(2U, root_windows.size());
  if (root_windows[0] == root_window)
    return root_windows[1];
  return root_windows[0];
}

// Returns the coordinate along the secondary axis to snap to.
int CoordinateAlongSecondaryAxis(SecondaryMagnetismEdge edge,
                                 int leading,
                                 int trailing,
                                 int none) {
  switch (edge) {
    case SECONDARY_MAGNETISM_EDGE_LEADING:
      return leading;
    case SECONDARY_MAGNETISM_EDGE_TRAILING:
      return trailing;
    case SECONDARY_MAGNETISM_EDGE_NONE:
      return none;
  }
  NOTREACHED();
  return none;
}

// Returns the origin for |src| when magnetically attaching to |attach_to| along
// the edges |edges|. |edges| is a bitmask of the MagnetismEdges.
gfx::Point OriginForMagneticAttach(const gfx::Rect& src,
                                   const gfx::Rect& attach_to,
                                   const MatchedEdge& edge) {
  int x = 0, y = 0;
  switch (edge.primary_edge) {
    case MAGNETISM_EDGE_TOP:
      y = attach_to.bottom();
      break;
    case MAGNETISM_EDGE_LEFT:
      x = attach_to.right();
      break;
    case MAGNETISM_EDGE_BOTTOM:
      y = attach_to.y() - src.height();
      break;
    case MAGNETISM_EDGE_RIGHT:
      x = attach_to.x() - src.width();
      break;
  }
  switch (edge.primary_edge) {
    case MAGNETISM_EDGE_TOP:
    case MAGNETISM_EDGE_BOTTOM:
      x = CoordinateAlongSecondaryAxis(
          edge.secondary_edge, attach_to.x(), attach_to.right() - src.width(),
          src.x());
      break;
    case MAGNETISM_EDGE_LEFT:
    case MAGNETISM_EDGE_RIGHT:
      y = CoordinateAlongSecondaryAxis(
          edge.secondary_edge, attach_to.y(), attach_to.bottom() - src.height(),
          src.y());
      break;
  }
  return gfx::Point(x, y);
}

// Returns the bounds for a magnetic attach when resizing. |src| is the bounds
// of window being resized, |attach_to| the bounds of the window to attach to
// and |edge| identifies the edge to attach to.
gfx::Rect BoundsForMagneticResizeAttach(const gfx::Rect& src,
                                        const gfx::Rect& attach_to,
                                        const MatchedEdge& edge) {
  int x = src.x();
  int y = src.y();
  int w = src.width();
  int h = src.height();
  gfx::Point attach_origin(OriginForMagneticAttach(src, attach_to, edge));
  switch (edge.primary_edge) {
    case MAGNETISM_EDGE_LEFT:
      x = attach_origin.x();
      w = src.right() - x;
      break;
    case MAGNETISM_EDGE_RIGHT:
      w += attach_origin.x() - src.x();
      break;
    case MAGNETISM_EDGE_TOP:
      y = attach_origin.y();
      h = src.bottom() - y;
      break;
    case MAGNETISM_EDGE_BOTTOM:
      h += attach_origin.y() - src.y();
      break;
  }
  switch (edge.primary_edge) {
    case MAGNETISM_EDGE_LEFT:
    case MAGNETISM_EDGE_RIGHT:
      if (edge.secondary_edge == SECONDARY_MAGNETISM_EDGE_LEADING) {
        y = attach_origin.y();
        h = src.bottom() - y;
      } else if (edge.secondary_edge == SECONDARY_MAGNETISM_EDGE_TRAILING) {
        h += attach_origin.y() - src.y();
      }
      break;
    case MAGNETISM_EDGE_TOP:
    case MAGNETISM_EDGE_BOTTOM:
      if (edge.secondary_edge == SECONDARY_MAGNETISM_EDGE_LEADING) {
        x = attach_origin.x();
        w = src.right() - x;
      } else if (edge.secondary_edge == SECONDARY_MAGNETISM_EDGE_TRAILING) {
        w += attach_origin.x() - src.x();
      }
      break;
  }
  return gfx::Rect(x, y, w, h);
}

// Converts a window comopnent edge to the magnetic edge to snap to.
uint32 WindowComponentToMagneticEdge(int window_component) {
  switch (window_component) {
    case HTTOPLEFT:
      return MAGNETISM_EDGE_LEFT | MAGNETISM_EDGE_TOP;
    case HTTOPRIGHT:
      return MAGNETISM_EDGE_TOP | MAGNETISM_EDGE_RIGHT;
    case HTBOTTOMLEFT:
      return MAGNETISM_EDGE_LEFT | MAGNETISM_EDGE_BOTTOM;
    case HTBOTTOMRIGHT:
      return MAGNETISM_EDGE_RIGHT | MAGNETISM_EDGE_BOTTOM;
    case HTTOP:
      return MAGNETISM_EDGE_TOP;
    case HTBOTTOM:
      return MAGNETISM_EDGE_BOTTOM;
    case HTRIGHT:
      return MAGNETISM_EDGE_RIGHT;
    case HTLEFT:
      return MAGNETISM_EDGE_LEFT;
    default:
      break;
  }
  return 0;
}

}  // namespace

// static
const int WorkspaceWindowResizer::kMinOnscreenSize = 20;

// static
const int WorkspaceWindowResizer::kMinOnscreenHeight = 32;

// static
const int WorkspaceWindowResizer::kScreenEdgeInset = 8;

WorkspaceWindowResizer::~WorkspaceWindowResizer() {
  Shell* shell = Shell::GetInstance();
  shell->mouse_cursor_filter()->set_mouse_warp_mode(
      MouseCursorEventFilter::WARP_ALWAYS);
  shell->mouse_cursor_filter()->HideSharedEdgeIndicator();
  shell->cursor_manager()->UnlockCursor();

  // Delete phantom controllers first so that they will never see the deleted
  // |layer_|.
  snap_phantom_window_controller_.reset();
  drag_phantom_window_controller_.reset();

  if (layer_)
    wm::DeepDeleteLayers(layer_);

  if (destroyed_)
    *destroyed_ = true;
}

// static
WorkspaceWindowResizer* WorkspaceWindowResizer::Create(
    aura::Window* window,
    const gfx::Point& location_in_parent,
    int window_component,
    const std::vector<aura::Window*>& attached_windows) {
  Details details(window, location_in_parent, window_component);
  return details.is_resizable ?
      new WorkspaceWindowResizer(details, attached_windows) : NULL;
}

void WorkspaceWindowResizer::Drag(const gfx::Point& location_in_parent,
                                  int event_flags) {
  last_mouse_location_ = location_in_parent;

  const int snap_size =
      event_flags & ui::EF_CONTROL_DOWN ? 0 : kScreenEdgeInset;
  // |bounds| is in |window()->parent()|'s coordinates.
  gfx::Rect bounds = CalculateBoundsForDrag(details_, location_in_parent);

  if (wm::IsWindowNormal(window()))
    AdjustBoundsForMainWindow(snap_size, &bounds);

  if (bounds != window()->bounds()) {
    if (!did_move_or_resize_) {
      if (!details_.restore_bounds.IsEmpty())
        ClearRestoreBounds(window());
      RestackWindows();
    }
    did_move_or_resize_ = true;
  }

  gfx::Point location_in_screen = location_in_parent;
  wm::ConvertPointToScreen(window()->parent(), &location_in_screen);
  const bool in_original_root =
      wm::GetRootWindowAt(location_in_screen) == window()->GetRootWindow();
  // Hide a phantom window for snapping if the cursor is in another root window.
  if (in_original_root && wm::CanResizeWindow(window())) {
    UpdateSnapPhantomWindow(location_in_parent, bounds);
  } else {
    snap_type_ = SNAP_NONE;
    snap_phantom_window_controller_.reset();
  }

  if (!attached_windows_.empty())
    LayoutAttachedWindows(bounds);
  if (bounds != window()->bounds()) {
    bool destroyed = false;
    destroyed_ = &destroyed;
    window()->SetBounds(bounds);
    if (destroyed)
      return;
    destroyed_ = NULL;
  }
  // Show a phantom window for dragging in another root window.
  if (HasSecondaryRootWindow())
    UpdateDragPhantomWindow(bounds, in_original_root);
  else
    drag_phantom_window_controller_.reset();

}

void WorkspaceWindowResizer::CompleteDrag(int event_flags) {
  wm::SetUserHasChangedWindowPositionOrSize(details_.window, true);
  window()->layer()->SetOpacity(details_.initial_opacity);
  drag_phantom_window_controller_.reset();
  snap_phantom_window_controller_.reset();
  if (!did_move_or_resize_ || details_.window_component != HTCAPTION)
    return;

  // When the window is not in the normal show state, we do not snap thw window.
  // This happens when the user minimizes or maximizes the window by keyboard
  // shortcut while dragging it. If the window is the result of dragging a tab
  // out of a maximized window, it's already in the normal show state when this
  // is called, so it does not matter.
  if (wm::IsWindowNormal(window()) &&
      (snap_type_ == SNAP_LEFT_EDGE || snap_type_ == SNAP_RIGHT_EDGE)) {
    if (!GetRestoreBoundsInScreen(window())) {
      gfx::Rect initial_bounds = ScreenAsh::ConvertRectToScreen(
          window()->parent(), details_.initial_bounds_in_parent);
      SetRestoreBoundsInScreen(window(), details_.restore_bounds.IsEmpty() ?
                               initial_bounds :
                               details_.restore_bounds);
    }
    window()->SetBounds(snap_sizer_->target_bounds());
    return;
  }
  gfx::Rect bounds(GetFinalBounds(window()->bounds()));

  // Check if the destination is another display.
  gfx::Point last_mouse_location_in_screen = last_mouse_location_;
  wm::ConvertPointToScreen(window()->parent(), &last_mouse_location_in_screen);
  gfx::Screen* screen = Shell::GetScreen();
  const gfx::Display dst_display =
      screen->GetDisplayNearestPoint(last_mouse_location_in_screen);

  if (dst_display.id() !=
      screen->GetDisplayNearestWindow(window()->GetRootWindow()).id()) {
    // Don't animate when moving to another display.
    const gfx::Rect dst_bounds =
        ScreenAsh::ConvertRectToScreen(window()->parent(), bounds);
    window()->SetBoundsInScreen(dst_bounds, dst_display);
  }
}

void WorkspaceWindowResizer::RevertDrag() {
  window()->layer()->SetOpacity(details_.initial_opacity);
  drag_phantom_window_controller_.reset();
  snap_phantom_window_controller_.reset();
  Shell::GetInstance()->mouse_cursor_filter()->HideSharedEdgeIndicator();

  if (!did_move_or_resize_)
    return;

  window()->SetBounds(details_.initial_bounds_in_parent);
  if (!details_.restore_bounds.IsEmpty())
    SetRestoreBoundsInScreen(details_.window, details_.restore_bounds);

  if (details_.window_component == HTRIGHT) {
    int last_x = details_.initial_bounds_in_parent.right();
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      gfx::Rect bounds(attached_windows_[i]->bounds());
      bounds.set_x(last_x);
      bounds.set_width(initial_size_[i]);
      attached_windows_[i]->SetBounds(bounds);
      last_x = attached_windows_[i]->bounds().right();
    }
  } else {
    int last_y = details_.initial_bounds_in_parent.bottom();
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      gfx::Rect bounds(attached_windows_[i]->bounds());
      bounds.set_y(last_y);
      bounds.set_height(initial_size_[i]);
      attached_windows_[i]->SetBounds(bounds);
      last_y = attached_windows_[i]->bounds().bottom();
    }
  }
}

aura::Window* WorkspaceWindowResizer::GetTarget() {
  return details_.window;
}

WorkspaceWindowResizer::WorkspaceWindowResizer(
    const Details& details,
    const std::vector<aura::Window*>& attached_windows)
    : details_(details),
      attached_windows_(attached_windows),
      did_move_or_resize_(false),
      total_min_(0),
      total_initial_size_(0),
      snap_type_(SNAP_NONE),
      num_mouse_moves_since_bounds_change_(0),
      layer_(NULL),
      destroyed_(NULL),
      magnetism_window_(NULL) {
  DCHECK(details_.is_resizable);

  Shell* shell = Shell::GetInstance();
  shell->cursor_manager()->LockCursor();

  // The pointer should be confined in one display during resizing a window
  // because the window cannot span two displays at the same time anyway. The
  // exception is window/tab dragging operation. During that operation,
  // |mouse_warp_mode_| should be set to WARP_DRAG so that the user could move a
  // window/tab to another display.
  MouseCursorEventFilter* mouse_cursor_filter = shell->mouse_cursor_filter();
  mouse_cursor_filter->set_mouse_warp_mode(
      ShouldAllowMouseWarp() ?
      MouseCursorEventFilter::WARP_DRAG : MouseCursorEventFilter::WARP_NONE);
  if (ShouldAllowMouseWarp()) {
    mouse_cursor_filter->ShowSharedEdgeIndicator(
        details.window->GetRootWindow());
  }

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
    initial_size_.push_back(initial_size);
    // If current size is smaller than the min, use the current size as the min.
    // This way we don't snap on resize.
    int min_size = std::min(initial_size,
                            std::max(PrimaryAxisSize(min), kMinOnscreenSize));
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
    const gfx::Rect& bounds) const {
  if (snap_phantom_window_controller_.get() &&
      snap_phantom_window_controller_->IsShowing()) {
    return snap_phantom_window_controller_->bounds();
  }
  return bounds;
}

void WorkspaceWindowResizer::LayoutAttachedWindows(
    const gfx::Rect& bounds) {
  gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(window()));
  std::vector<int> sizes;
  CalculateAttachedSizes(
      PrimaryAxisSize(details_.initial_bounds_in_parent.size()),
      PrimaryAxisSize(bounds.size()),
      PrimaryAxisCoordinate(bounds.right(), bounds.bottom()),
      PrimaryAxisCoordinate(work_area.right(), work_area.bottom()),
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
    std::vector<int>* sizes) const {
  sizes->clear();
  if (current_size < initial_size) {
    // If the primary window is sized smaller, resize the attached windows.
    int current = start;
    int delta = initial_size - current_size;
    for (size_t i = 0; i < attached_windows_.size(); ++i) {
      int next = current + initial_size_[i] + expand_fraction_[i] * delta;
      if (i + 1 == attached_windows_.size())
        next = start + total_initial_size_ + (initial_size - current_size);
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
      if (i + 1 == attached_windows_.size())
        size = end - current;
      current += size;
      sizes->push_back(size);
    }
  }
}

void WorkspaceWindowResizer::MagneticallySnapToOtherWindows(gfx::Rect* bounds) {
  if (UpdateMagnetismWindow(*bounds, kAllMagnetismEdges)) {
    bounds->set_origin(
        OriginForMagneticAttach(*bounds, magnetism_window_->bounds(),
                                magnetism_edge_));
  }
}

void WorkspaceWindowResizer::MagneticallySnapResizeToOtherWindows(
    gfx::Rect* bounds) {
  const uint32 edges = WindowComponentToMagneticEdge(details_.window_component);
  if (UpdateMagnetismWindow(*bounds, edges)) {
    *bounds = BoundsForMagneticResizeAttach(
          *bounds, magnetism_window_->bounds(), magnetism_edge_);
  }
}

bool WorkspaceWindowResizer::UpdateMagnetismWindow(const gfx::Rect& bounds,
                                                    uint32 edges) {
  MagnetismMatcher matcher(bounds, edges);

  // If we snapped to a window then check it first. That way we don't bounce
  // around when close to multiple edges.
  if (magnetism_window_) {
    if (window_tracker_.Contains(magnetism_window_) &&
        matcher.ShouldAttach(magnetism_window_->bounds(), &magnetism_edge_)) {
      return true;
    }
    window_tracker_.Remove(magnetism_window_);
    magnetism_window_ = NULL;
  }

  aura::Window* parent = window()->parent();
  const aura::Window::Windows& windows(parent->children());
  for (aura::Window::Windows::const_reverse_iterator i = windows.rbegin();
       i != windows.rend() && !matcher.AreEdgesObscured(); ++i) {
    aura::Window* other = *i;
    if (other == window() || !other->IsVisible())
      continue;
    if (matcher.ShouldAttach(other->bounds(), &magnetism_edge_)) {
      magnetism_window_ = other;
      window_tracker_.Add(magnetism_window_);
      return true;
    }
  }
  return false;
}

void WorkspaceWindowResizer::AdjustBoundsForMainWindow(
    int snap_size,
    gfx::Rect* bounds) {
  gfx::Point last_mouse_location_in_screen = last_mouse_location_;
  wm::ConvertPointToScreen(window()->parent(), &last_mouse_location_in_screen);
  gfx::Display display = Shell::GetScreen()->GetDisplayNearestPoint(
      last_mouse_location_in_screen);
  gfx::Rect work_area =
      ScreenAsh::ConvertRectFromScreen(window()->parent(), display.work_area());
  if (details_.window_component == HTCAPTION) {
    // Adjust the bounds to the work area where the mouse cursor is located.
    // Always keep kMinOnscreenHeight on the bottom.
    int max_y = work_area.bottom() - kMinOnscreenHeight;
    if (bounds->y() > max_y) {
      bounds->set_y(max_y);
    } else if (bounds->y() <= work_area.y()) {
      // Don't allow dragging above the top of the display until the mouse
      // cursor reaches the work area above if any.
      bounds->set_y(work_area.y());
    }

    if (snap_size > 0) {
      SnapToWorkAreaEdges(work_area, snap_size, bounds);
      MagneticallySnapToOtherWindows(bounds);
    }
  } else if (snap_size > 0) {
    MagneticallySnapResizeToOtherWindows(bounds);
    if (!magnetism_window_ && snap_size > 0)
      SnapResizeToWorkAreaBounds(work_area, snap_size, bounds);
  }

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
    int snap_size,
    gfx::Rect* bounds) const {
  const int left_edge = work_area.x();
  const int right_edge = work_area.right();
  const int top_edge = work_area.y();
  const int bottom_edge = work_area.bottom();
  if (ShouldSnapToEdge(bounds->x() - left_edge, snap_size)) {
    bounds->set_x(left_edge);
  } else if (ShouldSnapToEdge(right_edge - bounds->right(),
                              snap_size)) {
    bounds->set_x(right_edge - bounds->width());
  }
  if (ShouldSnapToEdge(bounds->y() - top_edge, snap_size)) {
    bounds->set_y(top_edge);
  } else if (ShouldSnapToEdge(bottom_edge - bounds->bottom(), snap_size) &&
             bounds->height() < (bottom_edge - top_edge)) {
    // Only snap to the bottom if the window is smaller than the work area.
    // Doing otherwise can lead to window snapping in weird ways as it bounces
    // between snapping to top then bottom.
    bounds->set_y(bottom_edge - bounds->height());
  }
}

void WorkspaceWindowResizer::SnapResizeToWorkAreaBounds(
    const gfx::Rect& work_area,
    int snap_size,
    gfx::Rect* bounds) const {
  const uint32 edges = WindowComponentToMagneticEdge(details_.window_component);
  const int left_edge = work_area.x();
  const int right_edge = work_area.right();
  const int top_edge = work_area.y();
  const int bottom_edge = work_area.bottom();
  if (edges & MAGNETISM_EDGE_TOP &&
      ShouldSnapToEdge(bounds->y() - top_edge, snap_size)) {
    bounds->set_height(bounds->bottom() - top_edge);
    bounds->set_y(top_edge);
  }
  if (edges & MAGNETISM_EDGE_LEFT &&
      ShouldSnapToEdge(bounds->x() - left_edge, snap_size)) {
    bounds->set_width(bounds->right() - left_edge);
    bounds->set_x(left_edge);
  }
  if (edges & MAGNETISM_EDGE_BOTTOM &&
      ShouldSnapToEdge(bottom_edge - bounds->bottom(), snap_size)) {
    bounds->set_height(bottom_edge - bounds->y());
  }
  if (edges & MAGNETISM_EDGE_RIGHT &&
      ShouldSnapToEdge(right_edge - bounds->right(), snap_size)) {
    bounds->set_width(right_edge - bounds->x());
  }
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

void WorkspaceWindowResizer::UpdateDragPhantomWindow(const gfx::Rect& bounds,
                                                     bool in_original_root) {
  if (!did_move_or_resize_ || details_.window_component != HTCAPTION ||
      !ShouldAllowMouseWarp()) {
    return;
  }

  // It's available. Show a phantom window on the display if needed.
  aura::RootWindow* another_root =
      GetAnotherRootWindow(window()->GetRootWindow());
  const gfx::Rect root_bounds_in_screen(another_root->GetBoundsInScreen());
  const gfx::Rect bounds_in_screen =
      ScreenAsh::ConvertRectToScreen(window()->parent(), bounds);
  gfx::Rect bounds_in_another_root =
      gfx::IntersectRects(root_bounds_in_screen, bounds_in_screen);

  const float fraction_in_another_window =
      (bounds_in_another_root.width() * bounds_in_another_root.height()) /
      static_cast<float>(bounds.width() * bounds.height());
  const float phantom_opacity =
      !in_original_root ? 1 : (kMaxOpacity * fraction_in_another_window);
  const float window_opacity =
      in_original_root ? 1 : (kMaxOpacity * (1 - fraction_in_another_window));

  if (fraction_in_another_window > 0) {
    if (!drag_phantom_window_controller_.get()) {
      drag_phantom_window_controller_.reset(
          new PhantomWindowController(window()));
      drag_phantom_window_controller_->set_style(
          PhantomWindowController::STYLE_DRAGGING);
      // Always show the drag phantom on the |another_root| window.
      drag_phantom_window_controller_->SetDestinationDisplay(
          Shell::GetScreen()->GetDisplayMatching(
              another_root->GetBoundsInScreen()));
      if (!layer_)
        RecreateWindowLayers();
      drag_phantom_window_controller_->Show(bounds_in_screen, layer_);
    } else {
      // No animation.
      drag_phantom_window_controller_->SetBounds(bounds_in_screen);
    }
    drag_phantom_window_controller_->SetOpacity(phantom_opacity);
    window()->layer()->SetOpacity(window_opacity);
  } else {
    drag_phantom_window_controller_.reset();
    window()->layer()->SetOpacity(1.0f);
  }
}

void WorkspaceWindowResizer::UpdateSnapPhantomWindow(const gfx::Point& location,
                                                     const gfx::Rect& bounds) {
  if (!did_move_or_resize_ || details_.window_component != HTCAPTION)
    return;

  SnapType last_type = snap_type_;
  snap_type_ = GetSnapType(location);
  if (snap_type_ == SNAP_NONE || snap_type_ != last_type) {
    snap_phantom_window_controller_.reset();
    snap_sizer_.reset();
    if (snap_type_ == SNAP_NONE)
      return;
  }
  if (!snap_sizer_.get()) {
    SnapSizer::Edge edge = (snap_type_ == SNAP_LEFT_EDGE) ?
        SnapSizer::LEFT_EDGE : SnapSizer::RIGHT_EDGE;
    snap_sizer_.reset(new SnapSizer(window(),
                                    location,
                                    edge,
                                    internal::SnapSizer::OTHER_INPUT));
  } else {
    snap_sizer_->Update(location);
  }
  if (!snap_phantom_window_controller_.get()) {
    snap_phantom_window_controller_.reset(
        new PhantomWindowController(window()));
  }
  snap_phantom_window_controller_->Show(ScreenAsh::ConvertRectToScreen(
      window()->parent(), snap_sizer_->target_bounds()), NULL);
}

void WorkspaceWindowResizer::RestackWindows() {
  if (attached_windows_.empty())
    return;
  // Build a map from index in children to window, returning if there is a
  // window with a different parent.
  typedef std::map<size_t, aura::Window*> IndexToWindowMap;
  IndexToWindowMap map;
  aura::Window* parent = window()->parent();
  const aura::Window::Windows& windows(parent->children());
  map[std::find(windows.begin(), windows.end(), window()) -
      windows.begin()] = window();
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
  // TODO: this likely only wants total display area, not the area of a single
  // display.
  gfx::Rect area(ScreenAsh::GetDisplayBoundsInParent(window()));
  if (location.x() <= area.x())
    return SNAP_LEFT_EDGE;
  if (location.x() >= area.right() - 1)
    return SNAP_RIGHT_EDGE;
  return SNAP_NONE;
}

bool WorkspaceWindowResizer::ShouldAllowMouseWarp() const {
  return (details_.window_component == HTCAPTION) &&
      (window()->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_NONE) &&
      (window()->type() == aura::client::WINDOW_TYPE_NORMAL);
}

void WorkspaceWindowResizer::RecreateWindowLayers() {
  DCHECK(!layer_);
  layer_ = wm::RecreateWindowLayers(window(), true);
  layer_->set_delegate(window()->layer()->delegate());
  // Place the layer at (0, 0) of the PhantomWindowController's window.
  gfx::Rect layer_bounds = layer_->bounds();
  layer_bounds.set_origin(gfx::Point(0, 0));
  layer_->SetBounds(layer_bounds);
  layer_->SetVisible(false);
  // Detach it from the current container.
  layer_->parent()->Remove(layer_);
}

}  // namespace internal
}  // namespace ash
