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
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/cursor_manager.h"
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

// Returns the origin for |src| when magnetically attaching to |attach_to|
// along the edge |edge|.
gfx::Point OriginForMagneticAttach(const gfx::Rect& src,
                                   const gfx::Rect& attach_to,
                                   MagnetismEdge edge) {
  switch (edge) {
    case MAGNETISM_EDGE_TOP:
      return gfx::Point(src.x(), attach_to.bottom());
    case MAGNETISM_EDGE_LEFT:
      return gfx::Point(attach_to.right(), src.y());
    case MAGNETISM_EDGE_BOTTOM:
      return gfx::Point(src.x(), attach_to.y() - src.height());
    case MAGNETISM_EDGE_RIGHT:
      return gfx::Point(attach_to.x() - src.width(), src.y());
  }
  NOTREACHED();
  return gfx::Point();
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

void WorkspaceWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  std::pair<aura::RootWindow*, gfx::Point> actual_location =
      wm::GetRootWindowRelativeToWindow(window()->parent(), location);
  aura::RootWindow* current_root = actual_location.first;
  gfx::Point location_in_parent = actual_location.second;
  aura::Window::ConvertPointToTarget(current_root,
                                     window()->parent(),
                                     &location_in_parent);
  last_mouse_location_ = location_in_parent;

  // Do not use |location| below this point, use |location_in_parent| instead.
  // When the pointer is on |window()->GetRootWindow()|, |location| and
  // |location_in_parent| have the same value and both of them are in
  // |window()->parent()|'s coordinates, but once the pointer enters the
  // other root window, you will see an unexpected value on the former. See
  // comments in wm::GetRootWindowRelativeToWindow() for details.

  int grid_size = event_flags & ui::EF_CONTROL_DOWN ? 0 : kScreenEdgeInset;
  gfx::Rect bounds =  // in |window()->parent()|'s coordinates.
      CalculateBoundsForDrag(details_, location_in_parent);

  if (wm::IsWindowNormal(window()))
    AdjustBoundsForMainWindow(&bounds, grid_size);

  if (bounds != window()->bounds()) {
    if (!did_move_or_resize_) {
      if (!details_.restore_bounds.IsEmpty())
        ClearRestoreBounds(window());
      RestackWindows();
    }
    did_move_or_resize_ = true;
  }

  const bool in_original_root = (window()->GetRootWindow() == current_root);
  // Hide a phantom window for snapping if the cursor is in another root window.
  if (in_original_root)
    UpdateSnapPhantomWindow(location_in_parent, bounds);
  else
    snap_phantom_window_controller_.reset();

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
  window()->layer()->SetOpacity(details_.initial_opacity);
  drag_phantom_window_controller_.reset();
  snap_phantom_window_controller_.reset();
  if (!did_move_or_resize_ || details_.window_component != HTCAPTION)
    return;

  if (snap_type_ == SNAP_LEFT_EDGE || snap_type_ == SNAP_RIGHT_EDGE) {
    if (!GetRestoreBoundsInScreen(window()))
      SetRestoreBoundsInParent(window(), details_.restore_bounds.IsEmpty() ?
                                         details_.initial_bounds :
                                         details_.restore_bounds);
    window()->SetBounds(snap_sizer_->target_bounds());
    return;
  }

  gfx::Rect bounds(GetFinalBounds(window()->bounds()));

  // Check if the destination is another display.
  gfx::Point last_mouse_location_in_screen = last_mouse_location_;
  wm::ConvertPointToScreen(window()->parent(), &last_mouse_location_in_screen);
  const gfx::Display dst_display =
      gfx::Screen::GetDisplayNearestPoint(last_mouse_location_in_screen);

  if (dst_display.id() !=
      gfx::Screen::GetDisplayNearestWindow(window()->GetRootWindow()).id()) {
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

  window()->SetBounds(details_.initial_bounds);
  if (!details_.restore_bounds.IsEmpty())
    SetRestoreBoundsInScreen(details_.window, details_.restore_bounds);

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
      magnetism_window_(NULL),
      magnetism_edge_(MAGNETISM_EDGE_TOP) {
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
      PrimaryAxisSize(details_.initial_bounds.size()),
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
    // If we snapped to a window then check it first. That way we don't bounce
    // around when close to multiple edges.
  if (magnetism_window_) {
    if (window_tracker_.Contains(magnetism_window_) &&
        MagnetismMatcher::ShouldAttachOnEdge(
            *bounds, magnetism_window_->bounds(), magnetism_edge_)) {
      bounds->set_origin(
          OriginForMagneticAttach(*bounds, magnetism_window_->bounds(),
                                  magnetism_edge_));
      return;
    }
    window_tracker_.Remove(magnetism_window_);
    magnetism_window_ = NULL;
  }

  MagnetismMatcher matcher(*bounds);
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
      bounds->set_origin(
          OriginForMagneticAttach(*bounds, magnetism_window_->bounds(),
                                  magnetism_edge_));
      return;
    }
  }
}

void WorkspaceWindowResizer::AdjustBoundsForMainWindow(
    gfx::Rect* bounds,
    int grid_size) {
  // Always keep kMinOnscreenHeight on the bottom except when an extended
  // display is available and a window is being dragged.
  gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(window()));
  int max_y = work_area.bottom() - kMinOnscreenHeight;
  if ((details_.window_component != HTCAPTION || !HasSecondaryRootWindow()) &&
      bounds->y() > max_y) {
    bounds->set_y(max_y);
  }

  // Don't allow dragging above the top of the display except when an extended
  // display is available and a window is being dragged.
  if ((details_.window_component != HTCAPTION || !HasSecondaryRootWindow()) &&
      bounds->y() <= work_area.y()) {
    bounds->set_y(work_area.y());
  }

  if (grid_size > 0 && details_.window_component == HTCAPTION) {
    SnapToWorkAreaEdges(work_area, bounds, grid_size);

    MagneticallySnapToOtherWindows(bounds);
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
    gfx::Rect* bounds,
    int grid_size) const {
  int left_edge = work_area.x();
  int right_edge = work_area.right();
  int top_edge = work_area.y();
  int bottom_edge = work_area.bottom();
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
      ScreenAsh::GetDisplayWorkAreaBoundsInParent(window()));
  return (attached_windows_.empty() &&
          window()->bounds().bottom() == work_area.bottom()) ||
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
  const gfx::Rect bounds_in_another_root =
      root_bounds_in_screen.Intersect(bounds_in_screen);

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
          gfx::Screen::GetDisplayMatching(another_root->GetBoundsInScreen()));
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
    snap_sizer_.reset(new SnapSizer(window(), location, edge));
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
