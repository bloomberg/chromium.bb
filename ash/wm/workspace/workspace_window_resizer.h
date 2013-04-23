// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WINDOW_RESIZER_H_
#define ASH_WM_WORKSPACE_WINDOW_RESIZER_H_

#include <vector>

#include "ash/wm/window_resizer.h"
#include "ash/wm/workspace/magnetism_matcher.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_tracker.h"

namespace ash {
namespace internal {

class PhantomWindowController;
class SnapSizer;
class WindowSize;

// WindowResizer implementation for workspaces. This enforces that windows are
// not allowed to vertically move or resize outside of the work area. As windows
// are moved outside the work area they are shrunk. We remember the height of
// the window before it was moved so that if the window is again moved up we
// attempt to restore the old height.
class ASH_EXPORT WorkspaceWindowResizer : public WindowResizer {
 public:
  // When dragging an attached window this is the min size we'll make sure is
  // visible. In the vertical direction we take the max of this and that from
  // the delegate.
  static const int kMinOnscreenSize;

  // Min height we'll force on screen when dragging the caption.
  // TODO: this should come from a property on the window.
  static const int kMinOnscreenHeight;

  // Snap region when dragging close to the edges. That is, as the window gets
  // this close to an edge of the screen it snaps to the edge.
  static const int kScreenEdgeInset;

  virtual ~WorkspaceWindowResizer();

  static WorkspaceWindowResizer* Create(
      aura::Window* window,
      const gfx::Point& location_in_parent,
      int window_component,
      const std::vector<aura::Window*>& attached_windows);

  // Overridden from WindowResizer:
  virtual void Drag(const gfx::Point& location_in_parent,
                    int event_flags) OVERRIDE;
  virtual void CompleteDrag(int event_flags) OVERRIDE;
  virtual void RevertDrag() OVERRIDE;
  virtual aura::Window* GetTarget() OVERRIDE;

  const gfx::Point& GetInitialLocationInParentForTest() const {
    return details_.initial_location_in_parent;
  }

 private:
  WorkspaceWindowResizer(const Details& details,
                         const std::vector<aura::Window*>& attached_windows);

 private:
  FRIEND_TEST_ALL_PREFIXES(WorkspaceWindowResizerTest, CancelSnapPhantom);
  FRIEND_TEST_ALL_PREFIXES(WorkspaceWindowResizerTest, PhantomSnapMaxSize);

  // Type of snapping.
  enum SnapType {
    // Snap to the left/right edge of the screen.
    SNAP_LEFT_EDGE,
    SNAP_RIGHT_EDGE,

    // No snap position.
    SNAP_NONE
  };

  // Returns the final bounds to place the window at. This differs from
  // the current when snapping.
  gfx::Rect GetFinalBounds(const gfx::Rect& bounds) const;

  // Lays out the attached windows. |bounds| is the bounds of the main window.
  void LayoutAttachedWindows(gfx::Rect* bounds);

  // Calculates the new sizes of the attached windows, given that the main
  // window has been resized (along the primary axis) by |delta|.
  // |available_size| is the maximum length of the space that the attached
  // windows are allowed to occupy (ie: the distance between the right/bottom
  // edge of the primary window and the right/bottom of the desktop area).
  // Populates |sizes| with the desired sizes of the attached windows, and
  // returns the number of pixels that couldn't be allocated to the attached
  // windows (due to min/max size constraints).
  // Note the return value can be positive or negative, a negative value
  // indicating that that many pixels couldn't be removed from the attached
  // windows.
  int CalculateAttachedSizes(
      int delta,
      int available_size,
      std::vector<int>* sizes) const;

  // Divides |amount| evenly between |sizes|. If |amount| is negative it
  // indicates how many pixels |sizes| should be shrunk by.
  // Returns how many pixels failed to be allocated/removed from |sizes|.
  int GrowFairly(int amount, std::vector<WindowSize>& sizes) const;

  // Calculate the ratio of pixels that each WindowSize in |sizes| should
  // receive when growing or shrinking.
  void CalculateGrowthRatios(const std::vector<WindowSize*>& sizes,
                             std::vector<float>* out_ratios) const;

  // Adds a WindowSize to |sizes| for each attached window.
  void CreateBucketsForAttached(std::vector<WindowSize>* sizes) const;

  // If possible snaps the window to a neary window. Updates |bounds| if there
  // was a close enough window.
  void MagneticallySnapToOtherWindows(gfx::Rect* bounds);

  // If possible snaps the resize to a neary window. Updates |bounds| if there
  // was a close enough window.
  void MagneticallySnapResizeToOtherWindows(gfx::Rect* bounds);

  // Finds the neareset window to magentically snap to. Updates
  // |magnetism_window_| and |magnetism_edge_| appropriately. |edges| is a
  // bitmask of the MagnetismEdges to match again. Returns true if a match is
  // found.
  bool UpdateMagnetismWindow(const gfx::Rect& bounds, uint32 edges);

  // Adjusts the bounds of the window: magnetically snapping, ensuring the
  // window has enough on screen... |snap_size| is the distance from an edge of
  // the work area before the window is snapped. A value of 0 results in no
  // snapping.
  void AdjustBoundsForMainWindow(int snap_size, gfx::Rect* bounds);

  // Stick the window bounds to the work area during a move.
  void StickToWorkAreaOnMove(const gfx::Rect& work_area,
                             int sticky_size,
                             gfx::Rect* bounds) const;

  // Stick the window bounds to the work area during a resize.
  void StickToWorkAreaOnResize(const gfx::Rect& work_area,
                               int sticky_size,
                               gfx::Rect* bounds) const;

  // Returns a coordinate along the primary axis. Used to share code for
  // left/right multi window resize and top/bottom resize.
  int PrimaryAxisSize(const gfx::Size& size) const;
  int PrimaryAxisCoordinate(int x, int y) const;

  // Updates the bounds of the phantom window for window snapping.
  void UpdateSnapPhantomWindow(const gfx::Point& location,
                               const gfx::Rect& bounds);

  // Restacks the windows z-order position so that one of the windows is at the
  // top of the z-order, and the rest directly underneath it.
  void RestackWindows();

  // Returns the SnapType for the specified point. SNAP_NONE is used if no
  // snapping should be used.
  SnapType GetSnapType(const gfx::Point& location) const;

  aura::Window* window() const { return details_.window; }

  const Details details_;

  const std::vector<aura::Window*> attached_windows_;

  // Set to true once Drag() is invoked and the bounds of the window change.
  bool did_move_or_resize_;

  // The initial size of each of the windows in |attached_windows_| along the
  // primary axis.
  std::vector<int> initial_size_;

  // Sum of the minimum sizes of the attached windows.
  int total_min_;

  // Sum of the sizes in |initial_size_|.
  int total_initial_size_;

  // Gives a previews of where the the window will end up. Only used if there
  // is a grid and the caption is being dragged.
  scoped_ptr<PhantomWindowController> snap_phantom_window_controller_;

  // Used to determine the target position of a snap.
  scoped_ptr<SnapSizer> snap_sizer_;

  // Last SnapType.
  SnapType snap_type_;

  // Number of mouse moves since the last bounds change. Only used for phantom
  // placement to track when the mouse is moved while pushed against the edge of
  // the screen.
  int num_mouse_moves_since_bounds_change_;

  // The mouse location passed to Drag().
  gfx::Point last_mouse_location_;

  // Window the drag has magnetically attached to.
  aura::Window* magnetism_window_;

  // Used to verify |magnetism_window_| is still valid.
  aura::WindowTracker window_tracker_;

  // If |magnetism_window_| is non-NULL this indicates how the two windows
  // should attach.
  MatchedEdge magnetism_edge_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceWindowResizer);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WINDOW_RESIZER_H_
