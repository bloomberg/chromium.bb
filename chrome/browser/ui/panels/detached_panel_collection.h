// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_DETACHED_PANEL_COLLECTION_H_
#define CHROME_BROWSER_UI_PANELS_DETACHED_PANEL_COLLECTION_H_

#include <set>
#include "base/basictypes.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

class PanelManager;

// This class manages a group of free-floating panels.
class DetachedPanelCollection : public PanelCollection {
 public:
  typedef std::set<Panel*> Panels;

  explicit DetachedPanelCollection(PanelManager* panel_manager);
  virtual ~DetachedPanelCollection();

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
  virtual void OnMinimizeButtonClicked(Panel* panel,
                                       panel::ClickModifier modifier) OVERRIDE;
  virtual void OnRestoreButtonClicked(Panel* panel,
                                      panel::ClickModifier modifier) OVERRIDE;
  virtual bool CanShowMinimizeButton(const Panel* panel) const OVERRIDE;
  virtual bool CanShowRestoreButton(const Panel* panel) const OVERRIDE;
  virtual bool IsPanelMinimized(const Panel* panel) const OVERRIDE;
  virtual void SavePanelPlacement(Panel* panel) OVERRIDE;
  virtual void RestorePanelToSavedPlacement() OVERRIDE;
  virtual void DiscardSavedPanelPlacement() OVERRIDE;
  virtual void UpdatePanelOnCollectionChange(Panel* panel) OVERRIDE;
  virtual void OnPanelExpansionStateChanged(Panel* panel) OVERRIDE;
  virtual void OnPanelActiveStateChanged(Panel* panel) OVERRIDE;

  bool HasPanel(Panel* panel) const;

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
