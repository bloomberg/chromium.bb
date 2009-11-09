// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TABS_TAB_STRIP_GTK_H_
#define CHROME_BROWSER_GTK_TABS_TAB_STRIP_GTK_H_

#include <gtk/gtk.h>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/task.h"
#include "base/message_loop.h"
#include "chrome/browser/gtk/tabstrip_origin_provider.h"
#include "chrome/browser/gtk/tabs/tab_gtk.h"
#include "chrome/browser/gtk/view_id_util.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/owned_widget_gtk.h"

class BrowserWindowGtk;
class CustomDrawButton;
class DraggedTabControllerGtk;
class GtkThemeProvider;

class TabStripGtk : public TabStripModelObserver,
                    public TabGtk::TabDelegate,
                    public MessageLoopForUI::Observer,
                    public NotificationObserver,
                    public TabstripOriginProvider,
                    public ViewIDUtil::Delegate {
 public:
  class TabAnimation;

  TabStripGtk(TabStripModel* model, BrowserWindowGtk* window);
  virtual ~TabStripGtk();

  // Initialize and load the TabStrip into a container.
  // TODO(tc): Pass in theme provider so we can properly theme the tabs.
  void Init();

  void Show();
  void Hide();

  TabStripModel* model() const { return model_; }

  BrowserWindowGtk* window() const { return window_; }

  GtkWidget* widget() const { return tabstrip_.get(); }

  // Returns true if there is an active drag session.
  bool IsDragSessionActive() const { return drag_controller_.get() != NULL; }

  // Sets the bounds of the tabs.
  void Layout();

  // Queues a draw for the tabstrip widget.
  void SchedulePaint();

  // Sets the bounds of the tabstrip.
  void SetBounds(const gfx::Rect& bounds);

  // Returns the bounds of the tabstrip.
  const gfx::Rect& bounds() const { return bounds_; }

  // Updates loading animations for the TabStrip.
  void UpdateLoadingAnimations();

  // Return true if this tab strip is compatible with the provided tab strip.
  // Compatible tab strips can transfer tabs during drag and drop.
  bool IsCompatibleWith(TabStripGtk* other);

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  bool IsAnimating() const;

  // Destroys the active drag controller.
  void DestroyDragController();

  // Removes the drag source tab from this tabstrip, and deletes it.
  void DestroyDraggedSourceTab(TabGtk* tab);

  // Retrieve the ideal bounds for the Tab at the specified index.
  gfx::Rect GetIdealBounds(int index);

  // TabstripOriginProvider implementation -------------------------------------
  virtual gfx::Point GetTabStripOriginForWidget(GtkWidget* widget);

  // ViewIDUtil::Delegate implementation ---------------------------------------
  virtual GtkWidget* GetWidgetForViewID(ViewID id);

 protected:
  // TabStripModelObserver implementation:
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContents* contents,
                        int from_index,
                        int to_index,
                        bool pinned_state_changed);
  virtual void TabChangedAt(TabContents* contents, int index,
                            TabChangeType change_type);
  virtual void TabPinnedStateChanged(TabContents* contents, int index);

  // TabGtk::TabDelegate implementation:
  virtual bool IsTabSelected(const TabGtk* tab) const;
  virtual bool IsTabDetached(const TabGtk* tab) const;
  virtual void SelectTab(TabGtk* tab);
  virtual void CloseTab(TabGtk* tab);
  virtual bool IsCommandEnabledForTab(
      TabStripModel::ContextMenuCommand command_id, const TabGtk* tab) const;
  virtual void ExecuteCommandForTab(
      TabStripModel::ContextMenuCommand command_id, TabGtk* tab);
  virtual void StartHighlightTabsForCommand(
      TabStripModel::ContextMenuCommand command_id, TabGtk* tab);
  virtual void StopHighlightTabsForCommand(
      TabStripModel::ContextMenuCommand command_id, TabGtk* tab);
  virtual void StopAllHighlighting();
  virtual void MaybeStartDrag(TabGtk* tab, const gfx::Point& point);
  virtual void ContinueDrag(GdkDragContext* context);
  virtual bool EndDrag(bool canceled);
  virtual bool HasAvailableDragActions() const;
  virtual ThemeProvider* GetThemeProvider();

  // MessageLoop::Observer implementation:
  virtual void WillProcessEvent(GdkEvent* event);
  virtual void DidProcessEvent(GdkEvent* event);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Horizontal gap between pinned and non-pinned tabs.
  static const int pinned_to_non_pinned_gap_;

 private:
  friend class DraggedTabControllerGtk;
  friend class InsertTabAnimation;
  friend class RemoveTabAnimation;
  friend class MoveTabAnimation;
  friend class PinAndMoveAnimation;
  friend class PinnedTabAnimation;
  friend class ResizeLayoutAnimation;
  friend class TabAnimation;

  struct TabData {
    TabGtk* tab;
    gfx::Rect ideal_bounds;
  };

  // Used during a drop session of a url. Tracks the position of the drop as
  // well as a window used to highlight where the drop occurs.
  class DropInfo {
   public:
    DropInfo(int index, bool drop_before, bool point_down);
    ~DropInfo();

    // TODO(jhawkins): Factor out this code into a TransparentContainer class.

    // expose-event handler that redraws the drop indicator.
    static gboolean OnExposeEvent(GtkWidget* widget, GdkEventExpose* event,
                                  DropInfo* drop_info);

    // Sets the color map of the container window to allow the window to be
    // transparent.
    void SetContainerColorMap();

    // Sets full transparency for the container window.  This is used if
    // compositing is available for the screen.
    void SetContainerTransparency();

    // Sets the shape mask for the container window to emulate a transparent
    // container window.  This is used if compositing is not available for the
    // screen.
    void SetContainerShapeMask();

    // Creates the container widget.
    void CreateContainer();

    // Destroys the container widget.
    void DestroyContainer();

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

    // Transparent container window used to render the drop indicator over the
    // tabstrip and toolbar.
    GtkWidget* container;

    // The drop indicator image.
    GdkPixbuf* drop_arrow;

   private:
    DISALLOW_COPY_AND_ASSIGN(DropInfo);
  };

  // expose-event handler that redraws the tabstrip
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           TabStripGtk* tabstrip);

  // size-allocate handler that gets the new bounds of the tabstrip.
  static void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation,
                             TabStripGtk* tabstrip);

  // drag-motion handler that is signaled when the user performs a drag in the
  // tabstrip bounds.
  static gboolean OnDragMotion(GtkWidget* widget, GdkDragContext* context,
                               gint x, gint y, guint time,
                               TabStripGtk* tabstrip);

  // drag-drop handler that is notified when the user finishes a drag.
  static gboolean OnDragDrop(GtkWidget* widget, GdkDragContext* context,
                             gint x, gint y, guint time,
                             TabStripGtk* tabstrip);

  // drag-leave handler that is signaled when the mouse leaves the tabstrip
  // during a drag.
  static gboolean OnDragLeave(GtkWidget* widget, GdkDragContext* context,
                              guint time, TabStripGtk* tabstrip);

  // drag-failed handler that is signaled when the drag fails or is canceled.
  static gboolean OnDragFailed(GtkWidget* widget, GdkDragContext* context,
                               GtkDragResult result, TabStripGtk* tabstrip);

  // drag-data-received handler that receives the data associated with the drag.
  static gboolean OnDragDataReceived(GtkWidget* widget, GdkDragContext* context,
                                     gint x, gint y, GtkSelectionData* data,
                                     guint info, guint time,
                                     TabStripGtk* tabstrip);

  // Handles the clicked signal from the new tab button.
  static void OnNewTabClicked(GtkWidget* widget, TabStripGtk* tabstrip);

  // Sets the bounds of the tab and moves the tab widget to those bounds.
  void SetTabBounds(TabGtk* tab, const gfx::Rect& bounds);

  // Returns true if |rects| are all areas that match up with tab favicons.
  // |rects| must be sorted from left to right.  |tabs_to_paint| are the tab
  // positions that match the rects.
  bool CanPaintOnlyFavIcons(const GdkRectangle* rects,
                            int num_rects,
                            std::vector<int>* tabs_to_paint);

  // Paints the tab favicon areas for tabs in |tabs_to_paint|.
  void PaintOnlyFavIcons(GdkEventExpose* event,
                         const std::vector<int>& tabs_to_paint);

  // Initializes the new tab button.
  CustomDrawButton* MakeNewTabButton();

  // Gets the number of Tabs in the collection.
  int GetTabCount() const;

  // Returns the number of pinned tabs.
  int GetPinnedTabCount() const;

  // Retrieves the Tab at the specified index. Take care in using this, you may
  // need to use GetTabAtAdjustForAnimation.
  TabGtk* GetTabAt(int index) const;

  // Returns the tab at the specified index. If a remove animation is on going
  // and the index is >= the index of the tab being removed, the index is
  // incremented. While a remove operation is on going the indices of the model
  // do not line up with the indices of the view. This method adjusts the index
  // accordingly.
  //
  // Use this instead of GetTabAt if the index comes from the model.
  TabGtk* GetTabAtAdjustForAnimation(int index) const;

  // Returns the exact (unrounded) current width of each tab.
  void GetCurrentTabWidths(double* unselected_width,
                           double* selected_width) const;

  // Returns the exact (unrounded) desired width of each tab, based on the
  // desired strip width and number of tabs.  If
  // |width_of_tabs_for_mouse_close_| is nonnegative we use that value in
  // calculating the desired strip width; otherwise we use the current width.
  // |pinned_tab_count| gives the number of pinned tabs, and |tab_count| the
  // number of pinned and non-pinned tabs.
  void GetDesiredTabWidths(int tab_count,
                           int pinned_tab_count,
                           double* unselected_width,
                           double* selected_width) const;

  // Returns the horizontal offset before the tab at |tab_index|.
  int GetTabHOffset(int tab_index);

  // Returns the x-coordinate tabs start from.
  int tab_start_x() const;

  // Perform an animated resize-relayout of the TabStrip immediately.
  void ResizeLayoutTabs();

  // Returns whether or not the cursor is currently in the "tab strip zone"
  // which is defined as the region above the TabStrip and a bit below it.
  bool IsCursorInTabStripZone() const;

  // Ensure that the message loop observer used for event spying is added and
  // removed appropriately so we can tell when to resize layout the tab strip.
  void AddMessageLoopObserver();
  void RemoveMessageLoopObserver();

  // Calculates the available width for tabs, assuming a Tab is to be closed.
  int GetAvailableWidthForTabs(TabGtk* last_tab) const;

  // Finds the index of the TabContents corresponding to |tab| in our
  // associated TabStripModel, or -1 if there is none (e.g. the specified |tab|
  // is being animated closed).
  int GetIndexOfTab(const TabGtk* tab) const;

  // Cleans up the tab from the TabStrip at the specified |index|.
  void RemoveTabAt(int index);

  // Called from the message loop observer when a mouse movement has occurred
  // anywhere over our containing window.
  void HandleGlobalMouseMoveEvent();

  // Generates the ideal bounds of the TabStrip when all Tabs have finished
  // animating to their desired position/bounds. This is used by the standard
  // Layout method and other callers like the DraggedTabController that need
  // stable representations of Tab positions.
  void GenerateIdealBounds();

  // Lays out the New Tab button, assuming the right edge of the last Tab on
  // the TabStrip at |last_tab_right|.  |unselected_width| is the width of
  // unselected tabs at the moment this function is called.  The value changes
  // during animations, so we can't use current_unselected_width_.
  void LayoutNewTabButton(double last_tab_right, double unselected_width);

  // -- Link Drag & Drop ------------------------------------------------------

  // Returns the bounds to render the drop at, in screen coordinates. Sets
  // |is_beneath| to indicate whether the arrow is beneath the tab, or above
  // it.
  gfx::Rect GetDropBounds(int drop_index, bool drop_before, bool* is_beneath);

  // Updates the location of the drop based on the event.
  void UpdateDropIndex(GdkDragContext* context, gint x, gint y);

  // Sets the location of the drop, repainting as necessary.
  void SetDropIndex(int index, bool drop_before);

  // Determines whether the data is acceptable by the tabstrip and opens a new
  // tab with the data as URL if it is.  Returns true if the drop was
  // successful.
  bool CompleteDrop(guchar* data);

  // Returns the image to use for indicating a drop on a tab. If is_down is
  // true, this returns an arrow pointing down.
  static GdkPixbuf* GetDropArrowImage(bool is_down);

  // -- Animations -------------------------------------------------------------

  // A generic Layout method for various classes of TabStrip animations,
  // including Insert, Remove and Resize Layout cases.
  void AnimationLayout(double unselected_width);

  // Starts various types of TabStrip animations.
  void StartInsertTabAnimation(int index);
  void StartRemoveTabAnimation(int index, TabContents* contents);
  void StartMoveTabAnimation(int from_index, int to_index);
  void StartResizeLayoutAnimation();
  void StartPinnedTabAnimation(int index);
  void StartPinAndMoveTabAnimation(int from_index, int to_index,
                                   const gfx::Rect& start_bounds);

  // Returns true if detach or select changes in the model should be reflected
  // in the TabStrip. This returns false if we're closing all tabs in the
  // TabStrip and so we should prevent updating. This is not const because we
  // use this as a signal to cancel any active animations.
  bool CanUpdateDisplay();

  // Notifies the TabStrip that the specified TabAnimation has completed.
  // Optionally a full Layout will be performed, specified by |layout|.
  void FinishAnimation(TabAnimation* animation, bool layout);

  NotificationRegistrar registrar_;

  // The Tabs we contain, and their last generated "good" bounds.
  std::vector<TabData> tab_data_;

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

  // True if a resize layout animation should be run a short delay after the
  // mouse exits the TabStrip.
  // TODO(beng): (Cleanup) this would be better named "needs_resize_layout_".
  bool resize_layout_scheduled_;

  // The GtkFixed widget.
  OwnedWidgetGtk tabstrip_;

  // The bounds of the tabstrip.
  gfx::Rect bounds_;

  // Our model.
  TabStripModel* model_;

  // The BrowserWindowGtk containing this tab strip.
  BrowserWindowGtk* window_;

  // Theme resources.
  GtkThemeProvider* theme_provider_;

  // The currently running animation.
  scoped_ptr<TabAnimation> active_animation_;

  // The New Tab button.
  scoped_ptr<CustomDrawButton> newtab_button_;

  // Valid for the lifetime of a drag over us.
  scoped_ptr<DropInfo> drop_info_;

  // The controller for a drag initiated from a Tab. Valid for the lifetime of
  // the drag session.
  scoped_ptr<DraggedTabControllerGtk> drag_controller_;

  // A factory that is used to construct a delayed callback to the
  // ResizeLayoutTabsNow method.
  ScopedRunnableMethodFactory<TabStripGtk> resize_layout_factory_;

  // True if the tabstrip has already been added as a MessageLoop observer.
  bool added_as_message_loop_observer_;

  DISALLOW_COPY_AND_ASSIGN(TabStripGtk);
};

#endif  // CHROME_BROWSER_GTK_TABS_TAB_STRIP_GTK_H_
