// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_window_gtk.h"

#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"

NativePanel* Panel::CreateNativePanel(Browser* browser, Panel* panel,
                                      const gfx::Rect& bounds) {
  PanelBrowserWindowGtk* panel_browser_window_gtk =
      new PanelBrowserWindowGtk(browser, panel, bounds);
  panel_browser_window_gtk->Init();
  return panel_browser_window_gtk;
}

PanelBrowserWindowGtk::PanelBrowserWindowGtk(Browser* browser,
                                             Panel* panel,
                                             const gfx::Rect& bounds)
    : BrowserWindowGtk(browser),
      last_mouse_down_(NULL),
      drag_widget_(NULL),
      destroy_drag_widget_factory_(this),
      drag_end_factory_(this),
      panel_(panel),
      bounds_(bounds) {
}

PanelBrowserWindowGtk::~PanelBrowserWindowGtk() {
  if (drag_widget_) {
    // Terminate the grab if we have it. We could do this using any widget,
    // |drag_widget_| is just convenient.
    gtk_grab_add(drag_widget_);
    gtk_grab_remove(drag_widget_);
    DestroyDragWidget();
  }
}

void PanelBrowserWindowGtk::Init() {
  BrowserWindowGtk::Init();

  // Keep the window docked to the bottom of the screen on resizes.
  gtk_window_set_gravity(window(), GDK_GRAVITY_SOUTH_EAST);

  // Keep the window always on top.
  gtk_window_set_keep_above(window(), TRUE);

  // Show the window on all the virtual desktops.
  gtk_window_stick(window());

  // Do not show an icon in the task bar.  Window operations such as close,
  // minimize etc. can only be done from the panel UI.
  gtk_window_set_skip_taskbar_hint(window(), TRUE);

  g_signal_connect(titlebar_widget(), "button-press-event",
                   G_CALLBACK(OnTitlebarButtonPressEventThunk), this);
  g_signal_connect(titlebar_widget(), "button-release-event",
                   G_CALLBACK(OnTitlebarButtonReleaseEventThunk), this);
}

bool PanelBrowserWindowGtk::GetWindowEdge(int x, int y, GdkWindowEdge* edge) {
  // Since panels are not resizable or movable by the user, we should not
  // detect the window edge for behavioral purposes.  The edge, if any,
  // is present only for visual aspects.
  return FALSE;
}

bool PanelBrowserWindowGtk::HandleTitleBarLeftMousePress(
    GdkEventButton* event,
    guint32 last_click_time,
    gfx::Point last_click_position) {
  // In theory we should never enter this function as we have a handler for
  // button press on titlebar where we handle this.  Not putting NOTREACHED()
  // here because we don't want to crash if hit-testing for titlebar in window
  // button press handler in BrowserWindowGtk is off by a pixel or two.
  DLOG(WARNING) << "Hit-testing for titlebar off by a pixel or two?";
  return TRUE;
}

void PanelBrowserWindowGtk::SaveWindowPosition() {
  // We don't save window position for panels as it's controlled by
  // PanelManager.
  return;
}

void PanelBrowserWindowGtk::SetGeometryHints() {
  SetBoundsImpl();
}

void PanelBrowserWindowGtk::SetBounds(const gfx::Rect& bounds) {
  if (bounds_ != bounds) {
    bounds_ = bounds;
    SetBoundsImpl();
  }
}

bool PanelBrowserWindowGtk::UseCustomFrame() {
  // We always use custom frame for panels.
  return TRUE;
}

void PanelBrowserWindowGtk::ShowPanel() {
  Show();
}

void PanelBrowserWindowGtk::ShowPanelInactive() {
  ShowInactive();
}

gfx::Rect PanelBrowserWindowGtk::GetPanelBounds() const {
  return bounds_;
}

void PanelBrowserWindowGtk::SetPanelBounds(const gfx::Rect& bounds) {
  SetBounds(bounds);
}

void PanelBrowserWindowGtk::OnPanelExpansionStateChanged(
    Panel::ExpansionState expansion_state) {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindowGtk::ShouldBringUpPanelTitlebar(int mouse_x,
                                                       int mouse_y) const {
  NOTIMPLEMENTED();
  return false;
}

void PanelBrowserWindowGtk::ClosePanel() {
  Close();
}

void PanelBrowserWindowGtk::ActivatePanel() {
  Activate();
}

void PanelBrowserWindowGtk::DeactivatePanel() {
  Deactivate();
}

bool PanelBrowserWindowGtk::IsPanelActive() const {
  return IsActive();
}

gfx::NativeWindow PanelBrowserWindowGtk::GetNativePanelHandle() {
  return GetNativeHandle();
}

void PanelBrowserWindowGtk::UpdatePanelTitleBar() {
  UpdateTitleBar();
}

void PanelBrowserWindowGtk::ShowTaskManagerForPanel() {
  ShowTaskManager();
}

FindBar* PanelBrowserWindowGtk::CreatePanelFindBar() {
  return CreateFindBar();
}

void PanelBrowserWindowGtk::NotifyPanelOnUserChangedTheme() {
  UserChangedTheme();
}

void PanelBrowserWindowGtk::DrawAttention() {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindowGtk::IsDrawingAttention() const {
  NOTIMPLEMENTED();
  return false;
}

bool PanelBrowserWindowGtk::PreHandlePanelKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return PreHandleKeyboardEvent(event, is_keyboard_shortcut);
}

void PanelBrowserWindowGtk::HandlePanelKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  HandleKeyboardEvent(event);
}

Browser* PanelBrowserWindowGtk::GetPanelBrowser() const {
  return browser();
}

void PanelBrowserWindowGtk::DestroyPanelBrowser() {
  DestroyBrowser();
}

gfx::Size PanelBrowserWindowGtk::GetNonClientAreaExtent() const {
  NOTIMPLEMENTED();
  return gfx::Size();
}

int PanelBrowserWindowGtk::GetRestoredHeight() const {
  NOTIMPLEMENTED();
  return 0;
}

void PanelBrowserWindowGtk::SetRestoredHeight(int height) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowGtk::SetBoundsImpl() {
  gtk_window_move(window_, bounds_.x(), bounds_.y());
  gtk_window_resize(window(), bounds_.width(), bounds_.height());
}

void PanelBrowserWindowGtk::WillProcessEvent(GdkEvent* event) {
  // Nothing to do.
}

void PanelBrowserWindowGtk::DidProcessEvent(GdkEvent* event) {
  DCHECK(last_mouse_down_);
  if (event->type != GDK_MOTION_NOTIFY)
    return;

  gdouble new_x_double;
  gdouble new_y_double;
  gdouble old_x_double;
  gdouble old_y_double;
  gdk_event_get_root_coords(event, &new_x_double, &new_y_double);
  gdk_event_get_root_coords(last_mouse_down_, &old_x_double, &old_y_double);

  gint new_x = static_cast<gint>(new_x_double);
  gint new_y = static_cast<gint>(new_y_double);
  gint old_x = static_cast<gint>(old_x_double);
  gint old_y = static_cast<gint>(old_y_double);

  if (drag_widget_) {
    panel_->manager()->Drag(new_x - old_x);
    gdk_event_free(last_mouse_down_);
    last_mouse_down_ = gdk_event_copy(reinterpret_cast<GdkEvent*>(event));
  } else if (gtk_drag_check_threshold(titlebar_widget(), old_x,
                                      old_y, new_x, new_y)) {
    CreateDragWidget();
    GtkTargetList* list = ui::GetTargetListFromCodeMask(ui::CHROME_TAB);
    gtk_drag_begin(drag_widget_, list, GDK_ACTION_MOVE, 1, last_mouse_down_);
    // gtk_drag_begin increments reference count for GtkTargetList.  So unref
    // it here to reduce the reference count.
    gtk_target_list_unref(list);
    panel_->manager()->StartDragging(panel_.get());
  }
}

void PanelBrowserWindowGtk::CreateDragWidget() {
  DCHECK(!drag_widget_);
  drag_widget_ = gtk_invisible_new();
  g_signal_connect_after(drag_widget_, "drag-begin",
                         G_CALLBACK(OnDragBeginThunk), this);
  g_signal_connect(drag_widget_, "drag-failed",
                   G_CALLBACK(OnDragFailedThunk), this);
  g_signal_connect(drag_widget_, "button-release-event",
                   G_CALLBACK(OnDragButtonReleasedThunk), this);
}

void PanelBrowserWindowGtk::DestroyDragWidget() {
  if (drag_widget_) {
    gtk_widget_destroy(drag_widget_);
    drag_widget_ = NULL;
  }
}

void PanelBrowserWindowGtk::EndDrag(bool canceled) {
  // Make sure we only run EndDrag once by canceling any tasks that want
  // to call EndDrag.
  drag_end_factory_.RevokeAll();

  // We must let gtk clean up after we handle the drag operation, otherwise
  // there will be outstanding references to the drag widget when we try to
  // destroy it.
  MessageLoop::current()->PostTask(FROM_HERE,
      destroy_drag_widget_factory_.NewRunnableMethod(
      &PanelBrowserWindowGtk::DestroyDragWidget));
  CleanupDragDrop();
  panel_->manager()->EndDragging(canceled);
}

void PanelBrowserWindowGtk::CleanupDragDrop() {
  if (last_mouse_down_) {
    MessageLoopForUI::current()->RemoveObserver(this);
    gdk_event_free(last_mouse_down_);
    last_mouse_down_ = NULL;
  }
}

gboolean PanelBrowserWindowGtk::OnTitlebarButtonPressEvent(
    GtkWidget* widget, GdkEventButton* event) {
  // Every button press ensures either a button-release-event or a drag-fail
  // signal for |widget|.
  if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
    // Store the button press event, used to initiate a drag.
    DCHECK(!last_mouse_down_);
    last_mouse_down_ = gdk_event_copy(reinterpret_cast<GdkEvent*>(event));
    MessageLoopForUI::current()->AddObserver(this);
  }

  return TRUE;
}

gboolean PanelBrowserWindowGtk::OnTitlebarButtonReleaseEvent(
    GtkWidget* widget, GdkEventButton* event) {
  CleanupDragDrop();
  return TRUE;
}

void PanelBrowserWindowGtk::OnDragBegin(GtkWidget* widget,
                                        GdkDragContext* context) {
  // Set drag icon to be a transparent pixbuf.
  GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
  gdk_pixbuf_fill(pixbuf, 0);
  gtk_drag_set_icon_pixbuf(context, pixbuf, 0, 0);
  g_object_unref(pixbuf);
}

gboolean PanelBrowserWindowGtk::OnDragFailed(
    GtkWidget* widget, GdkDragContext* context, GtkDragResult result) {
  bool canceled = (result != GTK_DRAG_RESULT_NO_TARGET);
  EndDrag(canceled);
  return TRUE;
}

gboolean PanelBrowserWindowGtk::OnDragButtonReleased(GtkWidget* widget,
                                                     GdkEventButton* button) {
  // We always get this event when gtk is releasing the grab and ending the
  // drag.  This gets fired before drag-failed handler gets fired.  However,
  // if the user ended the drag with space or enter, we don't get a follow up
  // event to tell us the drag has finished (either a drag-failed or a
  // drag-end).  We post a task, instead of calling EndDrag right here, to give
  // GTK+ a chance to send the drag-failed event with the right status.  If
  // GTK+ does send the drag-failed event, we cancel the task.
  MessageLoop::current()->PostTask(FROM_HERE,
      drag_end_factory_.NewRunnableMethod(
          &PanelBrowserWindowGtk::EndDrag, false));
  return TRUE;
}

// NativePanelTesting implementation.
class NativePanelTestingGtk : public NativePanelTesting {
 public:
  explicit NativePanelTestingGtk(
      PanelBrowserWindowGtk* panel_browser_window_gtk);

 private:
  virtual void PressLeftMouseButtonTitlebar(
      const gfx::Point& point) OVERRIDE;
  virtual void ReleaseMouseButtonTitlebar() OVERRIDE;
  virtual void DragTitlebar(int delta_x, int delta_y) OVERRIDE;
  virtual void CancelDragTitlebar() OVERRIDE;
  virtual void FinishDragTitlebar() OVERRIDE;

  PanelBrowserWindowGtk* panel_browser_window_gtk_;
};

// static
NativePanelTesting* NativePanelTesting::Create(NativePanel* native_panel) {
  return new NativePanelTestingGtk(static_cast<PanelBrowserWindowGtk*>(
      native_panel));
}

NativePanelTestingGtk::NativePanelTestingGtk(
    PanelBrowserWindowGtk* panel_browser_window_gtk) :
    panel_browser_window_gtk_(panel_browser_window_gtk) {
}

void NativePanelTestingGtk::PressLeftMouseButtonTitlebar(
    const gfx::Point& point) {
  GdkEvent* event = gdk_event_new(GDK_BUTTON_PRESS);
  event->button.button = 1;
  event->button.x_root = point.x();
  event->button.y_root = point.y();
  panel_browser_window_gtk_->OnTitlebarButtonPressEvent(
      panel_browser_window_gtk_->titlebar_widget(),
      reinterpret_cast<GdkEventButton*>(event));
  gdk_event_free(event);
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::ReleaseMouseButtonTitlebar() {
  panel_browser_window_gtk_->OnTitlebarButtonReleaseEvent(NULL, NULL);
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::DragTitlebar(int delta_x, int delta_y) {
  if (!panel_browser_window_gtk_->drag_widget_) {
    panel_browser_window_gtk_->CreateDragWidget();
    panel_browser_window_gtk_->panel_->manager()->StartDragging(
        panel_browser_window_gtk_->panel_.get());
  }
  GdkEvent* event = gdk_event_new(GDK_MOTION_NOTIFY);
  gdk_event_get_root_coords(panel_browser_window_gtk_->last_mouse_down_,
      &event->motion.x_root, &event->motion.y_root);
  event->motion.x_root += delta_x;
  event->motion.y_root += delta_y;
  panel_browser_window_gtk_->DidProcessEvent(event);
  gdk_event_free(event);
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::CancelDragTitlebar() {
  panel_browser_window_gtk_->OnDragFailed(
      panel_browser_window_gtk_->drag_widget_, NULL,
      GTK_DRAG_RESULT_USER_CANCELLED);
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::FinishDragTitlebar() {
  panel_browser_window_gtk_->OnDragFailed(
      panel_browser_window_gtk_->drag_widget_, NULL,
      GTK_DRAG_RESULT_NO_TARGET);
  MessageLoopForUI::current()->RunAllPending();
}
