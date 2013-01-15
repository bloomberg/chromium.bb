// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_STACKED_PANEL_COLLECTION_H_
#define CHROME_BROWSER_UI_PANELS_STACKED_PANEL_COLLECTION_H_

#include <list>
#include <vector>
#include "base/basictypes.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "ui/gfx/rect.h"

class NativePanelStack;
class PanelManager;
namespace gfx {
class Vector2d;
}

class StackedPanelCollection : public PanelCollection {
 public:
  typedef std::list<Panel*> Panels;

  explicit StackedPanelCollection(PanelManager* panel_manager);
  virtual ~StackedPanelCollection();

  // PanelCollection OVERRIDES:
  virtual void OnDisplayAreaChanged(const gfx::Rect& old_display_area) OVERRIDE;
  virtual void RefreshLayout() OVERRIDE;
  virtual void AddPanel(Panel* panel,
                        PositioningMask positioning_mask) OVERRIDE;
  virtual void RemovePanel(Panel* panel) OVERRIDE;
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
  virtual void MinimizeAll() OVERRIDE;
  virtual void RestoreAll() OVERRIDE;
  virtual bool CanMinimizePanel(const Panel* panel) const OVERRIDE;
  virtual bool IsPanelMinimized(const Panel* panel) const OVERRIDE;
  virtual void SavePanelPlacement(Panel* panel) OVERRIDE;
  virtual void RestorePanelToSavedPlacement() OVERRIDE;
  virtual void DiscardSavedPanelPlacement()  OVERRIDE;
  virtual void UpdatePanelOnCollectionChange(Panel* panel) OVERRIDE;
  virtual void OnPanelActiveStateChanged(Panel* panel) OVERRIDE;

  Panel* GetPanelAbove(Panel* panel) const;
  bool HasPanel(Panel* panel) const;

  void MoveAllDraggingPanelsInstantly(const gfx::Vector2d& delta_origin);

  NativePanelStack* native_stack() const { return native_stack_; }
  int num_panels() const { return panels_.size(); }
  const Panels& panels() const { return panels_; }
  Panel* top_panel() const { return panels_.empty() ? NULL : panels_.front(); }
  Panel* bottom_panel() const {
    return panels_.empty() ? NULL : panels_.back();
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

  PanelManager* panel_manager_;

  NativePanelStack* native_stack_;  // Weak, owns us.

  Panels panels_;  // The top panel is in the front of the list.

  // Used to save the placement information for a panel.
  PanelPlacement saved_panel_placement_;

  DISALLOW_COPY_AND_ASSIGN(StackedPanelCollection);
};

#endif  // CHROME_BROWSER_UI_PANELS_STACKED_PANEL_COLLECTION_H_
