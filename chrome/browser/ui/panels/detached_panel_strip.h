// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_DETACHED_PANEL_STRIP_H_
#define CHROME_BROWSER_UI_PANELS_DETACHED_PANEL_STRIP_H_
#pragma once

#include <set>
#include "base/basictypes.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

class Browser;
class PanelManager;

// This class manages a group of free-floating panels.
class DetachedPanelStrip : public PanelStrip {
 public:
  typedef std::set<Panel*> Panels;

  explicit DetachedPanelStrip(PanelManager* panel_manager);
  virtual ~DetachedPanelStrip();

  // PanelStrip OVERRIDES:
  virtual gfx::Rect GetDisplayArea() const OVERRIDE;
  virtual void SetDisplayArea(const gfx::Rect& display_area) OVERRIDE;
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
  virtual void StartDraggingPanelWithinStrip(Panel* panel) OVERRIDE;
  virtual void DragPanelWithinStrip(Panel* panel,
                                    int delta_x,
                                    int delta_y) OVERRIDE;
  virtual void EndDraggingPanelWithinStrip(Panel* panel,
                                           bool aborted) OVERRIDE;
  virtual void ClearDraggingStateWhenPanelClosed() OVERRIDE;
  virtual void UpdatePanelOnStripChange(Panel* panel) OVERRIDE;
  virtual void OnPanelActiveStateChanged(Panel* panel) OVERRIDE;

  bool HasPanel(Panel* panel) const;

  int num_panels() const { return panels_.size(); }
  const Panels& panels() const { return panels_; }

 private:
  struct PanelPlacement {
    Panel* panel;
    gfx::Point position;

    PanelPlacement() : panel(NULL) { }
  };

  PanelManager* panel_manager_;  // Weak, owns us.

  // All panels in the panel strip must fit within this area.
  gfx::Rect display_area_;

  // Collection of all panels.
  Panels panels_;

  // Used to save the placement information for a panel.
  PanelPlacement saved_panel_placement_;

  DISALLOW_COPY_AND_ASSIGN(DetachedPanelStrip);
};

#endif  // CHROME_BROWSER_UI_PANELS_DETACHED_PANEL_STRIP_H_
