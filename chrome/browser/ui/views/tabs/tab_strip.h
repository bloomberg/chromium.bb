// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_
#pragma once

#include "base/ref_counted.h"
#include "base/timer.h"
#include "chrome/browser/ui/views/tabs/base_tab_strip.h"
#include "ui/base/animation/animation_container.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "views/controls/button/image_button.h"
#include "views/mouse_watcher.h"

class Tab;

namespace views {
class ImageView;
#if defined(OS_LINUX)
class WidgetGtk;
#elif defined(OS_WIN)
class WidgetWin;
#endif
}

///////////////////////////////////////////////////////////////////////////////
//
// TabStrip
//
//  A View that represents the TabStripModel. The TabStrip has the
//  following responsibilities:
//    - It implements the TabStripModelObserver interface, and acts as a
//      container for Tabs, and is also responsible for creating them.
//    - It takes part in Tab Drag & Drop with Tab, TabDragHelper and
//      DraggedTab, focusing on tasks that require reshuffling other tabs
//      in response to dragged tabs.
//
///////////////////////////////////////////////////////////////////////////////
class TabStrip : public BaseTabStrip,
                 public views::ButtonListener,
                 public views::MouseWatcherListener {
 public:
  explicit TabStrip(TabStripController* controller);
  virtual ~TabStrip();

  // Creates the new tab button.
  void InitTabStripButtons();

  // Returns the bounds of the new tab button.
  gfx::Rect GetNewTabButtonBounds();

  // MouseWatcherListener overrides:
  virtual void MouseMovedOutOfView();

  // BaseTabStrip implementation:
  virtual void SetBackgroundOffset(const gfx::Point& offset);
  virtual bool IsPositionInWindowCaption(const gfx::Point& point);
  virtual void PrepareForCloseAt(int model_index);
  virtual void RemoveTabAt(int model_index);
  virtual void SelectTabAt(int old_model_index, int new_model_index);
  virtual void TabTitleChangedNotLoading(int model_index);
  virtual void StartHighlight(int model_index);
  virtual void StopAllHighlighting();
  virtual BaseTab* CreateTabForDragging();

  // views::View overrides:
  virtual void PaintChildren(gfx::Canvas* canvas);
  virtual const views::View* GetViewByID(int id) const;
  virtual gfx::Size GetPreferredSize();
  // NOTE: the drag and drop methods are invoked from FrameView. This is done to
  // allow for a drop region that extends outside the bounds of the TabStrip.
  virtual void OnDragEntered(const views::DropTargetEvent& event);
  virtual int OnDragUpdated(const views::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const views::DropTargetEvent& event);
  virtual AccessibilityTypes::Role GetAccessibleRole();
  virtual views::View* GetEventHandlerForPoint(const gfx::Point& point);
  virtual void OnThemeChanged();

 protected:
  // BaseTabStrip overrides:
  virtual BaseTab* CreateTab();
  virtual void StartInsertTabAnimation(int model_index, bool foreground);
  virtual void AnimateToIdealBounds();
  virtual bool ShouldHighlightCloseButtonAfterRemove();
  virtual void DoLayout();

  // views::View implementation:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // TabController overrides.
  virtual bool IsTabSelected(const BaseTab* btr) const;

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Horizontal gap between mini and non-mini-tabs.
  static const int mini_to_non_mini_gap_;

 private:
  friend class DraggedTabController;

  // Used during a drop session of a url. Tracks the position of the drop as
  // well as a window used to highlight where the drop occurs.
  struct DropInfo {
    DropInfo(int index, bool drop_before, bool paint_down);
    ~DropInfo();

    // Index of the tab to drop on. If drop_before is true, the drop should
    // occur between the tab at drop_index - 1 and drop_index.
    // WARNING: if drop_before is true it is possible this will == tab_count,
    // which indicates the drop should create a new tab at the end of the tabs.
    int drop_index;
    bool drop_before;

    // Direction the arrow should point in. If true, the arrow is displayed
    // above the tab and points down. If false, the arrow is displayed beneath
    // the tab and points up.
    bool point_down;

    // Renders the drop indicator.
    // TODO(beng): should be views::Widget.
#if defined(OS_WIN)
    views::WidgetWin* arrow_window;
#else
    views::WidgetGtk* arrow_window;
#endif
    views::ImageView* arrow_view;

   private:
    DISALLOW_COPY_AND_ASSIGN(DropInfo);
  };

  void Init();

  // Set the images for the new tab button.
  void LoadNewTabButtonImage();

  // Retrieves the Tab at the specified index. Remember, the specified index
  // is in terms of tab_data, *not* the model.
  Tab* GetTabAtTabDataIndex(int tab_data_index) const;

  // Returns the tab at the specified index. If a remove animation is on going
  // and the index is >= the index of the tab being removed, the index is
  // incremented. While a remove operation is on going the indices of the model
  // do not line up with the indices of the view. This method adjusts the index
  // accordingly.
  //
  // Use this instead of GetTabAtTabDataIndex if the index comes from the model.
  Tab* GetTabAtModelIndex(int model_index) const;

  // Returns the number of mini-tabs.
  int GetMiniTabCount() const;

  // -- Tab Resize Layout -----------------------------------------------------

  // Returns the exact (unrounded) current width of each tab.
  void GetCurrentTabWidths(double* unselected_width,
                           double* selected_width) const;

  // Returns the exact (unrounded) desired width of each tab, based on the
  // desired strip width and number of tabs.  If
  // |width_of_tabs_for_mouse_close_| is nonnegative we use that value in
  // calculating the desired strip width; otherwise we use the current width.
  // |mini_tab_count| gives the number of mini-tabs and |tab_count| the number
  // of mini and non-mini-tabs.
  void GetDesiredTabWidths(int tab_count,
                           int mini_tab_count,
                           double* unselected_width,
                           double* selected_width) const;

  // Perform an animated resize-relayout of the TabStrip immediately.
  void ResizeLayoutTabs();

  // Ensure that the message loop observer used for event spying is added and
  // removed appropriately so we can tell when to resize layout the tab strip.
  void AddMessageLoopObserver();
  void RemoveMessageLoopObserver();

  // -- Link Drag & Drop ------------------------------------------------------

  // Returns the bounds to render the drop at, in screen coordinates. Sets
  // |is_beneath| to indicate whether the arrow is beneath the tab, or above
  // it.
  gfx::Rect GetDropBounds(int drop_index, bool drop_before, bool* is_beneath);

  // Updates the location of the drop based on the event.
  void UpdateDropIndex(const views::DropTargetEvent& event);

  // Sets the location of the drop, repainting as necessary.
  void SetDropIndex(int tab_data_index, bool drop_before);

  // Returns the drop effect for dropping a URL on the tab strip. This does
  // not query the data in anyway, it only looks at the source operations.
  int GetDropEffect(const views::DropTargetEvent& event);

  // Returns the image to use for indicating a drop on a tab. If is_down is
  // true, this returns an arrow pointing down.
  static SkBitmap* GetDropArrowImage(bool is_down);

  // -- Animations ------------------------------------------------------------

  // Generates the ideal bounds of the TabStrip when all Tabs have finished
  // animating to their desired position/bounds. This is used by the standard
  // Layout method and other callers like the DraggedTabController that need
  // stable representations of Tab positions.
  virtual void GenerateIdealBounds();

  // Starts various types of TabStrip animations.
  void StartResizeLayoutAnimation();
  void StartMoveTabAnimation(int from_model_index,
                             int to_model_index);
  virtual void StartMiniTabAnimation();
  void StartMouseInitiatedRemoveTabAnimation(int model_index);

  // Calculates the available width for tabs, assuming a Tab is to be closed.
  int GetAvailableWidthForTabs(Tab* last_tab) const;

  // Returns true if the specified point in TabStrip coords is within the
  // hit-test region of the specified Tab.
  bool IsPointInTab(Tab* tab, const gfx::Point& point_in_tabstrip_coords);

  // -- Member Variables ------------------------------------------------------

  // The "New Tab" button.
  views::ImageButton* newtab_button_;

  // Ideal bounds of the new tab button.
  gfx::Rect newtab_button_bounds_;

  // The current widths of various types of tabs.  We save these so that, as
  // users close tabs while we're holding them at the same size, we can lay out
  // tabs exactly and eliminate the "pixel jitter" we'd get from just leaving
  // them all at their existing, rounded widths.
  double current_unselected_width_;
  double current_selected_width_;

  // If this value is nonnegative, it is used in GetDesiredTabWidths() to
  // calculate how much space in the tab strip to use for tabs.  Most of the
  // time this will be -1, but while we're handling closing a tab via the mouse,
  // we'll set this to the edge of the last tab before closing, so that if we
  // are closing the last tab and need to resize immediately, we'll resize only
  // back to this width, thus once again placing the last tab under the mouse
  // cursor.
  int available_width_for_tabs_;

  // True if PrepareForCloseAt has been invoked. When true remove animations
  // preserve current tab bounds.
  bool in_tab_close_;

  // The size of the new tab button must be hardcoded because we need to be
  // able to lay it out before we are able to get its image from the
  // ui::ThemeProvider.  It also makes sense to do this, because the size of the
  // new tab button should not need to be calculated dynamically.
  static const int kNewTabButtonWidth = 28;
  static const int kNewTabButtonHeight = 18;

  // Valid for the lifetime of a drag over us.
  scoped_ptr<DropInfo> drop_info_;

  // To ensure all tabs pulse at the same time they share the same animation
  // container. This is that animation container.
  scoped_refptr<ui::AnimationContainer> animation_container_;

  // Used for stage 1 of new tab animation.
  base::OneShotTimer<TabStrip> new_tab_timer_;

  scoped_ptr<views::MouseWatcher> mouse_watcher_;

  DISALLOW_COPY_AND_ASSIGN(TabStrip);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_
