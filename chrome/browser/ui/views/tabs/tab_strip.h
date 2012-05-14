// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/timer.h"
#include "chrome/browser/ui/views/tabs/base_tab.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "ui/base/animation/animation_container.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

class BaseTab;
class NewTabButton;
class Tab;
class TabDragController;
class TabStripController;
class TabStripSelectionModel;
class TouchTabStripLayout;

namespace views {
class ImageView;
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
class TabStrip : public views::View,
                 public views::ButtonListener,
                 public views::MouseWatcherListener,
                 public TabController {
 public:
  static const char kViewClassName[];

  explicit TabStrip(TabStripController* controller);
  virtual ~TabStrip();

  // Returns the bounds of the new tab button.
  gfx::Rect GetNewTabButtonBounds();

  // Returns true if the new tab button should be sized to the top of the tab
  // strip.
  bool SizeTabButtonToTopOfTabStrip();

  // Starts highlighting the tab at the specified index.
  void StartHighlight(int model_index);

  // Stops all tab higlighting.
  void StopAllHighlighting();

  // Adds a tab at the specified index.
  void AddTabAt(int model_index, const TabRendererData& data, bool is_active);

  // Moves a tab.
  void MoveTab(int from_model_index,
               int to_model_index,
               const TabRendererData& data);

  // Removes a tab at the specified index.
  void RemoveTabAt(int model_index);

  // Sets the tab data at the specified model index.
  void SetTabData(int model_index, const TabRendererData& data);

  // Invoked from the controller when the close initiates from the TabController
  // (the user clicked the tab close button or middle clicked the tab). This is
  // invoked from Close. Because of unload handlers Close is not always
  // immediately followed by RemoveTabAt.
  void PrepareForCloseAt(int model_index);

  // Invoked when the selection changes from |old_selection| to
  // |new_selection|.
  void SetSelection(const TabStripSelectionModel& old_selection,
                    const TabStripSelectionModel& new_selection);

  // Invoked when the title of a tab changes and the tab isn't loading.
  void TabTitleChangedNotLoading(int model_index);

  // Retrieves the ideal bounds for the Tab at the specified index.
  const gfx::Rect& ideal_bounds(int tab_data_index) {
    return tabs_.ideal_bounds(tab_data_index);
  }

  // Returns the Tab at |index|.
  Tab* tab_at(int index) const;

  // Returns the index of the specified tab in the model coordiate system, or
  // -1 if tab is closing or not valid.
  int GetModelIndexOfBaseTab(const BaseTab* tab) const;

  // Gets the number of Tabs in the tab strip.
  int tab_count() const { return tabs_.view_size(); }

  // Cover method for TabStripController::GetCount.
  int GetModelCount() const;

  // Cover method for TabStripController::IsValidIndex.
  bool IsValidModelIndex(int model_index) const;

  TabStripController* controller() const { return controller_.get(); }

  // Creates and returns a tab that can be used for dragging. Ownership passes
  // to the caller.
  BaseTab* CreateTabForDragging();

  // Returns true if a drag session is currently active.
  bool IsDragSessionActive() const;

  // Returns true if a tab is being dragged into this tab strip.
  bool IsActiveDropTarget() const;

  // Returns true if the tab strip is editable. Returns false if the tab strip
  // is being dragged or animated to prevent extensions from messing things up
  // while that's happening.
  bool IsTabStripEditable() const;

  // Returns false when there is a drag operation in progress so that the frame
  // doesn't close.
  bool IsTabStripCloseable() const;

  // Updates the loading animations displayed by tabs in the tabstrip to the
  // next frame.
  void UpdateLoadingAnimations();

  // Returns true if the specified point (in TabStrip coordinates) is in the
  // window caption area of the browser window.
  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Set the background offset used by inactive tabs to match the frame image.
  void SetBackgroundOffset(const gfx::Point& offset);

  // Returns the new tab button. This is never NULL.
  views::View* newtab_button();

  // TabController overrides:
  virtual const TabStripSelectionModel& GetSelectionModel() OVERRIDE;
  virtual bool SupportsMultipleSelection() OVERRIDE;
  virtual void SelectTab(BaseTab* tab) OVERRIDE;
  virtual void ExtendSelectionTo(BaseTab* tab) OVERRIDE;
  virtual void ToggleSelected(BaseTab* tab) OVERRIDE;
  virtual void AddSelectionFromAnchorTo(BaseTab* tab) OVERRIDE;
  virtual void CloseTab(BaseTab* tab) OVERRIDE;
  virtual void ShowContextMenuForTab(BaseTab* tab,
                                     const gfx::Point& p) OVERRIDE;
  virtual bool IsActiveTab(const BaseTab* tab) const OVERRIDE;
  virtual bool IsTabSelected(const BaseTab* tab) const OVERRIDE;
  virtual bool IsTabPinned(const BaseTab* tab) const OVERRIDE;
  virtual bool IsTabCloseable(const BaseTab* tab) const OVERRIDE;
  virtual void MaybeStartDrag(
      BaseTab* tab,
      const views::LocatedEvent& event,
      const TabStripSelectionModel& original_selection) OVERRIDE;
  virtual void ContinueDrag(const views::MouseEvent& event) OVERRIDE;
  virtual bool EndDrag(bool canceled) OVERRIDE;
  virtual BaseTab* GetTabAt(BaseTab* tab,
                            const gfx::Point& tab_in_tab_coordinates) OVERRIDE;
  virtual void ClickActiveTab(const BaseTab* tab) const OVERRIDE;

  // MouseWatcherListener overrides:
  virtual void MouseMovedOutOfHost() OVERRIDE;

  // views::View overrides:
  virtual void Layout() OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  // NOTE: the drag and drop methods are invoked from FrameView. This is done
  // to allow for a drop region that extends outside the bounds of the TabStrip.
  virtual void OnDragEntered(const views::DropTargetEvent& event) OVERRIDE;
  virtual int OnDragUpdated(const views::DropTargetEvent& event) OVERRIDE;
  virtual void OnDragExited() OVERRIDE;
  virtual int OnPerformDrop(const views::DropTargetEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual views::View* GetEventHandlerForPoint(
      const gfx::Point& point) OVERRIDE;

 protected:
  // Horizontal gap between mini and non-mini-tabs.
  static const int kMiniToNonMiniGap;

  void set_ideal_bounds(int index, const gfx::Rect& bounds) {
    tabs_.set_ideal_bounds(index, bounds);
  }

  // Returns the number of mini-tabs.
  int GetMiniTabCount() const;

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // View overrides.
  virtual const views::View* GetViewByID(int id) const OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;

 private:
  typedef std::map<int, std::vector<BaseTab*> > TabsClosingMap;

  class RemoveTabDelegate;

  friend class TabDragController;
  friend class TabDragControllerTest;

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
    views::Widget* arrow_window;
    views::ImageView* arrow_view;

   private:
    DISALLOW_COPY_AND_ASSIGN(DropInfo);
  };

  void Init();

  // Creates and returns a new tab. The caller owners the returned tab.
  BaseTab* CreateTab();

  // Invoked from |AddTabAt| after the newly created tab has been inserted.
  void StartInsertTabAnimation(int model_index);

  // Invoked from |MoveTab| after |tab_data_| has been updated to animate the
  // move.
  void StartMoveTabAnimation();

  // Starts the remove tab animation.
  void StartRemoveTabAnimation(int model_index);

  // Schedules the animations and bounds changes necessary for a remove tab
  // animation.
  void ScheduleRemoveTabAnimation(BaseTab* tab);

  // Stops any ongoing animations. If |layout| is true and an animation is
  // ongoing this does a layout.
  void StopAnimating(bool layout);

  // Animates all the views to their ideal bounds.
  // NOTE: this does *not* invoke GenerateIdealBounds, it uses the bounds
  // currently set in ideal_bounds.
  void AnimateToIdealBounds();

  // Returns whether the highlight button should be highlighted after a remove.
  bool ShouldHighlightCloseButtonAfterRemove();

  // Invoked from Layout if the size changes or layout is really needed.
  void DoLayout();

  // Drags the active tab by |delta|. |initial_positions| is the x-coordinates
  // of the tabs when the drag started.
  void DragActiveTab(const std::vector<int>& initial_positions, int delta);

  // Invoked during drag to layout the tabs being dragged in |tabs| at
  // |location|. If |initial_drag| is true, this is the initial layout after the
  // user moved the mouse far enough to trigger a drag.
  void LayoutDraggedTabsAt(const std::vector<BaseTab*>& tabs,
                           BaseTab* active_tab,
                           const gfx::Point& location,
                           bool initial_drag);

  // Calculates the bounds needed for each of the tabs, placing the result in
  // |bounds|.
  void CalculateBoundsForDraggedTabs(
      const std::vector<BaseTab*>& tabs,
      std::vector<gfx::Rect>* bounds);

  // Returns the size needed for the specified tabs. This is invoked during drag
  // and drop to calculate offsets and positioning.
  int GetSizeNeededForTabs(const std::vector<BaseTab*>& tabs);

  // Adds the tab at |index| to |tabs_closing_map_| and removes the tab from
  // |tabs_|.
  void RemoveTabFromViewModel(int index);

  // Cleans up the Tab from the TabStrip. This is called from the tab animation
  // code and is not a general-purpose method.
  void RemoveAndDeleteTab(BaseTab* tab);

  // Adjusts the indices of all tabs in |tabs_closing_map_| whose index is
  // >= |index| to have a new index of |index + delta|.
  void UpdateTabsClosingMap(int index, int delta);

  // Used by TabDragController when the user starts or stops dragging tabs.
  void StartedDraggingTabs(const std::vector<BaseTab*>& tabs);

  // Invoked when TabDragController detaches a set of tabs.
  void DraggedTabsDetached();

  // Used by TabDragController when the user stops dragging tabs.
  void StoppedDraggingTabs(const std::vector<BaseTab*>& tabs);

  // Invoked from StoppedDraggingTabs to cleanup |tab|. If |tab| is known
  // |is_first_tab| is set to true.
  void StoppedDraggingTab(BaseTab* tab, bool* is_first_tab);

  // Takes ownership of |controller|.
  void OwnDragController(TabDragController* controller);

  // Destroys the current TabDragController. This cancel the existing drag
  // operation.
  void DestroyDragController();

  // Releases ownership of the current TabDragController.
  TabDragController* ReleaseDragController();

  // Paints all the tabs in |tabs_closing_map_[index]|.
  void PaintClosingTabs(gfx::Canvas* canvas, int index);

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

  // Sets the bounds of the tabs to |tab_bounds|.
  void SetTabBoundsForDrag(const std::vector<gfx::Rect>& tab_bounds);

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
  static gfx::ImageSkia* GetDropArrowImage(bool is_down);

  // -- Animations ------------------------------------------------------------

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  bool IsAnimating() const;

  // Invoked prior to starting a new animation.
  void PrepareForAnimation();

  // Generates the ideal bounds for each of the tabs as well as the new tab
  // button.
  void GenerateIdealBounds();

  // Generates the ideal bounds for the mini tabs. Returns the index to position
  // the first non-mini tab and sets |first_non_mini_index| to the index of the
  // first non-mini tab.
  int GenerateIdealBoundsForMiniTabs(int* first_non_mini_index);

  // Returns the width needed for the new tab button (and padding).
  int new_tab_button_width() const;

  // Starts various types of TabStrip animations.
  void StartResizeLayoutAnimation();
  void StartMiniTabAnimation();
  void StartMouseInitiatedRemoveTabAnimation(int model_index);

  // Returns true if the specified point in TabStrip coords is within the
  // hit-test region of the specified Tab.
  bool IsPointInTab(Tab* tab, const gfx::Point& point_in_tabstrip_coords);

  // -- Touch Layout ----------------------------------------------------------

  // Returns the position normal tabs start at.
  int GetStartXForNormalTabs() const;

  // Returns the tab to use for event handling starting at index |start| and
  // iterating by |delta|.
  Tab* FindTabForEvent(const gfx::Point& point, int start, int delta);

  // Returns the x-coordinates of the tabs.
  std::vector<int> GetTabXCoordinates();

  // -- Member Variables ------------------------------------------------------

  // There is a one-to-one mapping between each of the tabs in the
  // TabStripController (TabStripModel) and |tabs_|. Because we animate tab
  // removal there exists a period of time where a tab is displayed but not in
  // the model. When this occurs the tab is removed from |tabs_| and placed in
  // |tabs_closing_map_|. When the animation completes the tab is removed from
  // |tabs_closing_map_|. The painting code ensures both sets of tabs are
  // painted, and the event handling code ensures only tabs in |tabs_| are used.
  views::ViewModel tabs_;
  TabsClosingMap tabs_closing_map_;

  scoped_ptr<TabStripController> controller_;

  // The "New Tab" button.
  NewTabButton* newtab_button_;

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

  // Valid for the lifetime of a drag over us.
  scoped_ptr<DropInfo> drop_info_;

  // To ensure all tabs pulse at the same time they share the same animation
  // container. This is that animation container.
  scoped_refptr<ui::AnimationContainer> animation_container_;

  // Used for stage 1 of new tab animation.
  base::OneShotTimer<TabStrip> new_tab_timer_;

  scoped_ptr<views::MouseWatcher> mouse_watcher_;

  // The controller for a drag initiated from a Tab. Valid for the lifetime of
  // the drag session.
  scoped_ptr<TabDragController> drag_controller_;

  views::BoundsAnimator bounds_animator_;

  // Size we last layed out at.
  gfx::Size last_layout_size_;

  // Only used while in touch mode.
  scoped_ptr<TouchTabStripLayout> touch_layout_;

  DISALLOW_COPY_AND_ASSIGN(TabStrip);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_
