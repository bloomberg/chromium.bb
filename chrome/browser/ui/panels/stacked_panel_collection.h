// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_STACKED_PANEL_COLLECTION_H_
#define CHROME_BROWSER_UI_PANELS_STACKED_PANEL_COLLECTION_H_

#include <list>
#include <vector>
#include "base/basictypes.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "ui/gfx/rect.h"

class PanelManager;

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

  bool empty() const { return panels_.empty(); }
  int num_panels() const { return panels_.size(); }
  const Panels& panels() const { return panels_; }
  Panel* top_panel() const { return panels_.front(); }
  Panel* bottom_panel() const { return panels_.back(); }

 private:
  PanelManager* panel_manager_;  // Weak, owns us.

  Panels panels_;  // The top panel is in the front of the list.

  DISALLOW_COPY_AND_ASSIGN(StackedPanelCollection);
};

#endif  // CHROME_BROWSER_UI_PANELS_STACKED_PANEL_COLLECTION_H_
