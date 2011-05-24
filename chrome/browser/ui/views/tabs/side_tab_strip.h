// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SIDE_TAB_STRIP_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SIDE_TAB_STRIP_H_
#pragma once

#include "chrome/browser/ui/views/tabs/base_tab_strip.h"
#include "views/controls/button/button.h"

struct TabRendererData;

class SideTabStrip : public BaseTabStrip, public views::ButtonListener {
 public:
  // The tabs are inset by this much along all axis.
  static const int kTabStripInset;

  explicit SideTabStrip(TabStripController* controller);
  virtual ~SideTabStrip();

  // AbstractTabStripView implementation:
  virtual bool IsPositionInWindowCaption(const gfx::Point& point) OVERRIDE;
  virtual void SetBackgroundOffset(const gfx::Point& offset) OVERRIDE;

  // BaseTabStrip implementation:
  virtual void StartHighlight(int model_index) OVERRIDE;
  virtual void StopAllHighlighting() OVERRIDE;
  virtual BaseTab* CreateTabForDragging() OVERRIDE;
  virtual void RemoveTabAt(int model_index) OVERRIDE;
  virtual void SelectTabAt(int old_model_index, int new_model_index) OVERRIDE;
  virtual void TabTitleChangedNotLoading(int model_index) OVERRIDE;

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;
  virtual views::View* GetEventHandlerForPoint(
      const gfx::Point& point) OVERRIDE;

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

 protected:
  // BaseTabStrip overrides:
  virtual BaseTab* CreateTab() OVERRIDE;
  virtual void GenerateIdealBounds() OVERRIDE;
  virtual void StartInsertTabAnimation(int model_index) OVERRIDE;
  virtual void AnimateToIdealBounds() OVERRIDE;
  virtual void DoLayout() OVERRIDE;
  virtual void LayoutDraggedTabsAt(const std::vector<BaseTab*>& tabs,
                                   BaseTab* active_tab,
                                   const gfx::Point& location,
                                   bool initial_drag) OVERRIDE;
  virtual void CalculateBoundsForDraggedTabs(
      const std::vector<BaseTab*>& tabs,
      std::vector<gfx::Rect>* bounds) OVERRIDE;
  virtual int GetSizeNeededForTabs(const std::vector<BaseTab*>& tabs) OVERRIDE;

  // views::View protected overrides:
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

 private:
  // Sets |first_tab_y_offset_|. This ensures |new_offset| is legal.
  void SetFirstTabYOffset(int new_offset);

  // Returns the max y offset.
  int GetMaxOffset() const;

  // Returns the max visible y-coordinate for tabs.
  int GetMaxTabY() const;

  // Make sure the tab at |tab_index| is visible.
  void MakeTabVisible(int tab_index);

  // The "New Tab" button.
  views::View* newtab_button_;

  // Ideal bounds of the new tab button.
  gfx::Rect newtab_button_bounds_;

  // Scroll buttons.
  views::View* scroll_up_button_;
  views::View* scroll_down_button_;

  // Separator between mini-tabs and the new tab button. The separator is
  // positioned above the visible area if there are no mini-tabs.
  views::View* separator_;

  // Bounds of the sepatator.
  gfx::Rect separator_bounds_;

  // Offset the first tab (or new tab button) is positioned at. If the user has
  // scrolled the tabs this is non-zero.
  int first_tab_y_offset_;

  // Height needed to display the tabs, separator and new tab button. Doesn't
  // include any padding.
  int ideal_height_;

  DISALLOW_COPY_AND_ASSIGN(SideTabStrip);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SIDE_TAB_STRIP_H_
