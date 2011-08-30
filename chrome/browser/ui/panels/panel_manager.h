// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_MANAGER_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_MANAGER_H_
#pragma once

#include <vector>
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/ui/panels/auto_hiding_desktop_bar.h"
#include "chrome/browser/ui/panels/panel.h"
#include "ui/gfx/rect.h"

class Browser;
class Panel;

// This class manages a set of panels.
// Note that the ref count is needed by using PostTask in the implementation.
class PanelManager : public AutoHidingDesktopBar::Observer,
                     public base::RefCounted<PanelManager> {
 public:
  typedef std::vector<Panel*> Panels;

  // Returns a single instance.
  static PanelManager* GetInstance();

  virtual ~PanelManager();

  // Called when the display is changed, i.e. work area is updated.
  void OnDisplayChanged();

  // Creates a panel and returns it. The panel might be queued for display
  // later.
  Panel* CreatePanel(Browser* browser);

  void Remove(Panel* panel);
  void RemoveAll();

  // Drags the given panel.
  void StartDragging(Panel* panel);
  void Drag(int delta_x);
  void EndDragging(bool cancelled);

  // Returns true if we should bring up the titlebars, given the current mouse
  // point.
  bool ShouldBringUpTitlebars(int mouse_x, int mouse_y) const;

  // Brings up or down the titlebars for all minimized panels.
  void BringUpOrDownTitlebars(bool bring_up);

  // Returns the bottom position for the panel per its expansion state. If auto-
  // hide bottom bar is present, we want to move the minimized panel to the
  // bottom of the screen, not the bottom of the work area.
  int GetBottomPositionForExpansionState(
      Panel::ExpansionState expansion_state) const;

  int num_panels() const { return panels_.size(); }
  bool is_dragging_panel() const;

#ifdef UNIT_TEST
  const Panels& panels() const { return panels_; }
  static int horizontal_spacing() { return kPanelsHorizontalSpacing; }

  const gfx::Rect& work_area() const {
    return work_area_;
  }

  void set_auto_hiding_desktop_bar(
      AutoHidingDesktopBar* auto_hiding_desktop_bar) {
    auto_hiding_desktop_bar_ = auto_hiding_desktop_bar;
  }

  void SetWorkAreaForTesting(const gfx::Rect& work_area) {
    SetWorkArea(work_area);
  }
#endif

 private:
  enum TitlebarAction {
    NO_ACTION,
    BRING_UP,
    BRING_DOWN
  };

  PanelManager();

  // Overridden from AutoHidingDesktopBar::Observer:
  virtual void OnAutoHidingDesktopBarThicknessChanged() OVERRIDE;
  virtual void OnAutoHidingDesktopBarVisibilityChanged(
      AutoHidingDesktopBar::Alignment alignment,
      AutoHidingDesktopBar::Visibility visibility) OVERRIDE;

  // Applies the new work area. This is called by OnDisplayChanged and the test
  // code.
  void SetWorkArea(const gfx::Rect& work_area);

  // Adjusts the work area to exclude the influence of auto-hiding desktop bars.
  void AdjustWorkAreaForAutoHidingDesktopBars();

  // Handles all the panels that're delayed to be removed.
  void DelayedRemove();

  // Does the remove. Called from Remove and DelayedRemove.
  void DoRemove(Panel* panel);

  // Rearranges the positions of the panels starting from the given iterator.
  // This is called when the display space has been changed, i.e. working
  // area being changed or a panel being closed.
  void Rearrange(Panels::iterator iter_to_start, int rightmost_position);

  // Finds one panel to close so that we may have space for the new panel
  // created by |extension|.
  void FindAndClosePanelOnOverflow(const Extension* extension);

  // Help functions to drag the given panel.
  void DragLeft();
  void DragRight();

  // Checks if the titlebars have been brought up or down. If not, do not wait
  // for the notifications to trigger it any more, and start to bring them up or
  // down immediately.
  void DelayedBringUpOrDownTitlebarsCheck();

  // Does the real job of bringing up or down the titlebars.
  void DoBringUpOrDownTitlebars(bool bring_up);

  int GetRightMostAvaialblePosition() const;

  Panels panels_;

  // Stores the panels that are pending to remove. We want to delay the removal
  // when we're in the process of the dragging.
  Panels panels_pending_to_remove_;

  // The maximum work area avaialble. This area does not include the area taken
  // by the always-visible (non-auto-hiding) desktop bars.
  gfx::Rect work_area_;

  // The useable work area for computing the panel bounds. This area excludes
  // the potential area that could be taken by the auto-hiding desktop
  // bars (we only consider those bars that are aligned to bottom, left, and
  // right of the screen edges) when they become fully visible.
  gfx::Rect adjusted_work_area_;

  // Panel to drag.
  size_t dragging_panel_index_;

  // Original x coordinate of the panel to drag. This is used to get back to
  // the original position when we cancel the dragging.
  int dragging_panel_original_x_;

  // Bounds of the panel to drag. It is first set to the original bounds when
  // the dragging happens. Then it is updated to the position that will be set
  // to when the dragging ends.
  gfx::Rect dragging_panel_bounds_;

  scoped_refptr<AutoHidingDesktopBar> auto_hiding_desktop_bar_;

  TitlebarAction delayed_titlebar_action_;

  ScopedRunnableMethodFactory<PanelManager> method_factory_;

  static const int kPanelsHorizontalSpacing = 4;

  DISALLOW_COPY_AND_ASSIGN(PanelManager);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_MANAGER_H_
