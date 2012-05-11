// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_DRAG_CONTROLLER_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_DRAG_CONTROLLER_H_
#pragma once

#include <set>
#include "base/basictypes.h"
#include "ui/gfx/point.h"

class Panel;
class PanelManager;
class PanelStrip;
namespace gfx {
class Rect;
}

// Responsible for handling drags initiated for all panels, including both
// intra-strip and inter-strip drags.
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

  bool IsDragging() const { return dragging_panel_ != NULL; }

  Panel* dragging_panel() const { return dragging_panel_; }

#ifdef UNIT_TEST
  static int GetDetachDockedPanelThreshold() {
    return kDetachDockedPanelThreshold;
  }

  static int GetDockDetachedPanelThreshold() {
    return kDockDetachedPanelThreshold;
  }
#endif

 private:
  // Helper methods to figure out if the panel can be dragged to other strip.
  // Returns target strip/boolean flag, and |new_panel_bounds| if the panel
  // can enter other strip at |mouse_location|.
  PanelStrip* ComputeDragTargetStrip(
      const gfx::Point& mouse_location, gfx::Rect* new_panel_bounds) const;
  bool CanDragToDockedStrip(
      const gfx::Point& mouse_location, gfx::Rect* new_panel_bounds) const;
  bool CanDragToDetachedStrip(
      const gfx::Point& mouse_location, gfx::Rect* new_panel_bounds) const;

  // The potential panel position is computed based on the fact that the panel
  // should follow the mouse movement.
  gfx::Point GetPanelPositionForMouseLocation(
      const gfx::Point& mouse_location) const;

  PanelManager* panel_manager_;  // Weak, owns us.

  // Panel currently being dragged.
  Panel* dragging_panel_;

  // The original panel strip when the drag is started.
  PanelStrip* dragging_panel_original_strip_;

  // The mouse location, in screen coordinates, when StartDragging or Drag was
  // pveviously called.
  gfx::Point last_mouse_location_;

  // The offset from mouse location to the panel position when the drag
  // starts.
  gfx::Point offset_from_mouse_location_on_drag_start_;

  // The minimum distance that the docked panel gets dragged up in order to
  // make it free-floating.
  static const int kDetachDockedPanelThreshold;

  // Indicates how close the bottom of the detached panel is to the bottom of
  // the docked area such that the detached panel becomes docked.
  static const int kDockDetachedPanelThreshold;

  DISALLOW_COPY_AND_ASSIGN(PanelDragController);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_DRAG_CONTROLLER_H_
