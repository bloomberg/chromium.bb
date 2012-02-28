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

// Responsible for handling drags initiated for all panels, including both
// intra-strip and inter-strip drags.
class PanelDragController {
 public:
  PanelDragController();
  ~PanelDragController();

  void StartDragging(Panel* panel);
  void Drag(int delta_x, int delta_y);
  void EndDragging(bool cancelled);

  // Asynchronous confirmation of panel having been closed.
  void OnPanelClosed(Panel* panel);

  bool IsDragging() const { return dragging_panel_ != NULL; }

  Panel* dragging_panel() const { return dragging_panel_; }
  gfx::Point dragging_panel_original_position() const {
    return dragging_panel_original_position_;
  }

 private:
  // Panel currently being dragged.
  Panel* dragging_panel_;

  // Original position, in screen coordinate system, of the panel being dragged.
  // This is used to get back to the original position when we cancel the
  // dragging.
  gfx::Point dragging_panel_original_position_;

  DISALLOW_COPY_AND_ASSIGN(PanelDragController);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_DRAG_CONTROLLER_H_
