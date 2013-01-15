// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_DRAG_CONTROLLER_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_DRAG_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "ui/gfx/vector2d.h"

class Panel;
class PanelCollection;
class PanelManager;
namespace gfx {
class Point;
class Rect;
}

// Controls all the drags initiated for all panels, including detaching,
// docking, stacking, snapping and intra-collection dragging.
class PanelDragController {
 public:
  explicit PanelDragController(PanelManager* panel_manager);
  ~PanelDragController();

  // Drags the given panel.
  // |mouse_location| is in screen coordinate system.
  void StartDragging(Panel* panel, const gfx::Point& mouse_location);
  void Drag(const gfx::Point& mouse_location);
  void EndDragging(bool cancelled);

  // Asynchronous confirmation of panel having been closed.
  void OnPanelClosed(Panel* panel);

  bool is_dragging() const { return dragging_panel_ != NULL; }
  Panel* dragging_panel() const { return dragging_panel_; }

  // For testing.
  static int GetDetachDockedPanelThresholdForTesting();
  static int GetDockDetachedPanelThresholdForTesting();
  static int GetGluePanelDistanceThresholdForTesting();
  static int GetGluePanelOverlapThresholdForTesting();

 private:
  enum GlueAction {
    STACK,
    SNAP
  };

  enum GlueEdge {
    TOP_EDGE,
    BOTTOM_EDGE,
    LEFT_EDGE,
    RIGHT_EDGE
  };

  gfx::Point GetPanelPositionForMouseLocation(
      const gfx::Point& mouse_location) const;

  // |target_position| is in screen coordinate systems. It contains the proposed
  //  panel origin to move to. Returns true if the request has been performed.
  void TryDetach(const gfx::Point& target_position);
  void TryDock(const gfx::Point& target_position);
  void TryStack(const gfx::Point& target_position);
  bool TryUnstackFromTop(const gfx::Point& target_position);
  bool TryUnstackFromBottom(const gfx::Point& target_position);
  void TrySnap(gfx::Point* target_position);

  // Finds the panel that the dragging panel with |potential_position| could
  // snap to or stack with. If such panel is found, |target_bounds| contains the
  // new bounds for the dragging panel and |target_edge| contains the matched
  // edge.
  Panel* FindPanelToGlue(const gfx::Point& potential_position,
                         GlueAction action,
                         gfx::Rect* target_bounds,
                         GlueEdge* target_edge) const;

  // Moves the |panel| (and all panels below if it is in a stack) to a different
  // collection.
  void MovePanelAndBelowToCollection(
      Panel* panel,
      PanelCollection* target_collection,
      PanelCollection::PositioningMask positioning_mask) const;

  PanelManager* panel_manager_;  // Weak, owns us.
  bool panel_stacking_enabled_;

  // Panel currently being dragged.
  Panel* dragging_panel_;

  // The original panel collection when the drag is started.
  PanelCollection* dragging_panel_original_collection_;

  // The offset from mouse location to the panel position when the drag
  // starts.
  gfx::Vector2d offset_from_mouse_location_on_drag_start_;

  DISALLOW_COPY_AND_ASSIGN(PanelDragController);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_DRAG_CONTROLLER_H_
