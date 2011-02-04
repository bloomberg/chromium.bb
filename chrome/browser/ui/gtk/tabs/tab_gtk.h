// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TABS_TAB_GTK_H_
#define CHROME_BROWSER_UI_GTK_TABS_TAB_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/gtk/tabs/tab_renderer_gtk.h"
#include "ui/base/gtk/gtk_signal.h"

namespace gfx {
class Path;
}

namespace ui {
class ThemeProvider;
}

class TabGtk : public TabRendererGtk,
               public MessageLoopForUI::Observer {
 public:
  // An interface implemented by an object that can help this Tab complete
  // various actions. The index parameter is the index of this Tab in the
  // TabRenderer::Model.
  class TabDelegate {
   public:
    // Returns true if the specified Tab is selected.
    virtual bool IsTabSelected(const TabGtk* tab) const = 0;

    // Returns true if the specified Tab is pinned.
    virtual bool IsTabPinned(const TabGtk* tab) const = 0;

    // Returns true if the specified Tab is detached.
    virtual bool IsTabDetached(const TabGtk* tab) const = 0;

    // Selects the specified Tab.
    virtual void SelectTab(TabGtk* tab) = 0;

    // Closes the specified Tab.
    virtual void CloseTab(TabGtk* tab) = 0;

    // Returns true if the specified command is enabled for the specified Tab.
    virtual bool IsCommandEnabledForTab(
        TabStripModel::ContextMenuCommand command_id,
        const TabGtk* tab) const = 0;

    // Executes the specified command for the specified Tab.
    virtual void ExecuteCommandForTab(
        TabStripModel::ContextMenuCommand command_id, TabGtk* tab) = 0;

    // Starts/Stops highlighting the tabs that will be affected by the
    // specified command for the specified Tab.
    virtual void StartHighlightTabsForCommand(
        TabStripModel::ContextMenuCommand command_id, TabGtk* tab) = 0;
    virtual void StopHighlightTabsForCommand(
        TabStripModel::ContextMenuCommand command_id, TabGtk* tab) = 0;
    virtual void StopAllHighlighting() = 0;

    // Potentially starts a drag for the specified Tab.
    virtual void MaybeStartDrag(TabGtk* tab, const gfx::Point& point) = 0;

    // Continues dragging a Tab.
    virtual void ContinueDrag(GdkDragContext* context) = 0;

    // Ends dragging a Tab. |canceled| is true if the drag was aborted in a way
    // other than the user releasing the mouse. Returns whether the tab has been
    // destroyed.
    virtual bool EndDrag(bool canceled) = 0;

    // Returns true if the associated TabStrip's delegate supports tab moving or
    // detaching. Used by the Frame to determine if dragging on the Tab
    // itself should move the window in cases where there's only one
    // non drag-able Tab.
    virtual bool HasAvailableDragActions() const = 0;

    // Returns the theme provider for icons and colors.
    virtual ui::ThemeProvider* GetThemeProvider() = 0;

   protected:
    virtual ~TabDelegate() {}
  };

  explicit TabGtk(TabDelegate* delegate);
  virtual ~TabGtk();

  // Access the delegate.
  TabDelegate* delegate() const { return delegate_; }

  GtkWidget* widget() const { return event_box_; }

  // Used to set/check whether this Tab is being animated closed.
  void set_closing(bool closing) { closing_ = closing; }
  bool closing() const { return closing_; }

  // Used to set/check whether this Tab is being dragged.
  void set_dragging(bool dragging) { dragging_ = dragging; }
  bool dragging() const { return dragging_; }

  // TabRendererGtk overrides:
  virtual bool IsSelected() const;
  virtual bool IsVisible() const;
  virtual void SetVisible(bool visible) const;
  virtual void CloseButtonClicked();
  virtual void UpdateData(TabContents* contents, bool app, bool loading_only);
  virtual void SetBounds(const gfx::Rect& bounds);

 private:
  class ContextMenuController;
  class TabGtkObserverHelper;
  friend class ContextMenuController;

  // MessageLoop::Observer implementation:
  virtual void WillProcessEvent(GdkEvent* event);
  virtual void DidProcessEvent(GdkEvent* event);

  // button-press-event handler that handles mouse clicks.
  CHROMEGTK_CALLBACK_1(TabGtk, gboolean, OnButtonPressEvent, GdkEventButton*);

  // button-release-event handler that handles mouse click releases.
  CHROMEGTK_CALLBACK_1(TabGtk, gboolean, OnButtonReleaseEvent, GdkEventButton*);

  // drag-begin is emitted when the drag is started. We connect so that we can
  // set the drag icon to a transparent pixbuf.
  CHROMEGTK_CALLBACK_1(TabGtk, void, OnDragBegin, GdkDragContext*);

  // drag-failed is emitted when the drag is finished.  In our case the signal
  // does not imply failure as we don't use the drag-n-drop API to transfer drop
  // data.
  CHROMEGTK_CALLBACK_2(TabGtk, gboolean, OnDragFailed, GdkDragContext*,
                       GtkDragResult);

  // When a drag is ending, a fake button release event is passed to the drag
  // widget to fake letting go of the mouse button.  We need a callback for
  // this event because it is the only way to catch drag end events when the
  // user presses space or return.
  CHROMEGTK_CALLBACK_1(TabGtk, gboolean, OnDragButtonReleased, GdkEventButton*);

  // Invoked when the context menu closes.
  void ContextMenuClosed();

  // Sets whether the tooltip should be shown or not, depending on the size of
  // the tab.
  void UpdateTooltipState();

  // Creates the drag widget used to track a drag operation.
  void CreateDragWidget();

  // Destroys the drag widget.
  void DestroyDragWidget();

  // Starts the dragging operation.  |drag_offset| is the offset inside the tab
  // bounds where the grab occurred.
  void StartDragging(gfx::Point drag_offset);

  // Ends the dragging operations.  |canceled| is true if the operation was
  // canceled.
  void EndDrag(bool canceled);

  // An instance of a delegate object that can perform various actions based on
  // user gestures.
  TabDelegate* delegate_;

  // True if the tab is being animated closed.
  bool closing_;

  // True if the tab is being dragged.
  bool dragging_;

  // The context menu controller.
  scoped_ptr<ContextMenuController> menu_controller_;

  // The windowless widget used to collect input events for the tab.  We can't
  // use an OwnedWidgetGtk because of the way the dragged tab controller
  // destroys the source tab.  The source tab is destroyed when the drag ends
  // before we let gtk handle the end of the drag.  This results in the widget
  // having an extra reference, which will cause OwnedWidgetGtk.Destroy to
  // DCHECK.
  GtkWidget* event_box_;

  // A copy of the last button press event, used to initiate a drag.
  GdkEvent* last_mouse_down_;

  // A GtkInivisible used to track the drag event.  GtkInvisibles are of the
  // type GInitiallyUnowned, but the widget initialization code sinks the
  // reference, so we can't used an OwnedWidgetGtk here.
  GtkWidget* drag_widget_;

  // The cached width of the title in pixels, updated whenever the title
  // changes.
  int title_width_;

  // Keep track of whether or not we have an observer.
  scoped_ptr<TabGtkObserverHelper> observer_;

  // Used to destroy the drag widget after a return to the message loop.
  ScopedRunnableMethodFactory<TabGtk> destroy_factory_;

  // Due to a bug in GTK+, we need to force the end of a drag when we get a
  // mouse release event on the the dragged widget, otherwise, we don't know
  // when the drag has ended when the user presses space or enter.  We queue
  // a task to end the drag and only run it if GTK+ didn't send us the
  // drag-failed event.
  ScopedRunnableMethodFactory<TabGtk> drag_end_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_TABS_TAB_GTK_H_
