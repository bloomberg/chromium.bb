// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SIDE_TAB_STRIP_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SIDE_TAB_STRIP_H_
#pragma once

#include "chrome/browser/ui/views/tabs/base_tab_strip.h"

struct TabRendererData;

class SideTabStrip : public BaseTabStrip {
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

 protected:
  // BaseTabStrip overrides:
  virtual BaseTab* CreateTab() OVERRIDE;
  virtual bool IgnoreTitlePrefixEliding(BaseTab* tab) OVERRIDE;
  virtual void GenerateIdealBounds() OVERRIDE;
  virtual void StartInsertTabAnimation(int model_index) OVERRIDE;
  virtual void AnimateToIdealBounds() OVERRIDE;
  virtual void DoLayout() OVERRIDE;
  virtual void LayoutDraggedTabsAt(const std::vector<BaseTab*>& tabs,
                                   const gfx::Point& location) OVERRIDE;
  virtual int GetSizeNeededForTabs(const std::vector<BaseTab*>& tabs) OVERRIDE;

 private:
  // The "New Tab" button.
  views::View* newtab_button_;

  // Ideal bounds of the new tab button.
  gfx::Rect newtab_button_bounds_;

  // Separator between mini-tabs and the new tab button. The separator is
  // positioned above the visible area if there are no mini-tabs.
  views::View* separator_;

  // Bounds of the sepatator.
  gfx::Rect separator_bounds_;

  DISALLOW_COPY_AND_ASSIGN(SideTabStrip);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SIDE_TAB_STRIP_H_
