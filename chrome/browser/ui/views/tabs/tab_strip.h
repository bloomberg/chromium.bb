// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"
#include "ui/views/view_targeter_delegate.h"

class NewTabButton;
class StackedTabStripLayout;
class Tab;
class TabDragController;
class TabStripController;
class TabStripObserver;

namespace ui {
class ListSelectionModel;
}

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
                 public views::ViewTargeterDelegate,
                 public TabController {
 public:
  static const char kViewClassName[];

  // Horizontal offset for the new tab button to bring it closer to the
  // rightmost tab.
  static const int kNewTabButtonHorizontalOffset;

  // The vertical offset of the tab strip button. This offset applies only to
  // restored windows.
  static const int kNewTabButtonVerticalOffset;

  // The size of the new tab button must be hardcoded because we need to be
  // able to lay it out before we are able to get its image from the
  // ui::ThemeProvider.  It also makes sense to do this, because the size of the
  // new tab button should not need to be calculated dynamically.
  static const int kNewTabButtonAssetWidth;
  static const int kNewTabButtonAssetHeight;

  explicit TabStrip(TabStripController* controller);
  virtual ~TabStrip();

  // Add and remove observers to changes within this TabStrip.
  void AddObserver(TabStripObserver* observer);
  void RemoveObserver(TabStripObserver* observer);

  // If |adjust_layout| is true the stacked layout changes based on whether the
  // user uses a mouse or a touch device with the tabstrip.
  void set_adjust_layout(bool adjust_layout) { adjust_layout_ = adjust_layout; }

  // |stacked_layout_| defines what should happen when the tabs won't fit at
  // their ideal size. When |stacked_layout_| is true the tabs are always sized
  // to their ideal size and stacked on top of each other so that only a certain
  // set of tabs are visible. This is used when the user uses a touch device.
  // When |stacked_layout_| is false the tabs shrink to accommodate the
  // available space. This is the default.
  bool stacked_layout() const { return stacked_layout_; }

  // Sets |stacked_layout_| and animates if necessary.
  void SetStackedLayout(bool stacked_layout);

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

  // Returns true if the tab is not partly or fully clipped (due to overflow),
  // and the tab couldn't become partly clipped due to changing the selected tab
  // (for example, if currently the strip has the last tab selected, and
  // changing that to the first tab would cause |tab| to be pushed over enough
  // to clip).
  bool ShouldTabBeVisible(const Tab* tab) const;

  // Invoked from the controller when the close initiates from the TabController
  // (the user clicked the tab close button or middle clicked the tab). This is
  // invoked from Close. Because of unload handlers Close is not always
  // immediately followed by RemoveTabAt.
  void PrepareForCloseAt(int model_index, CloseTabSource source);

  // Invoked when the selection changes from |old_selection| to
  // |new_selection|.
  void SetSelection(const ui::ListSelectionModel& old_selection,
                    const ui::ListSelectionModel& new_selection);

  // Invoked when the title of a tab changes and the tab isn't loading.
  void TabTitleChangedNotLoading(int model_index);

  // Retrieves the ideal bounds for the Tab at the specified index.
  const gfx::Rect& ideal_bounds(int tab_data_index) {
    return tabs_.ideal_bounds(tab_data_index);
  }

  // Returns the Tab at |index|.
  Tab* tab_at(int index) const {
    return static_cast<Tab*>(tabs_.view_at(index));
  }

  // Returns the index of the specified tab in the model coordinate system, or
  // -1 if tab is closing or not valid.
  int GetModelIndexOfTab(const Tab* tab) const;

  // Gets the number of Tabs in the tab strip.
  int tab_count() const { return tabs_.view_size(); }

  // Cover method for TabStripController::GetCount.
  int GetModelCount() const;

  // Cover method for TabStripController::IsValidIndex.
  bool IsValidModelIndex(int model_index) const;

  TabStripController* controller() const { return controller_.get(); }

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

  // Returns true if the specified rect (in TabStrip coordinates) intersects
  // the window caption area of the browser window.
  bool IsRectInWindowCaption(const gfx::Rect& rect);

  // Set the background offset used by inactive tabs to match the frame image.
  void SetBackgroundOffset(const gfx::Point& offset);

  // Sets a painting style with miniature "tab indicator" rectangles at the top.
  void SetImmersiveStyle(bool enable);

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  bool IsAnimating() const;

  // Stops any ongoing animations. If |layout| is true and an animation is
  // ongoing this does a layout.
  void StopAnimating(bool layout);

  // Called to indicate whether the given URL is a supported file.
  void FileSupported(const GURL& url, bool supported);

  // TabController overrides:
  virtual const ui::ListSelectionModel& GetSelectionModel() OVERRIDE;
  virtual bool SupportsMultipleSelection() OVERRIDE;
  virtual void SelectTab(Tab* tab) OVERRIDE;
  virtual void ExtendSelectionTo(Tab* tab) OVERRIDE;
  virtual void ToggleSelected(Tab* tab) OVERRIDE;
  virtual void AddSelectionFromAnchorTo(Tab* tab) OVERRIDE;
  virtual void CloseTab(Tab* tab, CloseTabSource source) OVERRIDE;
  virtual void ToggleTabAudioMute(Tab* tab) OVERRIDE;
  virtual void ShowContextMenuForTab(Tab* tab,
                                     const gfx::Point& p,
                                     ui::MenuSourceType source_type) OVERRIDE;
  virtual bool IsActiveTab(const Tab* tab) const OVERRIDE;
  virtual bool IsTabSelected(const Tab* tab) const OVERRIDE;
  virtual bool IsTabPinned(const Tab* tab) const OVERRIDE;
  virtual void MaybeStartDrag(
      Tab* tab,
      const ui::LocatedEvent& event,
      const ui::ListSelectionModel& original_selection) OVERRIDE;
  virtual void ContinueDrag(views::View* view,
                            const ui::LocatedEvent& event) OVERRIDE;
  virtual bool EndDrag(EndDragReason reason) OVERRIDE;
  virtual Tab* GetTabAt(Tab* tab,
                        const gfx::Point& tab_in_tab_coordinates) OVERRIDE;
  virtual void OnMouseEventInTab(views::View* source,
                                 const ui::MouseEvent& event) OVERRIDE;
  virtual bool ShouldPaintTab(const Tab* tab, gfx::Rect* clip) OVERRIDE;
  virtual bool IsImmersiveStyle() const OVERRIDE;
  virtual void UpdateTabAccessibilityState(const Tab* tab,
                                           ui::AXViewState* state) OVERRIDE;

  // MouseWatcherListener overrides:
  virtual void MouseMovedOutOfHost() OVERRIDE;

  // views::View overrides:
  virtual void Layout() OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas,
                             const views::CullSet& cull_set) OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  // NOTE: the drag and drop methods are invoked from FrameView. This is done
  // to allow for a drop region that extends outside the bounds of the TabStrip.
  virtual void OnDragEntered(const ui::DropTargetEvent& event) OVERRIDE;
  virtual int OnDragUpdated(const ui::DropTargetEvent& event) OVERRIDE;
  virtual void OnDragExited() OVERRIDE;
  virtual int OnPerformDrop(const ui::DropTargetEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual views::View* GetTooltipHandlerForPoint(
      const gfx::Point& point) OVERRIDE;

  // Returns preferred height in immersive style.
  static int GetImmersiveHeight();

 private:
  typedef std::vector<Tab*> Tabs;
  typedef std::map<int, Tabs> TabsClosingMap;
  typedef std::pair<TabsClosingMap::iterator,
                    Tabs::iterator> FindClosingTabResult;

  class RemoveTabDelegate;

  friend class TabDragController;
  friend class TabDragControllerTest;
  FRIEND_TEST_ALL_PREFIXES(TabDragControllerTest, GestureEndShouldEndDragTest);
  friend class TabStripTest;
  FRIEND_TEST_ALL_PREFIXES(TabStripTest, TabHitTestMaskWhenStacked);
  FRIEND_TEST_ALL_PREFIXES(TabStripTest, ClippedTabCloseButton);

  // Used during a drop session of a url. Tracks the position of the drop as
  // well as a window used to highlight where the drop occurs.
  struct DropInfo {
    DropInfo(int drop_index,
             bool drop_before,
             bool point_down,
             views::Widget* context);
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

    // The URL for the drop event.
    GURL url;

    // Whether the MIME type of the file pointed to by |url| is supported.
    bool file_supported;

   private:
    DISALLOW_COPY_AND_ASSIGN(DropInfo);
  };

  // Horizontal gap between mini and non-mini-tabs.
  static const int kMiniToNonMiniGap;

  void Init();

  // Creates and returns a new tab. The caller owners the returned tab.
  Tab* CreateTab();

  // Invoked from |AddTabAt| after the newly created tab has been inserted.
  void StartInsertTabAnimation(int model_index);

  // Invoked from |MoveTab| after |tab_data_| has been updated to animate the
  // move.
  void StartMoveTabAnimation();

  // Starts the remove tab animation.
  void StartRemoveTabAnimation(int model_index);

  // Schedules the animations and bounds changes necessary for a remove tab
  // animation.
  void ScheduleRemoveTabAnimation(Tab* tab);

  // Animates all the views to their ideal bounds.
  // NOTE: this does *not* invoke GenerateIdealBounds, it uses the bounds
  // currently set in ideal_bounds.
  void AnimateToIdealBounds();

  // Returns whether the highlight button should be highlighted after a remove.
  bool ShouldHighlightCloseButtonAfterRemove();

  // Invoked from Layout if the size changes or layout is really needed.
  void DoLayout();

  // Sets the visibility state of all tabs based on ShouldTabBeVisible().
  void SetTabVisibility();

  // Drags the active tab by |delta|. |initial_positions| is the x-coordinates
  // of the tabs when the drag started.
  void DragActiveTab(const std::vector<int>& initial_positions, int delta);

  // Sets the ideal bounds x-coordinates to |positions|.
  void SetIdealBoundsFromPositions(const std::vector<int>& positions);

  // Stacks the dragged tabs. This is used if the drag operation is
  // MOVE_VISIBLE_TABS and the tabs don't fill the tabstrip. When this happens
  // the active tab follows the mouse and the other tabs stack around it.
  void StackDraggedTabs(int delta);

  // Returns true if dragging has resulted in temporarily stacking the tabs.
  bool IsStackingDraggedTabs() const;

  // Invoked during drag to layout the tabs being dragged in |tabs| at
  // |location|. If |initial_drag| is true, this is the initial layout after the
  // user moved the mouse far enough to trigger a drag.
  void LayoutDraggedTabsAt(const Tabs& tabs,
                           Tab* active_tab,
                           const gfx::Point& location,
                           bool initial_drag);

  // Calculates the bounds needed for each of the tabs, placing the result in
  // |bounds|.
  void CalculateBoundsForDraggedTabs(const Tabs& tabs,
                                     std::vector<gfx::Rect>* bounds);

  // Returns the size needed for the specified tabs. This is invoked during drag
  // and drop to calculate offsets and positioning.
  int GetSizeNeededForTabs(const Tabs& tabs);

  // Returns the number of mini-tabs.
  int GetMiniTabCount() const;

  // Returns the last tab in the strip that's actually visible.  This will be
  // the actual last tab unless the strip is in the overflow state.
  const Tab* GetLastVisibleTab() const;

  // Adds the tab at |index| to |tabs_closing_map_| and removes the tab from
  // |tabs_|.
  void RemoveTabFromViewModel(int index);

  // Cleans up the Tab from the TabStrip. This is called from the tab animation
  // code and is not a general-purpose method.
  void RemoveAndDeleteTab(Tab* tab);

  // Adjusts the indices of all tabs in |tabs_closing_map_| whose index is
  // >= |index| to have a new index of |index + delta|.
  void UpdateTabsClosingMap(int index, int delta);

  // Used by TabDragController when the user starts or stops dragging tabs.
  void StartedDraggingTabs(const Tabs& tabs);

  // Invoked when TabDragController detaches a set of tabs.
  void DraggedTabsDetached();

  // Used by TabDragController when the user stops dragging tabs. |move_only| is
  // true if the move behavior is TabDragController::MOVE_VISIBILE_TABS.
  // |completed| is true if the drag operation completed successfully, false if
  // it was reverted.
  void StoppedDraggingTabs(const Tabs& tabs,
                           const std::vector<int>& initial_positions,
                           bool move_only,
                           bool completed);

  // Invoked from StoppedDraggingTabs to cleanup |tab|. If |tab| is known
  // |is_first_tab| is set to true.
  void StoppedDraggingTab(Tab* tab, bool* is_first_tab);

  // Takes ownership of |controller|.
  void OwnDragController(TabDragController* controller);

  // Destroys the current TabDragController. This cancel the existing drag
  // operation.
  void DestroyDragController();

  // Releases ownership of the current TabDragController.
  TabDragController* ReleaseDragController();

  // Finds |tab| in the |tab_closing_map_| and returns a pair of iterators
  // indicating precisely where it is.
  FindClosingTabResult FindClosingTab(const Tab* tab);

  // Paints all the tabs in |tabs_closing_map_[index]|.
  void PaintClosingTabs(gfx::Canvas* canvas,
                        int index,
                        const views::CullSet& cull_set);

  // Invoked when a mouse event occurs over |source|. Potentially switches the
  // |stacked_layout_|.
  void UpdateStackedLayoutFromMouseEvent(views::View* source,
                                         const ui::MouseEvent& event);

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

  // Invokes ResizeLayoutTabs() as long as we're not in a drag session. If we
  // are in a drag session this restarts the timer.
  void ResizeLayoutTabsFromTouch();

  // Restarts |resize_layout_timer_|.
  void StartResizeLayoutTabsFromTouchTimer();

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
  void UpdateDropIndex(const ui::DropTargetEvent& event);

  // Sets the location of the drop, repainting as necessary.
  void SetDropIndex(int tab_data_index, bool drop_before);

  // Returns the drop effect for dropping a URL on the tab strip. This does
  // not query the data in anyway, it only looks at the source operations.
  int GetDropEffect(const ui::DropTargetEvent& event);

  // Returns the image to use for indicating a drop on a tab. If is_down is
  // true, this returns an arrow pointing down.
  static gfx::ImageSkia* GetDropArrowImage(bool is_down);

  // -- Animations ------------------------------------------------------------

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
  static int new_tab_button_width() {
    return kNewTabButtonAssetWidth + kNewTabButtonHorizontalOffset;
  }

  // Returns the width of the area that contains tabs. This does not include
  // the width of the new tab button.
  int tab_area_width() const { return width() - new_tab_button_width(); }

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

  // Returns the tab to use for event handling. This uses FindTabForEventFrom()
  // to do the actual searching.
  Tab* FindTabForEvent(const gfx::Point& point);

  // Returns the tab to use for event handling starting at index |start| and
  // iterating by |delta|.
  Tab* FindTabForEventFrom(const gfx::Point& point, int start, int delta);

  // For a given point, finds a tab that is hit by the point. If the point hits
  // an area on which two tabs are overlapping, the tab is selected as follows:
  // - If one of the tabs is active, select it.
  // - Select the left one.
  // If no tabs are hit, returns NULL.
  views::View* FindTabHitByPoint(const gfx::Point& point);

  // Returns the x-coordinates of the tabs.
  std::vector<int> GetTabXCoordinates();

  // Creates/Destroys |touch_layout_| as necessary.
  void SwapLayoutIfNecessary();

  // Returns true if |touch_layout_| is needed.
  bool NeedsTouchLayout() const;

  // Sets the value of |reset_to_shrink_on_exit_|. If true |mouse_watcher_| is
  // used to track when the mouse truly exits the tabstrip and the stacked
  // layout is reset.
  void SetResetToShrinkOnExit(bool value);

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // View overrides.
  virtual const views::View* GetViewByID(int id) const OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnMouseMoved(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;

  // ui::EventHandler overrides.
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // views::ViewTargeterDelegate:
  virtual views::View* TargetForRect(views::View* root,
                                     const gfx::Rect& rect) OVERRIDE;

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
  scoped_refptr<gfx::AnimationContainer> animation_container_;

  // MouseWatcher is used for two things:
  // . When a tab is closed to reset the layout.
  // . When a mouse is used and the layout dynamically adjusts and is currently
  //   stacked (|stacked_layout_| is true).
  scoped_ptr<views::MouseWatcher> mouse_watcher_;

  // The controller for a drag initiated from a Tab. Valid for the lifetime of
  // the drag session.
  scoped_ptr<TabDragController> drag_controller_;

  views::BoundsAnimator bounds_animator_;

  // Size we last layed out at.
  gfx::Size last_layout_size_;

  // See description above stacked_layout().
  bool stacked_layout_;

  // Should the layout dynamically adjust?
  bool adjust_layout_;

  // Only used while in touch mode.
  scoped_ptr<StackedTabStripLayout> touch_layout_;

  // If true the |stacked_layout_| is set to false when the mouse exits the
  // tabstrip (as determined using MouseWatcher).
  bool reset_to_shrink_on_exit_;

  // Location of the mouse at the time of the last move.
  gfx::Point last_mouse_move_location_;

  // Time of the last mouse move event.
  base::TimeTicks last_mouse_move_time_;

  // Number of mouse moves.
  int mouse_move_count_;

  // Timer used when a tab is closed and we need to relayout. Only used when a
  // tab close comes from a touch device.
  base::OneShotTimer<TabStrip> resize_layout_timer_;

  // True if tabs are painted as rectangular light-bars.
  bool immersive_style_;

  // Our observers.
  typedef ObserverList<TabStripObserver> TabStripObservers;
  TabStripObservers observers_;

  DISALLOW_COPY_AND_ASSIGN(TabStrip);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_H_
