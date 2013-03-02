// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_COLLECTION_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_COLLECTION_H_

#include "chrome/browser/ui/panels/panel_constants.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

class Panel;

// Common base class for a collection of panels. Subclasses manage
// various layouts for displaying panels in the collection.
class PanelCollection {
 public:
  // Types of layout for the panel collections.
  enum Type {
    DETACHED,  // free-floating panels
    DOCKED,    // panels are 'docked' along the window's edge
    STACKED,   // panels are stacked together
  };

  // Masks that control how the panel is added and positioned.
  enum PositioningMask {
    // The panel is added and placed at default position that is decided by the
    // collection.
    DEFAULT_POSITION = 0x0,
    // The panel is being added based on its current known position.
    KNOWN_POSITION = 0x1,
    // The panel is added and placed at top position (currently only used by
    // stacked collection)
    TOP_POSITION = 0x2,
    // Do not update panel bounds. Only valid with DEFAULT_POSIITON.
    DO_NOT_UPDATE_BOUNDS = 0x4,
    // Wait for a brief delay before refreshing layout of the collection after
    // adding panel to the collection. If not set, the collection will refresh
    // its layout immediately.
    DELAY_LAYOUT_REFRESH = 0x8,
    // Do not refresh layout. Used by stacking.
    NO_LAYOUT_REFRESH = 0x10,
    // Collapse other inactive stacked panels such the tha new panel can fit
    // within the working area. Used by stacking.
    COLLAPSE_TO_FIT = 0x20
  };

  enum RemovalReason {
    PANEL_CLOSED,
    PANEL_CHANGED_COLLECTION
  };

  Type type() const { return type_; }

  // Called when the display area is changed.
  virtual void OnDisplayChanged() = 0;

  // Updates the positioning of all panels in the collection, usually as
  // a result of removing or resizing a panel in collection.
  virtual void RefreshLayout() = 0;

  // Adds |panel| to the collection of panels.
  // |positioning_mask| indicates how |panel| should be added and positioned.
  virtual void AddPanel(Panel* panel, PositioningMask positioning_mask) = 0;

  // Removes |panel| from the collection of panels. Invoked asynchronously
  // after a panel has been closed.
  // |reason| denotes why the panel is removed from the collection.
  virtual void RemovePanel(Panel* panel, RemovalReason reason) = 0;

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
  // if |panel| is resizable in this collection.
  virtual panel::Resizability GetPanelResizability(
      const Panel* panel) const = 0;

  // Change panel's bounds and take care of all possible side effects
  // in ths collection as a result of the panel being resized by the user.
  // TODO (AndreiB) Add a parameter telling what how to approach animation
  // (no animation, continue existing, or start new).
  virtual void OnPanelResizedByMouse(Panel* panel,
                                     const gfx::Rect& new_bounds) = 0;

  // Invoked when the draw attention state of the panel has changed.
  // Subclass should update the display of the panel to match the new
  // draw attention state.
  virtual void OnPanelAttentionStateChanged(Panel* panel) = 0;

  // Invoked when the titlebar of a |panel| in the collection has been clicked.
  // Click behavior may be modified as indicated by |modifier|.
  virtual void OnPanelTitlebarClicked(Panel* panel,
                                      panel::ClickModifier modifier) = 0;

  // Called when a panel's expansion state changes.
  virtual void OnPanelExpansionStateChanged(Panel* panel) = 0;

  // Called when a panel in the collection becomes active or inactive.
  virtual void OnPanelActiveStateChanged(Panel* panel) = 0;

  // Updates the display to show |panel| as active.
  virtual void ActivatePanel(Panel* panel) = 0;

  // Updates the display to show |panel| as minimized/restored.
  virtual void MinimizePanel(Panel* panel) = 0;
  virtual void RestorePanel(Panel* panel) = 0;

  // Called when a panel's minimize/restore button is clicked.
  // The behavior might be modified as indicated by |modifier|.
  virtual void OnMinimizeButtonClicked(Panel* panel,
                                       panel::ClickModifier modifier) = 0;
  virtual void OnRestoreButtonClicked(Panel* panel,
                                      panel::ClickModifier modifier) = 0;

  // Returns true if minimize or restore button can be shown on the panel's
  // titlebar.
  virtual bool CanShowMinimizeButton(const Panel* panel) const = 0;
  virtual bool CanShowRestoreButton(const Panel* panel) const = 0;

  virtual bool IsPanelMinimized(const Panel* panel) const = 0;

  // Saves/restores/discards the placement information of |panel|. This is
  // useful in bringing back the dragging panel to its original positioning
  // when the drag is cancelled. After the placement information is saved,
  // the caller should only call one of RestorePanelToSavedPlacement or
  // DiscardSavedPanelPlacement.
  virtual void SavePanelPlacement(Panel* panel) = 0;
  virtual void RestorePanelToSavedPlacement() = 0;
  virtual void DiscardSavedPanelPlacement() = 0;

  // When a panel is added to this collection, some modifications to its visual
  // style or underlying implementation may be in order. Each collection decides
  // what properties should be applied to a newly-added panel.
  virtual void UpdatePanelOnCollectionChange(Panel* panel) = 0;

 protected:
  explicit PanelCollection(Type type);
  virtual ~PanelCollection();

  const Type type_;  // Type of this panel collection.
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_COLLECTION_H_
