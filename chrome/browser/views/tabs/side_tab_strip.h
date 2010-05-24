// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_H_
#define CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_H_

#include "chrome/browser/views/tabs/base_tab_strip.h"

struct TabRendererData;

class SideTabStrip : public BaseTabStrip {
 public:
  // The tabs are inset by this much along all axis.
  static const int kTabStripInset;

  explicit SideTabStrip(TabStripController* controller);
  virtual ~SideTabStrip();

  // BaseTabStrip implementation:
  virtual int GetPreferredHeight();
  virtual void SetBackgroundOffset(const gfx::Point& offset);
  virtual bool IsPositionInWindowCaption(const gfx::Point& point);
  virtual void SetDraggedTabBounds(int tab_index,
                                   const gfx::Rect& tab_bounds);
  virtual TabStrip* AsTabStrip();

  virtual void StartHighlight(int model_index);
  virtual void StopAllHighlighting();
  virtual BaseTab* CreateTabForDragging();
  virtual void RemoveTabAt(int model_index, bool initiated_close);
  virtual void SelectTabAt(int old_model_index, int new_model_index);
  virtual void TabTitleChangedNotLoading(int model_index);
  virtual void SetTabData(int model_index, const TabRendererData& data);

  // views::View overrides:
  virtual gfx::Size GetPreferredSize();
  virtual void PaintChildren(gfx::Canvas* canvas);

 protected:
  // BaseTabStrip overrides:
  virtual BaseTab* CreateTab();
  virtual void GenerateIdealBounds();
  virtual void StartInsertTabAnimation(int model_index, bool foreground);
  virtual void StartMoveTabAnimation();
  virtual void StopAnimating(bool layout);
  virtual void AnimateToIdealBounds();

 private:
  DISALLOW_COPY_AND_ASSIGN(SideTabStrip);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_H_
