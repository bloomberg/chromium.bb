// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/timer.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/dock_info.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_selection_model.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class View;
}
class BaseTab;
class Browser;
class DraggedTabView;
struct TabRendererData;
class TabStrip;
class TabStripModel;
class TabStripSelectionModel;

// TabDragController is responsible for managing the tab dragging session. When
// the user presses the mouse on a tab a new TabDragController is created and
// Drag() is invoked as the mouse is dragged. If the mouse is dragged far enough
// TabDragController starts a drag session. The drag session is completed when
// EndDrag() is invoked (or the TabDragController is destroyed).
//
// While dragging within a tab strip TabDragController sets the bounds of the
// tabs (this is referred to as attached). When the user drags far enough such
// that the tabs should be moved out of the tab strip two possible things
// can happen (this state is referred to as detached):
// . If |detach_into_browser_| is true then a new Browser is created and
//   RunMoveLoop() is invoked on the Widget to drag the browser around. This is
//   the default on chromeos and can be enabled on windows with a flag.
// . If |detach_into_browser_| is false a small representation of the active tab
//   is created and that is dragged around. This mode does not run a nested
//   message loop.
class TabDragController : public content::WebContentsDelegate,
                          public content::NotificationObserver,
                          public MessageLoopForUI::Observer,
                          public views::WidgetObserver,
                          public TabStripModelObserver {
 public:
  enum DetachBehavior {
    DETACHABLE,
    NOT_DETACHABLE
  };

  // What should happen as the mouse is dragged within the tabstrip.
  enum MoveBehavior {
    // Only the set of visible tabs should change. This is only applicable when
    // using touch layout.
    MOVE_VISIBILE_TABS,

    // Typical behavior where tabs are dragged around.
    REORDER
  };

  // Amount above or below the tabstrip the user has to drag before detaching.
  static const int kTouchVerticalDetachMagnetism;
  static const int kVerticalDetachMagnetism;

  TabDragController();
  virtual ~TabDragController();

  // Initializes TabDragController to drag the tabs in |tabs| originating
  // from |source_tabstrip|. |source_tab| is the tab that initiated the drag and
  // is contained in |tabs|.  |mouse_offset| is the distance of the mouse
  // pointer from the origin of the first tab in |tabs| and |source_tab_offset|
  // the offset from |source_tab|. |source_tab_offset| is the horizontal distant
  // for a horizontal tab strip, and the vertical distance for a vertical tab
  // strip. |initial_selection_model| is the selection model before the drag
  // started and is only non-empty if |source_tab| was not initially selected.
  void Init(TabStrip* source_tabstrip,
            BaseTab* source_tab,
            const std::vector<BaseTab*>& tabs,
            const gfx::Point& mouse_offset,
            int source_tab_offset,
            const TabStripSelectionModel& initial_selection_model,
            DetachBehavior detach_behavior,
            MoveBehavior move_behavior);

  // Returns true if there is a drag underway and the drag is attached to
  // |tab_strip|.
  // NOTE: this returns false if the TabDragController is in the process of
  // finishing the drag.
  static bool IsAttachedTo(TabStrip* tab_strip);

  // Returns true if there is a drag underway.
  static bool IsActive();

  // Sets the move behavior. Has no effect if started_drag() is true.
  void SetMoveBehavior(MoveBehavior behavior);

  // See description above fields for details on these.
  bool active() const { return active_; }
  const TabStrip* attached_tabstrip() const { return attached_tabstrip_; }

  // Returns true if a drag started.
  bool started_drag() const { return started_drag_; }

  // Invoked to drag to the new location, in screen coordinates.
  void Drag(const gfx::Point& point_in_screen);

  // Complete the current drag session.
  void EndDrag(EndDragReason reason);

 private:
  class DockDisplayer;
  friend class DockDisplayer;

  typedef std::set<gfx::NativeView> DockWindows;

  // Used to indicate the direction the mouse has moved when attached.
  static const int kMovedMouseLeft  = 1 << 0;
  static const int kMovedMouseRight = 1 << 1;

  // Enumeration of the ways a drag session can end.
  enum EndDragType {
    // Drag session exited normally: the user released the mouse.
    NORMAL,

    // The drag session was canceled (alt-tab during drag, escape ...)
    CANCELED,

    // The tab (NavigationController) was destroyed during the drag.
    TAB_DESTROYED
  };

  // Whether Detach() should release capture or not.
  enum ReleaseCapture {
    RELEASE_CAPTURE,
    DONT_RELEASE_CAPTURE,
  };

  // Specifies what should happen when RunMoveLoop completes.
  enum EndRunLoopBehavior {
    // Indicates the drag should end.
    END_RUN_LOOP_STOP_DRAGGING,

    // Indicates the drag should continue.
    END_RUN_LOOP_CONTINUE_DRAGGING
  };

  // Enumeration of the possible positions the detached tab may detach from.
  enum DetachPosition {
    DETACH_BEFORE,
    DETACH_AFTER,
    DETACH_ABOVE_OR_BELOW
  };

  // Indicates what should happen after invoking DragBrowserToNewTabStrip().
  enum DragBrowserResultType {
    // The caller should return immediately. This return value is used if a
    // nested message loop was created or we're in a nested message loop and
    // need to exit it.
    DRAG_BROWSER_RESULT_STOP,

    // The caller should continue.
    DRAG_BROWSER_RESULT_CONTINUE,
  };

  // Stores the date associated with a single tab that is being dragged.
  struct TabDragData {
    TabDragData();
    ~TabDragData();

    // The TabContents being dragged.
    TabContents* contents;

    // The original content::WebContentsDelegate of |contents|, before it was
    // detached from the browser window. We store this so that we can forward
    // certain delegate notifications back to it if we can't handle them
    // locally.
    content::WebContentsDelegate* original_delegate;

    // This is the index of the tab in |source_tabstrip_| when the drag
    // began. This is used to restore the previous state if the drag is aborted.
    int source_model_index;

    // If attached this is the tab in |attached_tabstrip_|.
    BaseTab* attached_tab;

    // Is the tab pinned?
    bool pinned;
  };

  typedef std::vector<TabDragData> DragData;

  // Sets |drag_data| from |tab|. This also registers for necessary
  // notifications and resets the delegate of the TabContents.
  void InitTabDragData(BaseTab* tab, TabDragData* drag_data);

  // Overridden from content::WebContentsDelegate:
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void NavigationStateChanged(const content::WebContents* source,
                                      unsigned changed_flags) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void LoadingStateChanged(content::WebContents* source) OVERRIDE;
  virtual bool ShouldSuppressDialogs() OVERRIDE;
  virtual content::JavaScriptDialogCreator*
      GetJavaScriptDialogCreator() OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from MessageLoop::Observer:
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE;
  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE;

  // Overriden from views::WidgetObserver:
  virtual void OnWidgetMoved(views::Widget* widget) OVERRIDE;

  // Overriden from TabStripModelObserver:
  virtual void TabStripEmpty() OVERRIDE;

  // Initialize the offset used to calculate the position to create windows
  // in |GetWindowCreatePoint|. This should only be invoked from |Init|.
  void InitWindowCreatePoint();

  // Returns the point where a detached window should be created given the
  // current mouse position |origin|.
  gfx::Point GetWindowCreatePoint(const gfx::Point& origin) const;

  void UpdateDockInfo(const gfx::Point& point_in_screen);

  // Saves focus in the window that the drag initiated from. Focus will be
  // restored appropriately if the drag ends within this same window.
  void SaveFocus();

  // Restore focus to the View that had focus before the drag was started, if
  // the drag ends within the same Window as it began.
  void RestoreFocus();

  // Tests whether |point_in_screen| is past a minimum elasticity threshold
  // required to start a drag.
  bool CanStartDrag(const gfx::Point& point_in_screen) const;

  // Move the DraggedTabView according to the current mouse screen position,
  // potentially updating the source and other TabStrips.
  void ContinueDragging(const gfx::Point& point_in_screen);

  // Transitions dragging from |attached_tabstrip_| to |target_tabstrip|.
  // |target_tabstrip| is NULL if the mouse is not over a valid tab strip.  See
  // DragBrowserResultType for details of the return type.
  DragBrowserResultType DragBrowserToNewTabStrip(
      TabStrip* target_tabstrip,
      const gfx::Point& point_in_screen);

  // Handles dragging for a touch tabstrip when the tabs are stacked. Doesn't
  // actually reorder the tabs in anyway, just changes what's visible.
  void DragActiveTabStacked(const gfx::Point& point_in_screen);

  // Moves the active tab to the next/previous tab. Used when the next/previous
  // tab is stacked.
  void MoveAttachedToNextStackedIndex(const gfx::Point& point_in_screen);
  void MoveAttachedToPreviousStackedIndex(const gfx::Point& point_in_screen);

  // Handles dragging tabs while the tabs are attached.
  void MoveAttached(const gfx::Point& point_in_screen);

  // Handles dragging while the tabs are detached.
  void MoveDetached(const gfx::Point& point_in_screen);

  // If necessary starts the |move_stacked_timer_|. The timer is started if
  // close enough to an edge with stacked tabs.
  void StartMoveStackedTimerIfNecessary(
      const gfx::Point& point_in_screen,
      int delay_ms);

  // Returns the TabStrip for the specified window, or NULL if one doesn't exist
  // or isn't compatible.
  TabStrip* GetTabStripForWindow(gfx::NativeWindow window);

  // Returns the compatible TabStrip to drag to at the specified point (screen
  // coordinates), or NULL if there is none.
  TabStrip* GetTargetTabStripForPoint(const gfx::Point& point_in_screen);

  // Returns true if |tabstrip| contains the specified point in screen
  // coordinates.
  bool DoesTabStripContain(TabStrip* tabstrip,
                           const gfx::Point& point_in_screen) const;

  // Returns the DetachPosition given the specified location in screen
  // coordinates.
  DetachPosition GetDetachPosition(const gfx::Point& point_in_screen);

  DockInfo GetDockInfoAtPoint(const gfx::Point& point_in_screen);

  // Attach the dragged Tab to the specified TabStrip.
  void Attach(TabStrip* attached_tabstrip, const gfx::Point& point_in_screen);

  // Detach the dragged Tab from the current TabStrip.
  void Detach(ReleaseCapture release_capture);

  // Detaches the tabs being dragged, creates a new Browser to contain them and
  // runs a nested move loop.
  void DetachIntoNewBrowserAndRunMoveLoop(const gfx::Point& point_in_screen);

  // Runs a nested message loop that handles moving the current
  // Browser. |drag_offset| is the offset from the window origin and is used in
  // calculating the location of the window offset from the cursor while
  // dragging.
  void RunMoveLoop(const gfx::Point& drag_offset);

  // Determines the index to insert tabs at. |dragged_bounds| is the bounds of
  // the tabs being dragged, |start| the index of the tab to start looking from
  // and |delta| the amount to increment (1 or -1).
  int GetInsertionIndexFrom(const gfx::Rect& dragged_bounds,
                            int start,
                            int delta) const;

  // Returns the index where the dragged WebContents should be inserted into
  // |attached_tabstrip_| given the DraggedTabView's bounds |dragged_bounds| in
  // coordinates relative to |attached_tabstrip_| and has had the mirroring
  // transformation applied.
  // NOTE: this is invoked from |Attach| before the tabs have been inserted.
  int GetInsertionIndexForDraggedBounds(const gfx::Rect& dragged_bounds) const;

  // Returns true if |dragged_bounds| is close enough to the next stacked tab
  // so that the active tab should be dragged there.
  bool ShouldDragToNextStackedTab(const gfx::Rect& dragged_bounds,
                                  int index) const;

  // Returns true if |dragged_bounds| is close enough to the previous stacked
  // tab so that the active tab should be dragged there.
  bool ShouldDragToPreviousStackedTab(const gfx::Rect& dragged_bounds,
                                      int index) const;

  // Used by GetInsertionIndexForDraggedBounds() when the tabstrip is stacked.
  int GetInsertionIndexForDraggedBoundsStacked(
      const gfx::Rect& dragged_bounds) const;

  // Retrieve the bounds of the DraggedTabView relative to the attached
  // TabStrip. |tab_strip_point| is in the attached TabStrip's coordinate
  // system.
  gfx::Rect GetDraggedViewTabStripBounds(const gfx::Point& tab_strip_point);

  // Get the position of the dragged tab view relative to the attached tab
  // strip with the mirroring transform applied.
  gfx::Point GetAttachedDragPoint(const gfx::Point& point_in_screen);

  // Finds the Tabs within the specified TabStrip that corresponds to the
  // WebContents of the dragged tabs. Returns an empty vector if not attached.
  std::vector<BaseTab*> GetTabsMatchingDraggedContents(TabStrip* tabstrip);

  // Returns the bounds for the tabs based on the attached tab strip. The
  // x-coordinate of each tab is offset by |x_offset|.
  std::vector<gfx::Rect> CalculateBoundsForDraggedTabs(int x_offset);

  // Does the work for EndDrag. If we actually started a drag and |how_end| is
  // not TAB_DESTROYED then one of EndDrag or RevertDrag is invoked.
  void EndDragImpl(EndDragType how_end);

  // Reverts a cancelled drag operation.
  void RevertDrag();

  // Reverts the tab at |drag_index| in |drag_data_|.
  void RevertDragAt(size_t drag_index);

  // Selects the dragged tabs in |model|. Does nothing if there are no longer
  // any dragged contents (as happens when a WebContents is deleted out from
  // under us).
  void ResetSelection(TabStripModel* model);

  // Finishes a succesful drag operation.
  void CompleteDrag();

  // Resets the delegates of the WebContents.
  void ResetDelegates();

  // Create the DraggedTabView.
  void CreateDraggedView(const std::vector<TabRendererData>& data,
                         const std::vector<gfx::Rect>& renderer_bounds);

  // Returns the bounds (in screen coordinates) of the specified View.
  gfx::Rect GetViewScreenBounds(views::View* tabstrip) const;

  // Hides the frame for the window that contains the TabStrip the current
  // drag session was initiated from.
  void HideFrame();

  // Closes a hidden frame at the end of a drag session.
  void CleanUpHiddenFrame();

  void DockDisplayerDestroyed(DockDisplayer* controller);

  void BringWindowUnderPointToFront(const gfx::Point& point_in_screen);

  // Convenience for getting the TabDragData corresponding to the tab the user
  // started dragging.
  TabDragData* source_tab_drag_data() {
    return &(drag_data_[source_tab_index_]);
  }

  // Convenience for |source_tab_drag_data()->contents|.
  TabContents* source_dragged_contents() {
    return source_tab_drag_data()->contents;
  }

  // Returns the Widget of the currently attached TabStrip's BrowserView.
  views::Widget* GetAttachedBrowserWidget();

  // Returns true if the tabs were originality one after the other in
  // |source_tabstrip_|.
  bool AreTabsConsecutive();

  // Creates and returns a new Browser to handle the drag.
  Browser* CreateBrowserForDrag(TabStrip* source,
                                const gfx::Point& point_in_screen,
                                gfx::Point* drag_offset,
                                std::vector<gfx::Rect>* drag_bounds);

  // Returns the TabStripModel for the specified tabstrip.
  TabStripModel* GetModel(TabStrip* tabstrip) const;

  // Returns the location of the cursor. This is either the location of the
  // mouse or the location of the current touch point.
  gfx::Point GetCursorScreenPoint();

  // Returns the offset from the top left corner of the window to
  // |point_in_screen|.
  gfx::Point GetWindowOffset(const gfx::Point& point_in_screen);

  // Returns true if moving the mouse only changes the visible tabs.
  bool move_only() const {
    return (move_behavior_ == MOVE_VISIBILE_TABS) != 0;
  }

  // If true Detaching creates a new browser and enters a nested message loop.
  const bool detach_into_browser_;

  // Handles registering for notifications.
  content::NotificationRegistrar registrar_;

  // The TabStrip the drag originated from.
  TabStrip* source_tabstrip_;

  // The TabStrip the dragged Tab is currently attached to, or NULL if the
  // dragged Tab is detached.
  TabStrip* attached_tabstrip_;

  // The visual representation of the dragged Tab.
  scoped_ptr<DraggedTabView> view_;

  // The position of the mouse (in screen coordinates) at the start of the drag
  // operation. This is used to calculate minimum elasticity before a
  // DraggedTabView is constructed.
  gfx::Point start_point_in_screen_;

  // This is the offset of the mouse from the top left of the Tab where
  // dragging begun. This is used to ensure that the dragged view is always
  // positioned at the correct location during the drag, and to ensure that the
  // detached window is created at the right location.
  gfx::Point mouse_offset_;

  // Offset of the mouse relative to the source tab.
  int source_tab_offset_;

  // Ratio of the x-coordinate of the |source_tab_offset_| to the width of the
  // tab. Not used for vertical tabs.
  float offset_to_width_ratio_;

  // A hint to use when positioning new windows created by detaching Tabs. This
  // is the distance of the mouse from the top left of the dragged tab as if it
  // were the distance of the mouse from the top left of the first tab in the
  // attached TabStrip from the top left of the window.
  gfx::Point window_create_point_;

  // Location of the first tab in the source tabstrip in screen coordinates.
  // This is used to calculate window_create_point_.
  gfx::Point first_source_tab_point_;

  // The bounds of the browser window before the last Tab was detached. When
  // the last Tab is detached, rather than destroying the frame (which would
  // abort the drag session), the frame is moved off-screen. If the drag is
  // aborted (e.g. by the user pressing Esc, or capture being lost), the Tab is
  // attached to the hidden frame and the frame moved back to these bounds.
  gfx::Rect restore_bounds_;

  // The last view that had focus in the window containing |source_tab_|. This
  // is saved so that focus can be restored properly when a drag begins and
  // ends within this same window.
  views::View* old_focused_view_;

  // The position along the major axis of the mouse cursor in screen coordinates
  // at the time of the last re-order event.
  int last_move_screen_loc_;

  DockInfo dock_info_;

  DockWindows dock_windows_;

  std::vector<DockDisplayer*> dock_controllers_;

  // Timer used to bring the window under the cursor to front. If the user
  // stops moving the mouse for a brief time over a browser window, it is
  // brought to front.
  base::OneShotTimer<TabDragController> bring_to_front_timer_;

  // Timer used to move the stacked tabs. See comment aboue
  // StartMoveStackedTimerIfNecessary().
  base::OneShotTimer<TabDragController> move_stacked_timer_;

  // Did the mouse move enough that we started a drag?
  bool started_drag_;

  // Is the drag active?
  bool active_;

  DragData drag_data_;

  // Index of the source tab in drag_data_.
  size_t source_tab_index_;

  // True until |MoveAttached| is invoked once.
  bool initial_move_;

  // The selection model before the drag started. See comment above Init() for
  // details.
  TabStripSelectionModel initial_selection_model_;

  // The selection model of |attached_tabstrip_| before the tabs were attached.
  TabStripSelectionModel selection_model_before_attach_;

  // Initial x-coordinates of the tabs when the drag started. Only used for
  // touch mode.
  std::vector<int> initial_tab_positions_;

  DetachBehavior detach_behavior_;
  MoveBehavior move_behavior_;

  // Updated as the mouse is moved when attached. Indicates whether the mouse
  // has ever moved to the left or right. If the tabs are ever detached this
  // is set to kMovedMouseRight | kMovedMouseLeft.
  int mouse_move_direction_;

  // Last location used in screen coordinates.
  gfx::Point last_point_in_screen_;

  // The following are needed when detaching into a browser
  // (|detach_into_browser_| is true).

  // Set to true if we've detached from a tabstrip and are running a nested
  // move message loop.
  bool is_dragging_window_;

  EndRunLoopBehavior end_run_loop_behavior_;

  // If true, we're waiting for a move loop to complete.
  bool waiting_for_run_loop_to_exit_;

  // The TabStrip to attach to after the move loop completes.
  TabStrip* tab_strip_to_attach_to_after_exit_;

  // Non-null for the duration of RunMoveLoop.
  views::Widget* move_loop_widget_;

  // If non-null set to true from destructor.
  bool* destroyed_;

  DISALLOW_COPY_AND_ASSIGN(TabDragController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_H_
