// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_H_
#define CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_H_

#include "chrome/browser/views/tabs/base_tab_strip.h"

struct TabRendererData;

class SideTabStrip : public BaseTabStrip {
 public:
  explicit SideTabStrip(TabStripController* controller);
  virtual ~SideTabStrip();

  // BaseTabStrip implementation:
  virtual int GetPreferredHeight();
  virtual void SetBackgroundOffset(const gfx::Point& offset);
  virtual bool IsPositionInWindowCaption(const gfx::Point& point);
  virtual void SetDraggedTabBounds(int tab_index,
                                   const gfx::Rect& tab_bounds);
  virtual bool IsDragSessionActive() const;
  virtual bool IsAnimating() const;
  virtual TabStrip* AsTabStrip();

  virtual void StartHighlight(int model_index);
  virtual void StopAllHighlighting();
  virtual BaseTabRenderer* GetBaseTabAtModelIndex(int model_index) const;
  virtual BaseTabRenderer* GetBaseTabAtTabIndex(int tab_index) const;
  virtual int GetModelIndexOfBaseTab(const BaseTabRenderer* tab) const;
  virtual int GetTabCount() const;
  virtual BaseTabRenderer* CreateTabForDragging();
  virtual void AddTabAt(int model_index,
                        bool foreground,
                        const TabRendererData& data);
  virtual void RemoveTabAt(int model_index);
  virtual void SelectTabAt(int old_model_index, int new_model_index);
  virtual void MoveTab(int from_model_index, int to_model_index);
  virtual void TabTitleChangedNotLoading(int model_index);
  virtual void SetTabData(int model_index, const TabRendererData& data);
  virtual void MaybeStartDrag(BaseTabRenderer* tab,
                              const views::MouseEvent& event);
  virtual void ContinueDrag(const views::MouseEvent& event);
  virtual bool EndDrag(bool canceled);

  // views::View overrides:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 private:
  DISALLOW_COPY_AND_ASSIGN(SideTabStrip);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_H_
