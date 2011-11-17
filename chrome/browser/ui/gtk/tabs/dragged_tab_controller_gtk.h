// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TABS_DRAGGED_TAB_CONTROLLER_GTK_H_
#define CHROME_BROWSER_UI_GTK_TABS_DRAGGED_TAB_CONTROLLER_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/ui/gtk/tabs/drag_data.h"
#include "chrome/browser/ui/tabs/dock_info.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/x/x11_util.h"

class DraggedViewGtk;
class TabGtk;
class TabStripGtk;
class TabStripModel;
class TabContentsWrapper;

class DraggedTabControllerGtk : public content::NotificationObserver,
                                public TabContentsDelegate {
 public:
  // |source_tabstrip| is the tabstrip where the tabs reside before any
  // dragging occurs. |source_tab| is the tab that is under the mouse pointer
  // when dragging starts, it also becomes the active tab if not active
  // already. |tabs| contains all the selected tabs when dragging starts.
  DraggedTabControllerGtk(TabStripGtk* source_tabstrip, TabGtk* source_tab,
                          const std::vector<TabGtk*>& tabs);
  virtual ~DraggedTabControllerGtk();

  // Capture information needed to be used during a drag session for this
  // controller's associated source Tab and TabStrip. |mouse_offset| is the
  // distance of the mouse pointer from the Tab's origin.
  void CaptureDragInfo(const gfx::Point& mouse_offset);

  // Responds to drag events subsequent to StartDrag. If the mouse moves a
  // sufficient distance before the mouse is released, a drag session is
  // initiated.
  void Drag();

  // Complete the current drag session. If the drag session was canceled
  // because the user pressed Escape or something interrupted it, |canceled|
  // is true so the helper can revert the state to the world before the drag
  // begun. Returns whether the tab has been destroyed.
  bool EndDrag(bool canceled);

  // Retrieve the tab that corresponds to |contents| if it is being dragged by
  // this controller, or NULL if |contents| does not correspond to any tab
  // being dragged.
  TabGtk* GetDraggedTabForContents(TabContents* contents);

  // Returns true if |tab| matches any tab being dragged.
  bool IsDraggingTab(const TabGtk* tab);

  // Returns true if |tab_contents| matches any tab contents being dragged.
  bool IsDraggingTabContents(const TabContentsWrapper* tab_contents);

  // Returns true if the specified tab is detached.
  bool IsTabDetached(const TabGtk* tab);

 private:
  // Enumeration of the ways a drag session can end.
  enum EndDragType {
    // Drag session exited normally: the user released the mouse.
    NORMAL,

    // The drag session was canceled (alt-tab during drag, escape ...)
    CANCELED,

    // The tab (NavigationController) was destroyed during the drag.
    TAB_DESTROYED
  };

  DraggedTabData InitDraggedTabData(TabGtk* tab);

  // Overridden from TabContentsDelegate:
  // Deprecated. Please use the two-arguments variant instead.
  // TODO(adriansc): Remove this method once refactoring changed all call sites.
  virtual TabContents* OpenURLFromTab(
      TabContents* source,
      const GURL& url,
      const GURL& referrer,
      WindowOpenDisposition disposition,
      content::PageTransition transition) OVERRIDE;
  virtual TabContents* OpenURLFromTab(TabContents* source,
                                      const OpenURLParams& params) OVERRIDE;
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) OVERRIDE;
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) OVERRIDE;
  virtual void LoadingStateChanged(TabContents* source) OVERRIDE;
  virtual content::JavaScriptDialogCreator*
      GetJavaScriptDialogCreator() OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // Returns the point where a detached window should be created given the
  // current mouse position.
  gfx::Point GetWindowCreatePoint() const;

  // Move the DraggedTabView according to the current mouse screen position,
  // potentially updating the source and other TabStrips.
  void ContinueDragging();

  // Handles dragging tabs while the tabs are attached.
  void MoveAttached(const gfx::Point& screen_point);

  // Handles dragging while the tabs are detached.
  void MoveDetached(const gfx::Point& screen_point);

  // Selects the dragged tabs within |model|.
  void RestoreSelection(TabStripModel* model);

  // Returns the compatible TabStrip that is under the specified point (screen
  // coordinates), or NULL if there is none.
  TabStripGtk* GetTabStripForPoint(const gfx::Point& screen_point);

  // Returns the specified |tabstrip| if it contains the specified point
  // (screen coordinates), NULL if it does not.
  TabStripGtk* GetTabStripIfItContains(TabStripGtk* tabstrip,
                                       const gfx::Point& screen_point) const;

  // Attach the dragged Tab to the specified TabStrip.
  void Attach(TabStripGtk* attached_tabstrip, const gfx::Point& screen_point);

  // Detach the dragged Tab from the current TabStrip.
  void Detach();

  // Converts a screen point to a point relative to the tab strip.
  gfx::Point ConvertScreenPointToTabStripPoint(TabStripGtk* tabstrip,
                                               const gfx::Point& screen_point);

  // Retrieve the bounds of the DraggedTabGtk, relative to the attached
  // TabStrip, given location of the dragged tab in screen coordinates.
  gfx::Rect GetDraggedViewTabStripBounds(const gfx::Point& screen_point);

  // Returns the index where the dragged TabContents should be inserted into
  // the attached TabStripModel given the DraggedTabView's bounds
  // |dragged_bounds| in coordinates relative to the attached TabStrip.
  int GetInsertionIndexForDraggedBounds(const gfx::Rect& dragged_bounds);

  // Get the position of the dragged view relative to the upper left corner of
  // the screen. |screen_point| is the current position of mouse cursor.
  gfx::Point GetDraggedViewPoint(const gfx::Point& screen_point);

  // Finds the Tab within the specified TabStrip that corresponds to the
  // dragged TabContents.
  TabGtk* GetTabMatchingDraggedContents(TabStripGtk* tabstrip,
                                        TabContentsWrapper* contents);

  // Finds all the tabs within the specified TabStrip that correspond to the
  // dragged TabContents.
  std::vector<TabGtk*> GetTabsMatchingDraggedContents(TabStripGtk* tabstrip);

  // Sets the visible and draggging property of all dragged tabs. If |repaint|
  // is true it also schedules a repaint.
  void SetDraggedTabsVisible(bool visible, bool repaint);

  // Does the work for EndDrag. Returns whether the tab has been destroyed.
  bool EndDragImpl(EndDragType how_end);

  // If the drag was aborted for some reason, this function is called to un-do
  // the changes made during the drag operation.
  void RevertDrag();

  // Finishes the drag operation. Returns true if the drag controller should
  // be destroyed immediately, false otherwise.
  bool CompleteDrag();

  // Resets the delegates of the TabContents.
  void ResetDelegates();

  // Create the DraggedViewGtk if it does not yet exist.
  void EnsureDraggedView();

  // Gets the bounds to animate the dragged view when dragging is over.
  gfx::Rect GetAnimateBounds();

  // Utility to convert the specified TabStripModel index to something valid
  // for the attached TabStrip.
  int NormalizeIndexToAttachedTabStrip(int index) const;

  // Hides the window that contains the tab strip the current drag session was
  // initiated from.
  void HideWindow();

  // Presents the window that was hidden by HideWindow.
  void ShowWindow();

  // Closes a hidden frame at the end of a drag session.
  void CleanUpHiddenFrame();

  // Cleans up all the dragged tabs when they are no longer used.
  void CleanUpDraggedTabs();

  // Completes the drag session after the view has animated to its final
  // position.
  void OnAnimateToBoundsComplete();

  // Activates whichever window is under the mouse.
  void BringWindowUnderMouseToFront();

  // Returns true if the tabs were originally one after the other in
  // |source_tabstrip_|.
  bool AreTabsConsecutive();

  // Handles registering for notifications.
  content::NotificationRegistrar registrar_;

  // The tab strip |source_tab_| originated from.
  TabStripGtk* source_tabstrip_;

  // Holds various data for each dragged tab needed to handle dragging.
  scoped_ptr<DragData> drag_data_;

  // The TabStrip the dragged Tab is currently attached to, or NULL if the
  // dragged Tab is detached.
  TabStripGtk* attached_tabstrip_;

  // The visual representation of all the dragged tabs.
  scoped_ptr<DraggedViewGtk> dragged_view_;

  // The position of the mouse (in screen coordinates) at the start of the drag
  // operation. This is used to calculate minimum elasticity before a
  // DraggedTabView is constructed.
  gfx::Point start_screen_point_;

  // This is the offset of the mouse from the top left of the Tab where
  // dragging begun. This is used to ensure that the dragged view is always
  // positioned at the correct location during the drag, and to ensure that the
  // detached window is created at the right location.
  gfx::Point mouse_offset_;

  // Whether we're in the destructor or not.  Makes sure we don't destroy the
  // drag controller more than once.
  bool in_destructor_;

  // The horizontal position of the mouse cursor in screen coordinates at the
  // time of the last re-order event.
  int last_move_screen_x_;

  // True until |MoveAttached| is invoked once.
  bool initial_move_;

  // DockInfo for the tabstrip.
  DockInfo dock_info_;

  typedef std::set<GtkWidget*> DockWindows;
  DockWindows dock_windows_;

  // Timer used to bring the window under the cursor to front. If the user
  // stops moving the mouse for a brief time over a browser window, it is
  // brought to front.
  base::OneShotTimer<DraggedTabControllerGtk> bring_to_front_timer_;

  DISALLOW_COPY_AND_ASSIGN(DraggedTabControllerGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_TABS_DRAGGED_TAB_CONTROLLER_GTK_H_
