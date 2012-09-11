// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_DRAG_GTK_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_DRAG_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/gtk/gtk_signal.h"

class Panel;
class PanelDragDelegate;

// Class for GTK handling of move-drag and resize-drag on a panel.
// Only one type of drag may be active at any time.
// Dragging only begins if the mouse is moved beyond the drag threshold.
// If mouse is released without exceeding the drag threshold, the mouse
// press and release is treated as a mouse click.
class PanelDragGtk {
 public:
  explicit PanelDragGtk(Panel* panel);
  ~PanelDragGtk();

  GtkWidget* widget() const { return drag_widget_; }

  // Sets up mouse and keyboard events processing while the mouse
  // is pressed on a window edge.
  // |event| is the mouse press event.
  // |cursor| is the cursor to display during the drag.
  // |edge| is the window edge from which to resize the panel.
  void InitialWindowEdgeMousePress(GdkEventButton* event, GdkCursor* cursor,
                                   GdkWindowEdge& edge);

  // Sets up mouse and keyboard events processing while the mouse is
  // pressed on the titlebar.
  // |event| is the mouse press event.
  // |titlebar_widget| should handle the mouse release event if the mouse
  // is released without exceeding the drag threshold (a mouse click).
  void InitialTitlebarMousePress(GdkEventButton* event,
                                 GtkWidget* titlebar_widget);

 private:
  friend class GtkNativePanelTesting;

  enum DragState {
    NOT_DRAGGING,
    DRAG_CAN_START,  // mouse pressed
    DRAG_IN_PROGRESS,  // mouse moved beyond drag threshold
    DRAG_ENDED_WAITING_FOR_MOUSE_RELEASE
  };

  // Callbacks for GTK mouse and key events.
  CHROMEGTK_CALLBACK_1(PanelDragGtk, gboolean, OnMouseMoveEvent,
                       GdkEventMotion*);
  CHROMEGTK_CALLBACK_1(PanelDragGtk, gboolean, OnButtonPressEvent,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_1(PanelDragGtk, gboolean, OnButtonReleaseEvent,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_1(PanelDragGtk, gboolean, OnKeyPressEvent,
                       GdkEventKey*);
  CHROMEGTK_CALLBACK_1(PanelDragGtk, gboolean, OnKeyReleaseEvent,
                       GdkEventKey*);
  CHROMEGTK_CALLBACK_1(PanelDragGtk, gboolean, OnGrabBrokenEvent,
                       GdkEventGrabBroken*);

  // Utility to dcheck a bunch of state to ensure a clean slate.
  // Returns only if all the dchecks pass.
  void AssertCleanState();

  void GrabPointerAndKeyboard(GdkEventButton* event,
                              GdkCursor* cursor);
  void ReleasePointerAndKeyboardGrab();

  // Ends any drag that is currently in progress (if any).
  // Resets all drag state except for pointer and keyboard grabs.
  // The grabs are released when the mouse is released to prevent a
  // mouse release *after* the drag has ended (e.g. via ESC key) from
  // being treated as a mouse click.
  void EndDrag(bool canceled);

  // Weak pointer to the panel being dragged.
  Panel* panel_;

  // Invisible event box to receive mouse and key events.
  GtkWidget* drag_widget_;

  DragState drag_state_;

  // A copy of the initial button press event.
  GdkEvent* initial_mouse_down_;

  // Widget that should process the mouse click if mouse is released
  // without exceeding the drag threshold. May be NULL if no click
  // handling is necessary.
  GtkWidget* click_handler_;

  // Delegate for processing drag depends on actual type of drag.
  PanelDragDelegate* drag_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PanelDragGtk);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_DRAG_GTK_H_
