// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_STRIP_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_STRIP_H_
#pragma once

#include <vector>
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher_observer.h"
#include "ui/base/animation/animation_delegate.h"

class Browser;
class PanelManager;
namespace ui {
class SlideAnimation;
}

// Manipulates all the panels that are placed on the left-most overflow area.
class PanelOverflowStrip : public PanelMouseWatcherObserver,
                           public ui::AnimationDelegate {
 public:
  typedef std::vector<Panel*> Panels;

  explicit PanelOverflowStrip(PanelManager* panel_manager);
  virtual ~PanelOverflowStrip();

  // Sets the display area of the overflow strip.
  // |display_area| is in screen coordinates.
  void SetDisplayArea(const gfx::Rect& display_area);

  // Adds a panel to the strip. |is_new| indicates if the panel is a newly
  // created panel or one that is transitioning from another grouping of panels.
  void AddPanel(Panel* panel, bool is_new);

  // Returns |false| if the panel is not in the strip.
  bool Remove(Panel* panel);
  void RemoveAll();

  // Called when a panel's expansion state changes.
  void OnPanelExpansionStateChanged(
      Panel* panel, Panel::ExpansionState old_state);

  // Refreshes the layouts for all panels to reflect any possible changes.
  void Refresh();

  void OnFullScreenModeChanged(bool is_full_screen);

  int num_panels() const { return static_cast<int>(panels_.size()); }
  Panel* first_panel() const {
    return panels_.empty() ? NULL : panels_.front();
  }

#ifdef UNIT_TEST
  const Panels& panels() const { return panels_; }
#endif

 private:
  // Overridden from PanelMouseWatcherObserver:
  virtual void OnMouseMove(const gfx::Point& mouse_position) OVERRIDE;

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  void DoRefresh(size_t start_index, size_t end_index);

  // Computes the layout of the |index| panel to fit into the area.
  // Empty bounds will be returned if this is not possible due to not enough
  // space.
  gfx::Rect ComputeLayout(size_t index,
                          const gfx::Size& iconified_size) const;

  // Used to pop up the titles for overflow panels when the mouse hovers over
  // the overflow area.
  bool ShouldShowOverflowTitles(const gfx::Point& mouse_position) const;
  void ShowOverflowTitles(bool show_overflow_titles);

  // Weak pointer since PanelManager owns PanelOverflowStrip instance.
  PanelManager* panel_manager_;

  // The queue for storing all panels.
  Panels panels_;

  // The overflow area where panels are iconified due to insufficient space
 // in the panel strip.
  gfx::Rect display_area_;

  // For mouse hover-over effect.
  bool are_overflow_titles_shown_;
  scoped_ptr<ui::SlideAnimation> overflow_hover_animator_;
  int overflow_hover_animator_start_width_;
  int overflow_hover_animator_end_width_;

  // Invalid panel index.
  static const size_t kInvalidPanelIndex = static_cast<size_t>(-1);

  DISALLOW_COPY_AND_ASSIGN(PanelOverflowStrip);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_OVERFLOW_STRIP_H_
