// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_STRIP_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_STRIP_H_
#pragma once

#include "chrome/browser/ui/panels/panel_constants.h"
#include "ui/gfx/point.h"
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
  };

  // Masks that control how the panel is added and positioned.
  enum PositioningMask {
    // The panel is added and placed at default position that is decided by the
    // strip.
    DEFAULT_POSITION = 0x0,
    // The panel is being added based on its current known position.
    KNOWN_POSITION = 0x1,
    // Do not update panel bounds. Only valid with DEFAULT_POSITION.
    DO_NOT_UPDATE_BOUNDS = 0x2
  };

  Type type() const { return type_; }

  // Gets/Sets the bounds of the panel strip.
  // |display_area| is in screen coordinates.
  virtual gfx::Rect GetDisplayArea() const = 0;
  virtual void SetDisplayArea(const gfx::Rect& display_area) = 0;

  // Updates the positioning of all panels in the collection, usually as
  // a result of removing or resizing a panel in collection.
  virtual void RefreshLayout() = 0;

  // Adds |panel| to the collection of panels.
  // |positioning_mask| indicates how |panel| should be added and positioned.
  virtual void AddPanel(Panel* panel, PositioningMask positioning_mask) = 0;

  // Removes |panel| from the collection of panels. Invoked asynchronously
  // after a panel has been closed.
  virtual void RemovePanel(Panel* panel) = 0;

  // Closes all panels in the collection. Panels will be removed after closing.
  virtual void CloseAll() = 0;

  // Resizes the |panel| to the |preferred_window_size| and updates the layout
  // of other panels in the collection accordingly.
  // |preferred_window_size| is the outer dimensions of the window, not
  // the content area, and is in screen coordinates.
  // The preferred size may be adjusted to fit layout constraints.
  virtual void ResizePanelWindow(Panel* panel,
                                 const gfx::Size& preferred_window_size) = 0;

  // Returns the sides from which |panel| can be resized by the user
  // if |panel| is resizable in this strip.
  virtual panel::Resizability GetPanelResizability(
      const Panel* panel) const = 0;

  // Change panel's bounds and take care of all possible side effects
  // in ths strip as a result of the panel being resized by the user.
  // TODO (AndreiB) Add a parameter telling what how to approach animation
  // (no animation, continue existing, or start new).
  virtual void OnPanelResizedByMouse(Panel* panel,
                                     const gfx::Rect& new_bounds) = 0;

  // Invoked when the draw attention state of the panel has changed.
  // Subclass should update the display of the panel to match the new
  // draw attention state.
  virtual void OnPanelAttentionStateChanged(Panel* panel) = 0;

  // Invoked when the titlebar of a |panel| in the strip has been clicked.
  // Click behavior may be modified as indicated by |modifier|.
  virtual void OnPanelTitlebarClicked(Panel* panel,
                                      panel::ClickModifier modifier) = 0;

  // Called when a panel in the strip becomes active or inactive.
  virtual void OnPanelActiveStateChanged(Panel* panel) = 0;

  // Updates the display to show |panel| as active.
  virtual void ActivatePanel(Panel* panel) = 0;

  // Updates the display to show |panel| as minimized/restored.
  virtual void MinimizePanel(Panel* panel) = 0;
  virtual void RestorePanel(Panel* panel) = 0;

  // Updates the display to show all panels in the strip as minimized/restored.
  virtual void MinimizeAll() = 0;
  virtual void RestoreAll() = 0;

  virtual bool CanMinimizePanel(const Panel* panel) const = 0;
  virtual bool IsPanelMinimized(const Panel* panel) const = 0;

  // Saves/restores/discards the placement information of |panel|. This is
  // useful in bringing back the dragging panel to its original positioning
  // when the drag is cancelled. After the placement information is saved,
  // the caller should only call one of RestorePanelToSavedPlacement or
  // DiscardSavedPanelPlacement.
  virtual void SavePanelPlacement(Panel* panel) = 0;
  virtual void RestorePanelToSavedPlacement() = 0;
  virtual void DiscardSavedPanelPlacement() = 0;

  // Starts dragging |panel| within this strip. The panel should already be
  // in this strip.
  virtual void StartDraggingPanelWithinStrip(Panel* panel) = 0;

  // Drags |panel| within this strip to |target_position|.
  virtual void DragPanelWithinStrip(Panel* panel,
                                    const gfx::Point& target_position) = 0;

  // Ends dragging |panel| within this strip. |aborted| means the drag within
  // this strip is aborted due to one of the following:
  // 1) the drag leaves this strip and enters other strip
  // 2) the drag gets cancelled
  // If |aborted| is true, |panel| will stay as it is; otherwise, it will be
  // moved to its finalized position.
  // The drag controller is responsible for restoring the panel back to its
  // original strip and position when the drag gets cancelled.
  virtual void EndDraggingPanelWithinStrip(Panel* panel, bool aborted) = 0;

  // Ends dragging and clears dragging state when the dragged panel has closed.
  virtual void ClearDraggingStateWhenPanelClosed() = 0;

  // When a panel is added to this strip, some modifications to its visual
  // style or underlying implementation may be in order. Each strip decides
  // what properties should be applied to a newly-added panel.
  virtual void UpdatePanelOnStripChange(Panel* panel) = 0;

 protected:
  explicit PanelStrip(Type type);
  virtual ~PanelStrip();

  const Type type_;  // Type of this panel strip.
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_STRIP_H_
