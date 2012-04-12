// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_OVERFLOW_PANEL_STRIP_H_
#define CHROME_BROWSER_UI_PANELS_OVERFLOW_PANEL_STRIP_H_
#pragma once

#include <vector>
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher_observer.h"
#include "ui/base/animation/animation_delegate.h"

class Browser;
class PanelManager;
class PanelOverflowIndicator;
namespace ui {
class SlideAnimation;
}

// Manipulates all the panels that are placed on the left-most overflow area.
class OverflowPanelStrip : public PanelStrip,
                           public PanelMouseWatcherObserver,
                           public ui::AnimationDelegate {
 public:
  typedef std::vector<Panel*> Panels;

  explicit OverflowPanelStrip(PanelManager* panel_manager);
  virtual ~OverflowPanelStrip();

  // PanelStrip OVERRIDES:
  virtual void SetDisplayArea(const gfx::Rect& display_area) OVERRIDE;
  virtual void RefreshLayout() OVERRIDE;
  virtual void AddPanel(Panel* panel,
                        PositioningMask positioning_mask) OVERRIDE;
  virtual void RemovePanel(Panel* panel) OVERRIDE;
  virtual void CloseAll() OVERRIDE;
  virtual void ResizePanelWindow(
      Panel* panel,
      const gfx::Size& preferred_window_size) OVERRIDE;
  virtual bool CanResizePanel(const Panel* panel) const OVERRIDE;
  virtual void OnPanelResizedByMouse(Panel* panel,
                                     const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnPanelAttentionStateChanged(Panel* panel) OVERRIDE;
  virtual void OnPanelTitlebarClicked(Panel* panel,
                                      panel::ClickModifier modifier) OVERRIDE;
  virtual void ActivatePanel(Panel* panel) OVERRIDE;
  virtual void MinimizePanel(Panel* panel) OVERRIDE;
  virtual void RestorePanel(Panel* panel) OVERRIDE;
  virtual bool IsPanelMinimized(const Panel* panel) const OVERRIDE;
  virtual bool CanShowPanelAsActive(const Panel* panel) const OVERRIDE;
  virtual void SavePanelPlacement(Panel* panel) OVERRIDE;
  virtual void RestorePanelToSavedPlacement() OVERRIDE;
  virtual void DiscardSavedPanelPlacement()  OVERRIDE;
  virtual bool CanDragPanel(const Panel* panel) const OVERRIDE;
  virtual void StartDraggingPanelWithinStrip(Panel* panel) OVERRIDE;
  virtual void DragPanelWithinStrip(Panel* panel,
                                    int delta_x,
                                    int delta_y) OVERRIDE;
  virtual void EndDraggingPanelWithinStrip(Panel* panel,
                                           bool aborted) OVERRIDE;

  virtual void UpdatePanelOnStripChange(Panel* panel) OVERRIDE;

  void OnFullScreenModeChanged(bool is_full_screen);

  int num_panels() const { return static_cast<int>(panels_.size()); }
  Panel* first_panel() const {
    return panels_.empty() ? NULL : panels_.front();
  }
  const Panels& panels() const { return panels_; }

#ifdef UNIT_TEST
  int current_display_width() const { return current_display_width_; }

  gfx::Rect display_area() const { return display_area_; }

  // This might return NULL.
  PanelOverflowIndicator* overflow_indicator() const {
    return overflow_indicator_.get();
  }

  void set_max_visible_panels(int max_visible_panels) {
    max_visible_panels_ = max_visible_panels;
  }

  void set_max_visible_panels_on_hover(int max_visible_panels_on_hover) {
    max_visible_panels_on_hover_ = max_visible_panels_on_hover;
  }
#endif

 private:
  // Overridden from PanelMouseWatcherObserver:
  virtual void OnMouseMove(const gfx::Point& mouse_position) OVERRIDE;

  // Overridden from AnimationDelegate:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  void UpdateCurrentWidth();

  void DoRefresh(size_t start_index, size_t end_index);

  // Used to update the overflow indicator.
  void UpdateMaxVisiblePanelsOnHover();
  void UpdateOverflowIndicatorCount();
  void UpdateOverflowIndicatorAttention();

  // Computes the layout of the |index| panel to fit into the area.
  // Empty bounds will be returned if this is not possible due to not enough
  // space.
  gfx::Rect ComputeLayout(size_t index,
                          const gfx::Size& iconified_size) const;

  // Used to pop up the titles for overflow panels when the mouse hovers over
  // the overflow area.
  bool ShouldShowOverflowTitles(const gfx::Point& mouse_position) const;
  void ShowOverflowTitles(bool show_overflow_titles);

  int max_visible_panels() const {
    return are_overflow_titles_shown_ ? max_visible_panels_on_hover_
                                      : max_visible_panels_;
  }

  // Weak pointer since PanelManager owns OverflowPanelStrip instance.
  PanelManager* panel_manager_;

  // The queue for storing all panels.
  Panels panels_;

  // The overflow area where panels are iconified due to insufficient space
 // in the panel strip.
  gfx::Rect display_area_;

  // Current width of the overflow area. It is the width of the panel in the
  // iconified state when the mouse does not hover over it, or the width of
  // the panel showing more info when the mouse hovers over it.
  int current_display_width_;

  // Maximium number of overflow panels allowed to be shown when the mouse
  // does not hover over the overflow area.
  int max_visible_panels_;

  // Maximium number of overflow panels allowed to be shown when the mouse
  // hovers over the overflow area and causes it to be expanded.
  int max_visible_panels_on_hover_;

  // Used to show "+N" indicator when number of overflow panels exceed
  // |max_visible_panels_|.
  scoped_ptr<PanelOverflowIndicator> overflow_indicator_;

  // For mouse hover-over effect.
  bool are_overflow_titles_shown_;
  scoped_ptr<ui::SlideAnimation> overflow_hover_animator_;
  int overflow_hover_animator_start_width_;
  int overflow_hover_animator_end_width_;

  // Invalid panel index.
  static const size_t kInvalidPanelIndex = static_cast<size_t>(-1);

  DISALLOW_COPY_AND_ASSIGN(OverflowPanelStrip);
};

#endif  // CHROME_BROWSER_UI_PANELS_OVERFLOW_PANEL_STRIP_H_
