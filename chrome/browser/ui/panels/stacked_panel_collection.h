// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_STACKED_PANEL_COLLECTION_H_
#define CHROME_BROWSER_UI_PANELS_STACKED_PANEL_COLLECTION_H_

#include <list>
#include <vector>
#include "base/basictypes.h"
#include "chrome/browser/ui/panels/native_panel_stack_window.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "ui/gfx/rect.h"

class PanelManager;
namespace gfx {
class Vector2d;
}

class StackedPanelCollection : public PanelCollection,
                               public NativePanelStackWindowDelegate {
 public:
  typedef std::list<Panel*> Panels;

  explicit StackedPanelCollection(PanelManager* panel_manager);
  virtual ~StackedPanelCollection();

  // PanelCollection OVERRIDES:
  virtual void OnDisplayChanged() OVERRIDE;
  virtual void RefreshLayout() OVERRIDE;
  virtual void AddPanel(Panel* panel,
                        PositioningMask positioning_mask) OVERRIDE;
  virtual void RemovePanel(Panel* panel, RemovalReason reason) OVERRIDE;
  virtual void CloseAll() OVERRIDE;
  virtual void ResizePanelWindow(
      Panel* panel,
      const gfx::Size& preferred_window_size) OVERRIDE;
  virtual panel::Resizability GetPanelResizability(
      const Panel* panel) const OVERRIDE;
  virtual void OnPanelResizedByMouse(Panel* panel,
                                     const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnPanelAttentionStateChanged(Panel* panel) OVERRIDE;
  virtual void OnPanelTitlebarClicked(Panel* panel,
                                      panel::ClickModifier modifier) OVERRIDE;
  virtual void ActivatePanel(Panel* panel) OVERRIDE;
  virtual void MinimizePanel(Panel* panel) OVERRIDE;
  virtual void RestorePanel(Panel* panel) OVERRIDE;
  virtual void OnMinimizeButtonClicked(Panel* panel,
                                       panel::ClickModifier modifier) OVERRIDE;
  virtual void OnRestoreButtonClicked(Panel* panel,
                                      panel::ClickModifier modifier) OVERRIDE;
  virtual bool CanShowMinimizeButton(const Panel* panel) const OVERRIDE;
  virtual bool CanShowRestoreButton(const Panel* panel) const OVERRIDE;
  virtual bool IsPanelMinimized(const Panel* panel) const OVERRIDE;
  virtual bool UsesAlwaysOnTopPanels() const OVERRIDE;
  virtual void SavePanelPlacement(Panel* panel) OVERRIDE;
  virtual void RestorePanelToSavedPlacement() OVERRIDE;
  virtual void DiscardSavedPanelPlacement()  OVERRIDE;
  virtual void UpdatePanelOnCollectionChange(Panel* panel) OVERRIDE;
  virtual void OnPanelExpansionStateChanged(Panel* panel) OVERRIDE;
  virtual void OnPanelActiveStateChanged(Panel* panel) OVERRIDE;
  virtual gfx::Rect GetInitialPanelBounds(
      const gfx::Rect& requested_bounds) const OVERRIDE;

  Panel* GetPanelAbove(Panel* panel) const;
  Panel* GetPanelBelow(Panel* panel) const;
  bool HasPanel(Panel* panel) const;

  void MoveAllDraggingPanelsInstantly(const gfx::Vector2d& delta_origin);

  bool IsMinimized() const;
  bool IsAnimatingPanelBounds(Panel* panel) const;

  // Returns the maximum available space from the bottom of the stack. The
  // maximum available space is defined as the distance between the bottom
  // of the stack and the bottom of the working area, assuming that all inactive
  // panels are collapsed.
  int GetMaximiumAvailableBottomSpace() const;

  int num_panels() const { return panels_.size(); }
  const Panels& panels() const { return panels_; }
  Panel* top_panel() const { return panels_.empty() ? NULL : panels_.front(); }
  Panel* bottom_panel() const {
    return panels_.empty() ? NULL : panels_.back();
  }
  Panel* most_recently_active_panel() const {
    return most_recently_active_panels_.empty() ?
        NULL : most_recently_active_panels_.front();
  }

 private:
  struct PanelPlacement {
    Panel* panel;
    gfx::Point position;
    // Used to remember the top panel, if different from |panel|, for use when
    // restoring it. When there're only 2 panels in the stack and the bottom
    // panel is being dragged out of the stack, both panels will be moved to
    // the detached collection. We need to track the top panel in order to
    // put it back to the same stack of the dragging panel.
    Panel* top_panel;

    PanelPlacement() : panel(NULL), top_panel(NULL) { }
  };

  // Overridden from PanelBoundsBatchUpdateObserver:
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual gfx::Image GetIcon() const OVERRIDE;
  virtual void PanelBoundsBatchUpdateCompleted() OVERRIDE;

  // Returns the enclosing bounds that include all panels in the stack.
  gfx::Rect GetEnclosingBounds() const;

  // Returns the work area where the stack resides. If the stack spans across
  // multiple displays, return the work area of the display that most closely
  // intersects the stack.
  gfx::Rect GetWorkArea() const;

  // Refresh all panel layouts, with top panel poisitoned at |start_position|.
  // All panels should have same width as |common_width|.
  void RefreshLayoutWithTopPanelStartingAt(const gfx::Point& start_position,
                                           int common_width);

  // Tries to collapse panels in the least recently active order in order to get
  // enough bottom space for |needed_space|. Returns the current available space
  // so far if all panels that could be collapsed have been collapsed.
  int MinimizePanelsForSpace(int needed_space);

  // Returns the current available space above the top of the stack. The current
  // available space is defined as the distance between the top of the working
  // area and the top of the stack.
  int GetCurrentAvailableTopSpace() const;

  // Returns the current available space below the bottom of the stack. The
  // current available space is defined as the distance between the bottom
  // of the stack and the bottom of the working area.
  int GetCurrentAvailableBottomSpace() const;

  // Minimizes or restores all panels in the collection.
  void MinimizeAll();
  void RestoreAll(Panel* panel_clicked);

  void UpdatePanelCornerStyle(Panel* panel);

  NativePanelStackWindow* GetStackWindowForPanel(Panel* panel) const;

  PanelManager* panel_manager_;

  // Both stack window pointers are weak pointers and self owned. Once a stack
  // window is created, it will only be destructed by calling Close when it is
  // not longer needed as in RemovePanel and CloseAll.

  // The main background window that encloses all panels in the stack when
  // stacking is not occuring, or existing panels otherwise.
  // This window provides:
  // 1) the shadow around the the outer area of all panels.
  // 2) the consolidated taskbar icon and the consolidated preview
  //    (Windows only)
  NativePanelStackWindow* primary_stack_window_;

  // The additional background window that encloses those panels that are
  // being added to the stack when the stacking is occuring. Since those panels
  // have not been fully aligned with existing panels in the stack before the
  // stacking ends, we put those panels in a separate background window that
  // only provides the shadow around the outer area of those panels.
  NativePanelStackWindow* secondary_stack_window_;

  Panels panels_;  // The top panel is in the front of the list.

  // Keeps track of the panels in their active order. The most recently active
  // panel is in the front of the list.
  Panels most_recently_active_panels_;

  // Used to save the placement information for a panel.
  PanelPlacement saved_panel_placement_;

  bool minimizing_all_;  // True while minimizing all panels.

  DISALLOW_COPY_AND_ASSIGN(StackedPanelCollection);
};

#endif  // CHROME_BROWSER_UI_PANELS_STACKED_PANEL_COLLECTION_H_
