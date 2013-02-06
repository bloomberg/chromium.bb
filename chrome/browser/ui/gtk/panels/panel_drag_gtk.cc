// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/panels/panel_drag_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "ui/gfx/gtk_util.h"

namespace {

panel::ResizingSides GdkWindowEdgeToResizingSide(GdkWindowEdge edge) {
  switch (edge) {
  case GDK_WINDOW_EDGE_NORTH_WEST:
    return panel::RESIZE_TOP_LEFT;
  case GDK_WINDOW_EDGE_NORTH:
    return panel::RESIZE_TOP;
  case GDK_WINDOW_EDGE_NORTH_EAST:
    return panel::RESIZE_TOP_RIGHT;
  case GDK_WINDOW_EDGE_WEST:
    return panel::RESIZE_LEFT;
  case GDK_WINDOW_EDGE_EAST:
    return panel::RESIZE_RIGHT;
  case GDK_WINDOW_EDGE_SOUTH_WEST:
    return panel::RESIZE_BOTTOM_LEFT;
  case GDK_WINDOW_EDGE_SOUTH:
    return panel::RESIZE_BOTTOM;
  case GDK_WINDOW_EDGE_SOUTH_EAST:
    return panel::RESIZE_BOTTOM_RIGHT;
  default:
    return panel::RESIZE_NONE;
  }
}

}  // namespace

// Virtual base class to abstract move vs resize drag logic.
class PanelDragDelegate {
 public:
  explicit PanelDragDelegate(Panel* panel) : panel_(panel) {}
  virtual ~PanelDragDelegate() {}

  Panel* panel() const { return panel_; }
  PanelManager* panel_manager() const { return panel_->manager(); }

  // |point| is the mouse location in screen coordinates.
  virtual void DragStarted(gfx::Point point) = 0;
  virtual void Dragged(gfx::Point point) = 0;

  // |canceled| is true to abort the drag.
  virtual void DragEnded(bool canceled) = 0;

 private:
  Panel* panel_;  // Weak pointer to the panel being dragged.

  DISALLOW_COPY_AND_ASSIGN(PanelDragDelegate);
};

// Delegate for moving a panel by dragging the mouse.
class MoveDragDelegate : public PanelDragDelegate {
 public:
  explicit MoveDragDelegate(Panel* panel)
      : PanelDragDelegate(panel) {}
  virtual ~MoveDragDelegate() {}

  virtual void DragStarted(gfx::Point point) OVERRIDE {
    panel_manager()->StartDragging(panel(), point);
  }
  virtual void Dragged(gfx::Point point) OVERRIDE {
    panel_manager()->Drag(point);
  }
  virtual void DragEnded(bool canceled) OVERRIDE {
    panel_manager()->EndDragging(canceled);
  }

  DISALLOW_COPY_AND_ASSIGN(MoveDragDelegate);
};

// Delegate for resizing a panel by dragging the mouse.
class ResizeDragDelegate : public PanelDragDelegate {
 public:
  ResizeDragDelegate(Panel* panel, GdkWindowEdge edge)
      : PanelDragDelegate(panel),
        resizing_side_(GdkWindowEdgeToResizingSide(edge)) {}
  virtual ~ResizeDragDelegate() {}

  virtual void DragStarted(gfx::Point point) OVERRIDE {
    panel_manager()->StartResizingByMouse(panel(), point, resizing_side_);
  }
  virtual void Dragged(gfx::Point point) OVERRIDE {
    panel_manager()->ResizeByMouse(point);
  }
  virtual void DragEnded(bool canceled) OVERRIDE {
    panel_manager()->EndResizingByMouse(canceled);
  }
 private:
  // The edge from which the panel is being resized.
  panel::ResizingSides resizing_side_;

  DISALLOW_COPY_AND_ASSIGN(ResizeDragDelegate);
};

// Panel drag helper for processing mouse and keyboard events while
// the left mouse button is pressed.
PanelDragGtk::PanelDragGtk(Panel* panel)
    : panel_(panel),
      drag_state_(NOT_DRAGGING),
      initial_mouse_down_(NULL),
      click_handler_(NULL),
      drag_delegate_(NULL) {
  // Create an invisible event box to receive mouse and key events.
  drag_widget_ = gtk_event_box_new();
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(drag_widget_), FALSE);

  // Connect signals for events during a drag.
  g_signal_connect(drag_widget_, "motion-notify-event",
                   G_CALLBACK(OnMouseMoveEventThunk), this);
  g_signal_connect(drag_widget_, "key-press-event",
                   G_CALLBACK(OnKeyPressEventThunk), this);
  g_signal_connect(drag_widget_, "key-release-event",
                   G_CALLBACK(OnKeyReleaseEventThunk), this);
  g_signal_connect(drag_widget_, "button-press-event",
                   G_CALLBACK(OnButtonPressEventThunk), this);
  g_signal_connect(drag_widget_, "button-release-event",
                   G_CALLBACK(OnButtonReleaseEventThunk), this);
  g_signal_connect(drag_widget_, "grab-broken-event",
                   G_CALLBACK(OnGrabBrokenEventThunk), this);
}

PanelDragGtk::~PanelDragGtk() {
  EndDrag(true);  // Clean up drag state.
  ReleasePointerAndKeyboardGrab();
}

void PanelDragGtk::AssertCleanState() {
  DCHECK_EQ(NOT_DRAGGING, drag_state_);
  DCHECK(!drag_delegate_);
  DCHECK(!initial_mouse_down_);
  DCHECK(!click_handler_);
}

void PanelDragGtk::InitialWindowEdgeMousePress(GdkEventButton* event,
                                               GdkCursor* cursor,
                                               GdkWindowEdge& edge) {
  AssertCleanState();
  drag_delegate_ = new ResizeDragDelegate(panel_, edge);
  drag_state_ = DRAG_CAN_START;
  GrabPointerAndKeyboard(event, cursor);
}

void PanelDragGtk::InitialTitlebarMousePress(GdkEventButton* event,
                                             GtkWidget* titlebar_widget) {
  AssertCleanState();
  click_handler_ = titlebar_widget;
  drag_delegate_ = new MoveDragDelegate(panel_);
  drag_state_ = DRAG_CAN_START;
  GrabPointerAndKeyboard(event, gfx::GetCursor(GDK_FLEUR));  // Drag cursor.
}

void PanelDragGtk::GrabPointerAndKeyboard(GdkEventButton* event,
                                          GdkCursor* cursor) {
  // Remember initial mouse event for use in determining when drag
  // threshold has been exceeded.
  initial_mouse_down_ = gdk_event_copy(reinterpret_cast<GdkEvent*>(event));

  // Grab pointer and keyboard to make sure we have the focus and get
  // all mouse and keyboard events during the drag.
  GdkWindow* gdk_window = gtk_widget_get_window(drag_widget_);
  DCHECK(gdk_window);
  GdkGrabStatus pointer_grab_status =
      gdk_pointer_grab(gdk_window,
                       TRUE,
                       GdkEventMask(GDK_BUTTON_PRESS_MASK |
                                    GDK_BUTTON_RELEASE_MASK |
                                    GDK_POINTER_MOTION_MASK),
                       NULL,
                       cursor,
                       event->time);
  GdkGrabStatus keyboard_grab_status =
      gdk_keyboard_grab(gdk_window, TRUE, event->time);
  if (pointer_grab_status != GDK_GRAB_SUCCESS ||
      keyboard_grab_status != GDK_GRAB_SUCCESS) {
    // Grab could fail if someone else already has the pointer/keyboard
    // grabbed. Cancel the drag.
    DLOG(ERROR) << "Unable to grab pointer or keyboard (pointer_status="
                << pointer_grab_status << ", keyboard_status="
                << keyboard_grab_status << ")";
    EndDrag(true);
    ReleasePointerAndKeyboardGrab();
    return;
  }

  gtk_grab_add(drag_widget_);
}

void PanelDragGtk::ReleasePointerAndKeyboardGrab() {
  DCHECK(!drag_delegate_);
  if (drag_state_ == NOT_DRAGGING)
    return;

  DCHECK_EQ(DRAG_ENDED_WAITING_FOR_MOUSE_RELEASE, drag_state_);
  gdk_pointer_ungrab(GDK_CURRENT_TIME);
  gdk_keyboard_ungrab(GDK_CURRENT_TIME);
  gtk_grab_remove(drag_widget_);
  drag_state_ = NOT_DRAGGING;  // Drag is truly over now.
}

void PanelDragGtk::EndDrag(bool canceled) {
  if (drag_state_ == NOT_DRAGGING ||
      drag_state_ == DRAG_ENDED_WAITING_FOR_MOUSE_RELEASE) {
    DCHECK(!drag_delegate_);
    return;
  }

  DCHECK(drag_delegate_);

  if (initial_mouse_down_) {
    gdk_event_free(initial_mouse_down_);
    initial_mouse_down_ = NULL;
  }

  if (drag_state_ == DRAG_IN_PROGRESS) {
    drag_delegate_->DragEnded(canceled);
  }
  drag_state_ = DRAG_ENDED_WAITING_FOR_MOUSE_RELEASE;

  delete drag_delegate_;
  drag_delegate_ = NULL;

  click_handler_ = NULL;
}

gboolean PanelDragGtk::OnMouseMoveEvent(GtkWidget* widget,
                                        GdkEventMotion* event) {
  DCHECK(drag_state_ != NOT_DRAGGING);

  if (drag_state_ == DRAG_ENDED_WAITING_FOR_MOUSE_RELEASE) {
    DCHECK(!drag_delegate_);
    return TRUE;
  }

  DCHECK(drag_delegate_);

  gdouble new_x_double;
  gdouble new_y_double;
  gdk_event_get_root_coords(reinterpret_cast<GdkEvent*>(event),
                            &new_x_double, &new_y_double);
  gint new_x = static_cast<gint>(new_x_double);
  gint new_y = static_cast<gint>(new_y_double);

  // Begin dragging only after mouse has moved beyond the drag threshold.
  if (drag_state_ == DRAG_CAN_START) {
    DCHECK(initial_mouse_down_);
    gdouble old_x_double;
    gdouble old_y_double;
    gdk_event_get_root_coords(initial_mouse_down_,
                              &old_x_double, &old_y_double);
    gint old_x = static_cast<gint>(old_x_double);
    gint old_y = static_cast<gint>(old_y_double);

    if (gtk_drag_check_threshold(drag_widget_, old_x, old_y,
                                 new_x, new_y)) {
      drag_state_ = DRAG_IN_PROGRESS;
      drag_delegate_->DragStarted(gfx::Point(old_x, old_y));
      gdk_event_free(initial_mouse_down_);
      initial_mouse_down_ = NULL;
    }
  }

  if (drag_state_ == DRAG_IN_PROGRESS)
    drag_delegate_->Dragged(gfx::Point(new_x, new_y));

  return TRUE;
}

gboolean PanelDragGtk::OnButtonPressEvent(GtkWidget* widget,
                                          GdkEventButton* event) {
  DCHECK(drag_state_ != NOT_DRAGGING);
  return TRUE;
}

gboolean PanelDragGtk::OnButtonReleaseEvent(GtkWidget* widget,
                                            GdkEventButton* event) {
  DCHECK(drag_state_ != NOT_DRAGGING);

  if (event->button == 1) {
    // Treat release as a mouse click if drag was never started.
    if (drag_state_ == DRAG_CAN_START && click_handler_) {
      gtk_propagate_event(click_handler_,
                          reinterpret_cast<GdkEvent*>(event));
    }
    // Cleanup state regardless.
    EndDrag(false);
    ReleasePointerAndKeyboardGrab();
  }

  return TRUE;
}

gboolean PanelDragGtk::OnKeyPressEvent(GtkWidget* widget,
                                       GdkEventKey* event) {
  DCHECK(drag_state_ != NOT_DRAGGING);
  return TRUE;
}

gboolean PanelDragGtk::OnKeyReleaseEvent(GtkWidget* widget,
                                         GdkEventKey* event) {
  DCHECK(drag_state_ != NOT_DRAGGING);

  if (drag_state_ == DRAG_ENDED_WAITING_FOR_MOUSE_RELEASE) {
    DCHECK(!drag_delegate_);
    return TRUE;
  }

  DCHECK(drag_delegate_);

  switch (event->keyval) {
  case GDK_Escape:
    EndDrag(true);  // Cancel drag.
    break;
  case GDK_Return:
  case GDK_KP_Enter:
  case GDK_ISO_Enter:
  case GDK_space:
    EndDrag(false);  // Normal end.
    break;
  }
  return TRUE;
}

gboolean PanelDragGtk::OnGrabBrokenEvent(GtkWidget* widget,
                                         GdkEventGrabBroken* event) {
  DCHECK(drag_state_ != NOT_DRAGGING);
  EndDrag(true);  // Cancel drag.
  ReleasePointerAndKeyboardGrab();
  return TRUE;
}
