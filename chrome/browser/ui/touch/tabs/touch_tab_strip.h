// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_STRIP_H_
#define CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_STRIP_H_
#pragma once

#include "chrome/browser/ui/views/tabs/base_tab_strip.h"

class TouchTab;

///////////////////////////////////////////////////////////////////////////////
//
// TouchTabStrip
//
//  A View that represents the TabStripModel. The TouchTabStrip has the
//  following responsibilities:
//    - It implements the TabStripModelObserver interface, and acts as a
//      container for Tabs, and is also responsible for creating them.
//
///////////////////////////////////////////////////////////////////////////////
class TouchTabStrip : public BaseTabStrip {
 public:
  explicit TouchTabStrip(TabStripController* controller);
  virtual ~TouchTabStrip();

  // AbstractTabStripView implementation:
  virtual bool IsPositionInWindowCaption(const gfx::Point& point);
  virtual void SetBackgroundOffset(const gfx::Point& offset);

  // BaseTabStrip implementation:
  virtual void PrepareForCloseAt(int model_index);
  virtual void StartHighlight(int model_index);
  virtual void StopAllHighlighting();
  virtual BaseTab* CreateTabForDragging();
  virtual void RemoveTabAt(int model_index);
  virtual void SelectTabAt(int old_model_index, int new_model_index);
  virtual void TabTitleChangedNotLoading(int model_index);
  virtual BaseTab* CreateTab();
  virtual void StartInsertTabAnimation(int model_index, bool foreground);
  virtual void AnimateToIdealBounds();
  virtual bool ShouldHighlightCloseButtonAfterRemove();
  virtual void GenerateIdealBounds();
  virtual void LayoutDraggedTabsAt(const std::vector<BaseTab*>& tabs,
                                   const gfx::Point& location);
  virtual int GetSizeNeededForTabs(const std::vector<BaseTab*>& tabs);

  // Retrieves the Tab at the specified index. Remember, the specified index
  // is in terms of tab_data, *not* the model.
  TouchTab* GetTabAtTabDataIndex(int tab_data_index) const;

 private:
  void Init();

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;
  virtual views::View::TouchStatus OnTouchEvent(
      const views::TouchEvent& event) OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE;

  // True if PrepareForCloseAt has been invoked. When true remove animations
  // preserve current tab bounds.
  bool in_tab_close_;

  // Last time the tabstrip was tapped.
  base::Time last_tap_time_;

  // The view that was tapped last.
  View* last_tapped_view_;

  DISALLOW_COPY_AND_ASSIGN(TouchTabStrip);
};

#endif  // CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_STRIP_H_
