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
// TODO(wyck): Use transformable views for scrolling.
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
  virtual void StartInsertTabAnimation(int model_index);
  virtual void AnimateToIdealBounds();
  virtual bool ShouldHighlightCloseButtonAfterRemove();
  virtual void GenerateIdealBounds();
  virtual void LayoutDraggedTabsAt(const std::vector<BaseTab*>& tabs,
                                   BaseTab* active_tab,
                                   const gfx::Point& location,
                                   bool initial_drag);
  virtual void CalculateBoundsForDraggedTabs(
      const std::vector<BaseTab*>& tabs,
      std::vector<gfx::Rect>* bounds);
  virtual int GetSizeNeededForTabs(const std::vector<BaseTab*>& tabs);

  // views::View overrides:
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;

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

  // Adjusts the state of scroll interaction when a mouse press occurs at the
  // given point.  Sets an appropriate |initial_scroll_offset_|.
  void BeginScroll(const gfx::Point& point);

  // Adjusts the state of scroll interaction when the mouse is dragged to the
  // given point.  If the scroll is not beyond the minimum threshold, the tabs
  // will not actually scroll.
  void ContinueScroll(const gfx::Point& point);

  // Adjusts the state of scroll interaction when the mouse is released.  Either
  // scrolls to the final mouse release point or selects the current tab
  // depending on whether the mouse was dragged beyone the minimum threshold.
  void EndScroll(const gfx::Point& point);

  // Adjust the state of scroll interaction when the mouse capture is lost.  It
  // scrolls back to the original position before the scroll began.
  void CancelScroll();

  // Adjust the positions of the tabs to perform a scroll of |delta_x| relative
  // to the |initial_scroll_offset_|.
  void ScrollTo(int delta_x);

  // True if PrepareForCloseAt has been invoked. When true remove animations
  // preserve current tab bounds.
  bool in_tab_close_;

  // Last time the tabstrip was tapped.
  base::Time last_tap_time_;

  // The view that was tapped last.
  View* last_tapped_view_;

  // Records the mouse x coordinate at the start of a drag operation.
  int initial_mouse_x_;

  // Records the scroll offset at the time of the start of a drag operation.
  int initial_scroll_offset_;

  // The current offset of the view.  Positive scroll offsets move the icons to
  // the left.  Negative scroll offsets move the icons to the right.
  int scroll_offset_;

  // State of the scrolling interaction.  Will be true once the drag has been
  // displaced beyond the minimum dragging threshold.
  bool scrolling_;

  // Records the tab that was under the initial mouse press.  Must match the
  // tab that was under the final mouse release in order for the tab to
  // be selected.
  TouchTab* initial_tab_;

  // The minimum value that |scroll_offset_| can have.  Based on the total
  // width of all the content to be scrolled, less the viewport size.
  int min_scroll_offset_;

  DISALLOW_COPY_AND_ASSIGN(TouchTabStrip);
};

#endif  // CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_STRIP_H_
