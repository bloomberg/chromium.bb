// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_STRIP_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_STRIP_H_
#pragma once

#include "ui/gfx/rect.h"

class Panel;

// Common base class for a collection of panels. Subclasses manage
// various layouts for displaying panels in the collection.
class PanelStrip {
 public:
  // Types of layout for the panel collections.
  enum Type {
    DETACHED,  // free-floating panels
    DOCKED,    // panels are 'docked' along the window's edge
    IN_OVERFLOW,  // panels that cannot fit in the 'docked' panels area
  };

  Type type() const { return type_; }

  // Sets the bounds of the panel strip.
  // |display_area| is in screen coordinates.
  virtual void SetDisplayArea(const gfx::Rect& display_area) = 0;

  // Updates the positioning of all panels in the collection, usually as
  // a result of removing or resizing a panel in collection.
  virtual void RefreshLayout() = 0;

  // Adds |panel| to the collection of panels.
  virtual void AddPanel(Panel* panel) = 0;

  // Removes |panel| from the collection of panels. Invoked asynchronously
  // after a panel has been closed.
  // Returns |false| if the panel is not in the strip.
  virtual bool RemovePanel(Panel* panel) = 0;

  // Closes all panels in the collection. Panels will be removed after closing.
  virtual void CloseAll() = 0;

  // Resizes the |panel| to the |preferred_window_size| and updates the layout
  // of other panels in the collection accordingly.
  // |preferred_window_size| is the outer dimensions of the window, not
  // the content area, and is in screen coordinates.
  // The preferred size may be adjusted to fit layout constraints.
  virtual void ResizePanelWindow(Panel* panel,
                                 const gfx::Size& preferred_window_size) = 0;

  // Invoked when the draw attention state of the panel has changed.
  // Subclass should update the display of the panel to match the new
  // draw attention state.
  virtual void OnPanelAttentionStateChanged(Panel* panel) = 0;

  // Updates the display to show |panel| as active.
  virtual void ActivatePanel(Panel* panel) = 0;

  // Updates the display to show |panel| as  minimized/restored.
  virtual void MinimizePanel(Panel* panel) = 0;
  virtual void RestorePanel(Panel* panel) = 0;

  // Returns true if all panels in the strip
  // should be treated as minimized (overflow strip)
  virtual bool IsPanelMinimized(Panel* panel) const = 0;

  // Returns true if |panel| can be shown as active.
  virtual bool CanShowPanelAsActive(const Panel* panel) const = 0;

  // Returns true if |panel| is draggable.
  virtual bool CanDragPanel(const Panel* panel) const = 0;

  // Drags |panel| in the bounds of this strip.
  virtual void StartDraggingPanel(Panel* panel) = 0;
  // |delta_x| and |delta_y| denotes how much the mouse has been moved since
  // last time when DragPanel or StartDraggingPanel is called.
  virtual void DragPanel(Panel* panel, int delta_x, int delta_y) = 0;
  virtual void EndDraggingPanel(Panel* panel, bool cancelled) = 0;

 protected:
  explicit PanelStrip(Type type);
  virtual ~PanelStrip();

  const Type type_;  // Type of this panel strip.
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_STRIP_H_
