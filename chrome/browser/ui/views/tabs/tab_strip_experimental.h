// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_EXPERIMENTAL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_EXPERIMENTAL_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_experimental_observer.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_experimental.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"
#include "ui/views/view_targeter_delegate.h"

class NewTabButton;
class TabDragController;
class TabExperimental;
class TabStripModelExperimental;

namespace ui {
class ListSelectionModel;
}

namespace views {
class ImageView;
}

class TabStripExperimental : public TabStrip,
                             public views::ButtonListener,
                             public views::MouseWatcherListener,
                             public views::ViewTargeterDelegate,
                             public TabStripModelExperimentalObserver {
 public:
  explicit TabStripExperimental(TabStripModel* model);
  ~TabStripExperimental() override;

  // TabStrip implementation:
  TabStripImpl* AsTabStripImpl() override;
  int GetMaxX() const override;
  void SetBackgroundOffset(const gfx::Point& offset) override;
  bool IsRectInWindowCaption(const gfx::Rect& rect) override;
  bool IsPositionInWindowCaption(const gfx::Point& point) override;
  bool IsTabStripCloseable() const override;
  bool IsTabStripEditable() const override;
  bool IsTabCrashed(int tab_index) const override;
  bool TabHasNetworkError(int tab_index) const override;
  TabAlertState GetTabAlertState(int tab_index) const override;
  void UpdateLoadingAnimations() override;

  // Returns the bounds of the new tab button.
  gfx::Rect GetNewTabButtonBounds();

  // Returns true if the new tab button should be sized to the top of the tab
  // strip.
  bool SizeTabButtonToTopOfTabStrip();

  // Starts highlighting the tab at the specified index.
  void StartHighlight(int model_index);

  // Stops all tab higlighting.
  void StopAllHighlighting();

  // Moves a tab.
  /* TODO(brettw) moving.
  void MoveTab(int from_model_index,
               int to_model_index,
               const TabRendererData& data);
  */

  // Invoked from the controller when the close initiates from the TabController
  // (the user clicked the tab close button or middle clicked the tab). This is
  // invoked from Close. Because of unload handlers Close is not always
  // immediately followed by RemoveTabAt.
  void PrepareForCloseAt(int model_index, CloseTabSource source);

  // Invoked when a tab needs to show UI that it needs the user's attention.
  void SetTabNeedsAttention(int model_index, bool attention);

  // Retrieves the ideal bounds for the Tab at the specified index.
  /*
  const gfx::Rect& ideal_bounds(int tab_data_index) {
    return tabs_.ideal_bounds(tab_data_index);
  }
  */

  // Returns the NewTabButton.
  /* TODO(brettw) new tab button.
  NewTabButton* new_tab_button() { return new_tab_button_; }
  */

  // Cover method for TabStripController::GetCount.
  // TODO(brettw) remove this.
  int GetModelCount() const;

  // Cover method for TabStripController::IsValidIndex.
  bool IsValidModelIndex(int model_index) const;

  // Returns true if a drag session is currently active.
  bool IsDragSessionActive() const;

  // Returns true if a tab is being dragged into this tab strip.
  bool IsActiveDropTarget() const;

  // Returns the alpha that inactive tabs and the new tab button should use to
  // blend against the frame background.  Inactive tabs and the new tab button
  // differ in whether they change alpha when tab multiselection is occurring;
  // |for_new_tab_button| toggles between the two calculations.
  SkAlpha GetInactiveAlpha(bool for_new_tab_button) const;

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  bool IsAnimating() const;

  // Stops any ongoing animations. If |layout| is true and an animation is
  // ongoing this does a layout.
  void StopAnimating(bool layout);

  // Called to indicate whether the given URL is a supported file.
  void FileSupported(const GURL& url, bool supported);

  // TabStripModelExperimentalObserver implementation:
  void TabInserted(const TabDataExperimental* data, bool is_active) override;
  void TabClosing(const TabDataExperimental* data) override;
  void TabChanged(const TabDataExperimental* data) override;
  void TabSelectionChanged(const TabDataExperimental* old_data,
                           const TabDataExperimental* new_data) override;

  // TabController overrides:
  /*
  const ui::ListSelectionModel& GetSelectionModel() const override;
  bool SupportsMultipleSelection() override;
  bool ShouldHideCloseButtonForInactiveTabs() override;
  bool MaySetClip() override;
  void SelectTab(Tab* tab) override;
  void ExtendSelectionTo(Tab* tab) override;
  void ToggleSelected(Tab* tab) override;
  void AddSelectionFromAnchorTo(Tab* tab) override;
  void CloseTab(Tab* tab, CloseTabSource source) override;
  void ToggleTabAudioMute(Tab* tab) override;
  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::MenuSourceType source_type) override;
  bool IsActiveTab(const Tab* tab) const override;
  bool IsTabSelected(const Tab* tab) const override;
  bool IsTabPinned(const Tab* tab) const override;
  void MaybeStartDrag(
      Tab* tab,
      const ui::LocatedEvent& event,
      const ui::ListSelectionModel& original_selection) override;
  void ContinueDrag(views::View* view, const ui::LocatedEvent& event) override;
  bool EndDrag(EndDragReason reason) override;
  Tab* GetTabAt(Tab* tab, const gfx::Point& tab_in_tab_coordinates) override;
  void OnMouseEventInTab(views::View* source,
                         const ui::MouseEvent& event) override;
  bool ShouldPaintTab(
      const Tab* tab,
      const base::Callback<gfx::Path(const gfx::Size&)>& border_callback,
      gfx::Path* clip) override;
  bool CanPaintThrobberToLayer() const override;
  SkColor GetToolbarTopSeparatorColor() const override;
  base::string16 GetAccessibleTabName(const Tab* tab) const override;
  int GetBackgroundResourceId(bool* custom_image) const override;
  void UpdateTabAccessibilityState(const Tab* tab,
                                   ui::AXNodeData* node_data) override;
                                   */

  // MouseWatcherListener overrides:
  void MouseMovedOutOfHost() override;

  // views::View overrides:
  void Layout() override;
  void PaintChildren(const views::PaintInfo& paint_info) override;
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  // NOTE: the drag and drop methods are invoked from FrameView. This is done
  // to allow for a drop region that extends outside the bounds of the TabStrip.
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;

 private:
  class RemoveTabDelegate;

  friend class TabDragController;
  friend class TabDragControllerTest;
  friend class TabStripTest;
  FRIEND_TEST_ALL_PREFIXES(TabDragControllerTest, GestureEndShouldEndDragTest);
  FRIEND_TEST_ALL_PREFIXES(TabStripTest, TabForEventWhenStacked);
  FRIEND_TEST_ALL_PREFIXES(TabStripTest, TabCloseButtonVisibilityWhenStacked);

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

  void Init();

  int GetActiveIndex() const;

  // Invoked from |AddTabAt| after the newly created tab has been inserted.
  void StartInsertTabAnimation(const TabDataExperimental* data,
                               TabExperimental* tab);

  // Invoked from |MoveTab| after |tab_data_| has been updated to animate the
  // move.
  void StartMoveTabAnimation();

  // Starts the remove tab animation.
  void StartRemoveTabAnimation(TabExperimental* tab);

  // Schedules the animations and bounds changes necessary for a remove tab
  // animation.
  void ScheduleRemoveTabAnimation(TabExperimental* tab);

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

  // Stacks the dragged tabs. This is used if the drag operation is
  // MOVE_VISIBLE_TABS and the tabs don't fill the tabstrip. When this happens
  // the active tab follows the mouse and the other tabs stack around it.
  void StackDraggedTabs(int delta);

  // Returns true if dragging has resulted in temporarily stacking the tabs.
  bool IsStackingDraggedTabs() const;

  // Returns the number of pinned tabs.
  int GetPinnedTabCount() const;

  // Adds the tab at |index| to |tabs_closing_map_| and removes the tab from
  // |tabs_|.
  void RemoveTabFromViewModel(TabExperimental* tab);

  // Cleans up the Tab from the TabStrip. This is called from the tab animation
  // code and is not a general-purpose method.
  void RemoveAndDeleteTab(TabExperimental* tab);

  // Destroys the current TabDragController. This cancel the existing drag
  // operation.
  void DestroyDragController();

  // Releases ownership of the current TabDragController.
  TabDragController* ReleaseDragController();

  // -- Tab Resize Layout -----------------------------------------------------

  // Returns the current width of each tab. If the space for tabs is not evenly
  // divisible into these widths, the initial tabs in the strip will be 1 px
  // larger.
  int current_inactive_width() const { return current_inactive_width_; }
  int current_active_width() const { return current_active_width_; }

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

  // Generates the ideal bounds for the pinned tabs. Returns the index to
  // position the first non-pinned tab and sets |first_non_pinned_index| to the
  // index of the first non-pinned tab.
  int GenerateIdealBoundsForPinnedTabs(int* first_non_pinned_index);

  // Returns the width of the area that contains tabs. This does not include
  // the width of the new tab button.
  int GetTabAreaWidth() const;

  // Starts various types of TabStrip animations.
  void StartResizeLayoutAnimation();
  void StartPinnedTabAnimation();
  void StartMouseInitiatedRemoveTabAnimation(TabExperimental* tab);

  // Returns true if the specified point in TabStrip coords is within the
  // hit-test region of the specified Tab.
  bool IsPointInTab(TabExperimental* tab,
                    const gfx::Point& point_in_tabstrip_coords);

  // -- Touch Layout ----------------------------------------------------------

  // Returns the position normal tabs start at.
  int GetStartXForNormalTabs() const;

  // For a given point, finds a tab that is hit by the point. If the point hits
  // an area on which two tabs are overlapping, the tab is selected as follows:
  // - If one of the tabs is active, select it.
  // - Select the left one.
  // If no tabs are hit, returns null.
  TabExperimental* FindTabHitByPoint(const gfx::Point& point);

  // Returns the x-coordinates of the tabs.
  std::vector<int> GetTabXCoordinates();

  // views::ButtonListener implementation:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // View overrides.
  const views::View* GetViewByID(int id) const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;

  // ui::EventHandler overrides.
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ViewTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;

  // Returns the tab for the given data. The tab might not be owned directly
  // by this class, but could be owned by another tab. This handles "not found"
  // by returning null.
  TabExperimental* TabForData(const TabDataExperimental* data);

  // Invalidates the cached paint order for tabs. Call this whenever the model
  // changes.
  void InvalidateViewOrder();

  // Ensures that view_order_ corresponds to the latest tabs_ structure.
  void EnsureViewOrderUpToDate() const;

  // -- Member Variables ------------------------------------------------------

  TabStripModelExperimental* model_;

  // Active tabs currently in the model. Whenever this changes be sure to call
  // InvalidateViewOrder().
  base::flat_map<const TabDataExperimental*, TabExperimental*> tabs_;

  // Tabs currently in the view that lack a model representation because
  // they're in the process of animating closed.
  // TODO(brettw) order these properly like the TabStripImpl does. Currently
  // this will cause them to be painted in order of their memory address!
  base::flat_set<TabExperimental*> closing_tabs_;

  // Cached ordered vector for painting the tabs. This will include everything
  // in the tabs_ map and also the items in closing_tabs_.
  //
  // This is computed by EnsureViewOrderUpToDate().
  mutable std::vector<TabExperimental*> view_order_;

  // The "New Tab" button.
  /* TODO(brettw) new tab button.
  NewTabButton* new_tab_button_ = nullptr;
  */

  // Ideal bounds of the new tab button.
  gfx::Rect new_tab_button_bounds_;

  // Returns the current widths of each type of tab.  If the tabstrip width is
  // not evenly divisible into these widths, the initial tabs in the strip will
  // be 1 px larger.
  int current_inactive_width_;
  int current_active_width_;

  // If this value is nonnegative, it is used as the width to lay out tabs
  // (instead of tab_area_width()). Most of the time this will be -1, but while
  // we're handling closing a tab via the mouse, we'll set this to the edge of
  // the last tab before closing, so that if we are closing the last tab and
  // need to resize immediately, we'll resize only back to this width, thus
  // once again placing the last tab under the mouse cursor.
  int available_width_for_tabs_ = -1;

  // True if PrepareForCloseAt has been invoked. When true remove animations
  // preserve current tab bounds.
  bool in_tab_close_ = false;

  // Valid for the lifetime of a drag over us.
  std::unique_ptr<DropInfo> drop_info_;

  // To ensure all tabs pulse at the same time they share the same animation
  // container. This is that animation container.
  scoped_refptr<gfx::AnimationContainer> animation_container_;

  // MouseWatcher is used for two things:
  // . When a tab is closed to reset the layout.
  // . When a mouse is used and the layout dynamically adjusts and is currently
  //   stacked (|stacked_layout_| is true).
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;

  // The controller for a drag initiated from a Tab. Valid for the lifetime of
  // the drag session.
  std::unique_ptr<TabDragController> drag_controller_;

  views::BoundsAnimator bounds_animator_;

  // Size we last layed out at.
  gfx::Size last_layout_size_;

  // Location of the mouse at the time of the last move.
  gfx::Point last_mouse_move_location_;

  // Time of the last mouse move event.
  base::TimeTicks last_mouse_move_time_;

  // Number of mouse moves.
  int mouse_move_count_ = 0;

  // Timer used when a tab is closed and we need to relayout. Only used when a
  // tab close comes from a touch device.
  base::OneShotTimer resize_layout_timer_;

  DISALLOW_COPY_AND_ASSIGN(TabStripExperimental);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_EXPERIMENTAL_H_
