// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_DETACHED_PANEL_COLLECTION_H_
#define CHROME_BROWSER_UI_PANELS_DETACHED_PANEL_COLLECTION_H_

#include <list>
#include "base/basictypes.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

class PanelManager;

// This class manages a group of free-floating panels.
class DetachedPanelCollection : public PanelCollection {
 public:
  typedef std::list<Panel*> Panels;
  typedef bool (*PanelsComparer)(Panel* panel1, Panel* panel2);

  explicit DetachedPanelCollection(PanelManager* panel_manager);
  ~DetachedPanelCollection() override;

  // PanelCollection OVERRIDES:
  void OnDisplayChanged() override;
  void RefreshLayout() override;
  void AddPanel(Panel* panel, PositioningMask positioning_mask) override;
  void RemovePanel(Panel* panel, RemovalReason reason) override;
  void CloseAll() override;
  void ResizePanelWindow(Panel* panel,
                         const gfx::Size& preferred_window_size) override;
  panel::Resizability GetPanelResizability(const Panel* panel) const override;
  void OnPanelResizedByMouse(Panel* panel,
                             const gfx::Rect& new_bounds) override;
  void OnPanelAttentionStateChanged(Panel* panel) override;
  void OnPanelTitlebarClicked(Panel* panel,
                              panel::ClickModifier modifier) override;
  void ActivatePanel(Panel* panel) override;
  void MinimizePanel(Panel* panel) override;
  void RestorePanel(Panel* panel) override;
  void OnMinimizeButtonClicked(Panel* panel,
                               panel::ClickModifier modifier) override;
  void OnRestoreButtonClicked(Panel* panel,
                              panel::ClickModifier modifier) override;
  bool CanShowMinimizeButton(const Panel* panel) const override;
  bool CanShowRestoreButton(const Panel* panel) const override;
  bool IsPanelMinimized(const Panel* panel) const override;
  bool UsesAlwaysOnTopPanels() const override;
  void SavePanelPlacement(Panel* panel) override;
  void RestorePanelToSavedPlacement() override;
  void DiscardSavedPanelPlacement() override;
  void UpdatePanelOnCollectionChange(Panel* panel) override;
  void OnPanelExpansionStateChanged(Panel* panel) override;
  void OnPanelActiveStateChanged(Panel* panel) override;
  gfx::Rect GetInitialPanelBounds(
      const gfx::Rect& requested_bounds) const override;

  bool HasPanel(Panel* panel) const;

  void SortPanels(PanelsComparer comparer);

  int num_panels() const { return panels_.size(); }
  const Panels& panels() const { return panels_; }

  // Returns default top-left to use for a detached panel whose position is
  // not specified during panel creation.
  gfx::Point GetDefaultPanelOrigin();

 private:
  // Offset the default panel top-left position by kPanelTilePixels. Wrap
  // around to initial position if position goes beyond display area.
  void ComputeNextDefaultPanelOrigin();

  struct PanelPlacement {
    Panel* panel;
    gfx::Point position;

    PanelPlacement() : panel(NULL) { }
  };

  PanelManager* panel_manager_;  // Weak, owns us.

  // Collection of all panels.
  Panels panels_;

  // Used to save the placement information for a panel.
  PanelPlacement saved_panel_placement_;

  // Default top-left position to use for next detached panel if position is
  // unspecified by panel creator.
  gfx::Point default_panel_origin_;

  DISALLOW_COPY_AND_ASSIGN(DetachedPanelCollection);
};

#endif  // CHROME_BROWSER_UI_PANELS_DETACHED_PANEL_COLLECTION_H_
