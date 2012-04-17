// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_window_gtk.h"

#include "base/bind.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/gtk/browser_titlebar.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_bounds_animation.h"
#include "chrome/browser/ui/panels/panel_drag_gtk.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"

using content::WebContents;

namespace {

// RGB values for titlebar in draw attention state. A shade of orange.
const int kDrawAttentionR = 0xfa;
const int kDrawAttentionG = 0x98;
const int kDrawAttentionB = 0x3a;
const float kDrawAttentionRFraction = kDrawAttentionR / 255.0;
const float kDrawAttentionGFraction = kDrawAttentionG / 255.0;
const float kDrawAttentionBFraction = kDrawAttentionB / 255.0;

// Delay before click on a titlebar is allowed to minimize the panel after
// the 'draw attention' mode has been cleared.
const int kSuspendMinimizeOnClickIntervalMs = 500;

// Markup for title text in draw attention state. Set to color white.
const char* const kDrawAttentionTitleMarkupPrefix =
    "<span fgcolor='#ffffff'>";
const char* const kDrawAttentionTitleMarkupSuffix = "</span>";

// Set minimium width for window really small so it can be reduced to icon only
// size in overflow expansion state.
const int kMinWindowWidth = 2;

}  // namespace

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
      system_drag_disabled_for_testing_(false),
      last_mouse_down_(NULL),
      drag_widget_(NULL),
      drag_end_factory_(this),
      panel_(panel),
      bounds_(bounds),
      is_drawing_attention_(false),
      show_close_button_(true) {
}

PanelBrowserWindowGtk::~PanelBrowserWindowGtk() {
  CleanupDragDrop();
}

void PanelBrowserWindowGtk::Init() {
  BrowserWindowGtk::Init();

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

  registrar_.Add(
      this,
      chrome::NOTIFICATION_PANEL_CHANGED_LAYOUT_MODE,
      content::Source<Panel>(panel_.get()));
  registrar_.Add(
      this,
      chrome::NOTIFICATION_WINDOW_CLOSED,
      content::Source<GtkWindow>(window()));
}

bool PanelBrowserWindowGtk::GetWindowEdge(int x, int y, GdkWindowEdge* edge) {
  // Only detect the window edge when panels can be resized by the user.
  // This method is used by the base class to detect when the cursor has
  // hit the window edge in order to change the cursor to a resize cursor
  // and to detect when to initiate a resize drag.
  panel::Resizability resizability = panel_->CanResizeByMouse();
  if (panel::NOT_RESIZABLE == resizability)
    return false;

  if (!BrowserWindowGtk::GetWindowEdge(x, y, edge))
    return FALSE;

  // Special handling if bottom edge is not resizable.
  if (panel::RESIZABLE_ALL_SIDES_EXCEPT_BOTTOM == resizability) {
    if (*edge == GDK_WINDOW_EDGE_SOUTH)
      return FALSE;
    if (*edge == GDK_WINDOW_EDGE_SOUTH_WEST)
      *edge = GDK_WINDOW_EDGE_WEST;
    else if (*edge == GDK_WINDOW_EDGE_SOUTH_EAST)
      *edge = GDK_WINDOW_EDGE_EAST;
  }

  return TRUE;
}

void PanelBrowserWindowGtk::EnsureDragHelperCreated() {
  if (drag_helper_.get())
    return;

  drag_helper_.reset(new PanelDragGtk(panel_.get()));
  gtk_box_pack_end(GTK_BOX(window_vbox_), drag_helper_->widget(),
                   FALSE, FALSE, 0);
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

bool PanelBrowserWindowGtk::HandleWindowEdgeLeftMousePress(
    GtkWindow* window,
    GdkWindowEdge edge,
    GdkEventButton* event) {
  DCHECK_EQ(1U, event->button);
  DCHECK_EQ(GDK_BUTTON_PRESS, event->type);

  EnsureDragHelperCreated();
  // Resize cursor was set by BrowserWindowGtk when mouse moved over
  // window edge.
  GdkCursor* cursor =
      gdk_window_get_cursor(gtk_widget_get_window(GTK_WIDGET(window_)));
  drag_helper_->InitialWindowEdgeMousePress(event, cursor, edge);
  return TRUE;
}

void PanelBrowserWindowGtk::SaveWindowPosition() {
  // We don't save window position for panels as it's controlled by
  // PanelManager.
  return;
}

void PanelBrowserWindowGtk::SetGeometryHints() {
  // Set minimum height the window can be set to.
  GdkGeometry hints;
  hints.min_height = Panel::kMinimizedPanelHeight;
  hints.min_width = kMinWindowWidth;
  gtk_window_set_geometry_hints(
      window(), GTK_WIDGET(window()), &hints, GDK_HINT_MIN_SIZE);

  DCHECK(frame_size_.IsEmpty());
}

void PanelBrowserWindowGtk::SetBounds(const gfx::Rect& bounds) {
  // This should never be called.
  DLOG(WARNING) << "Unexpected call to PanelBrowserWindowGtk::SetBounds()";
}

void PanelBrowserWindowGtk::OnSizeChanged(int width, int height) {
  BrowserWindowGtk::OnSizeChanged(width, height);

  if (!frame_size_.IsEmpty())
    return;

  // The system is allowed to size the window before the desired panel
  // bounds are applied. Save the frame size allocated by the system as
  // it will be affected when we shrink the panel smaller than the frame.
  frame_size_ = GetNonClientFrameSize();

  int top = bounds_.bottom() - height;
  int left = bounds_.right() - width;

  gtk_window_move(window_, left, top);
  StartBoundsAnimation(gfx::Rect(left, top, width, height), bounds_);
  panel_->OnWindowSizeAvailable();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_WINDOW_SIZE_KNOWN,
      content::Source<Panel>(panel_.get()),
      content::NotificationService::NoDetails());
}

bool PanelBrowserWindowGtk::UseCustomFrame() {
  // We always use custom frame for panels.
  return TRUE;
}

void PanelBrowserWindowGtk::DrawPopupFrame(cairo_t* cr,
                                           GtkWidget* widget,
                                           GdkEventExpose* event) {
  BrowserWindowGtk::DrawPopupFrame(cr, widget, event);

  if (is_drawing_attention_)
    DrawAttentionFrame(cr, widget, event);
}

void PanelBrowserWindowGtk::DrawCustomFrame(cairo_t* cr,
                                            GtkWidget* widget,
                                            GdkEventExpose* event) {
  BrowserWindowGtk::DrawCustomFrame(cr, widget, event);

  if (is_drawing_attention_)
    DrawAttentionFrame(cr, widget, event);
}

void PanelBrowserWindowGtk::DrawAttentionFrame(cairo_t* cr,
                                               GtkWidget* widget,
                                               GdkEventExpose* event) {
  cairo_set_source_rgb(cr, kDrawAttentionRFraction,
                       kDrawAttentionGFraction,
                       kDrawAttentionBFraction);

  GdkRectangle dest_rectangle = GetTitlebarRectForDrawAttention();
  GdkRegion* dest_region = gdk_region_rectangle(&dest_rectangle);

  gdk_region_intersect(dest_region, event->region);
  gdk_cairo_region(cr, dest_region);

  cairo_clip(cr);
  cairo_paint(cr);
  gdk_region_destroy(dest_region);
}

void PanelBrowserWindowGtk::ActiveWindowChanged(GdkWindow* active_window) {
  bool was_active = IsActive();
  BrowserWindowGtk::ActiveWindowChanged(active_window);
  if (!window() || was_active == IsActive())  // State didn't change.
    return;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_ACTIVE_STATUS,
      content::Source<Panel>(panel_.get()),
      content::NotificationService::NoDetails());
}

BrowserWindowGtk::TitleDecoration PanelBrowserWindowGtk::GetWindowTitle(
    std::string* title) const {
  if (is_drawing_attention_) {
    std::string title_original;
    BrowserWindowGtk::TitleDecoration title_decoration =
        BrowserWindowGtk::GetWindowTitle(&title_original);
    DCHECK_EQ(BrowserWindowGtk::PLAIN_TEXT, title_decoration);
    gchar* title_escaped = g_markup_escape_text(title_original.c_str(), -1);
    gchar* title_with_markup = g_strconcat(kDrawAttentionTitleMarkupPrefix,
                                           title_escaped,
                                           kDrawAttentionTitleMarkupSuffix,
                                           NULL);
    *title = title_with_markup;
    g_free(title_escaped);
    g_free(title_with_markup);
    return BrowserWindowGtk::PANGO_MARKUP;
  } else {
    return BrowserWindowGtk::GetWindowTitle(title);
  }
}

bool PanelBrowserWindowGtk::ShouldShowCloseButton() const {
  return show_close_button_;
}

void PanelBrowserWindowGtk::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PANEL_CHANGED_LAYOUT_MODE:
      titlebar()->UpdateCustomFrame(true);
      break;
    case chrome::NOTIFICATION_WINDOW_CLOSED:
      // Cleanup.
      if (bounds_animator_.get())
        bounds_animator_.reset();

      CleanupDragDrop();
      if (drag_widget_) {
        // Terminate the grab if we have it. We could do this using any widget,
        // |drag_widget_| is just convenient.
        gtk_grab_add(drag_widget_);
        gtk_grab_remove(drag_widget_);
        DestroyDragWidget();
      }

      if (drag_helper_.get())
        drag_helper_.reset();

      panel_->OnNativePanelClosed();
      break;
  }

  BrowserWindowGtk::Observe(type, source, details);
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
  SetBoundsInternal(bounds, true);
}

void PanelBrowserWindowGtk::SetPanelBoundsInstantly(const gfx::Rect& bounds) {
  SetBoundsInternal(bounds, false);
}

void PanelBrowserWindowGtk::SetBoundsInternal(const gfx::Rect& bounds,
                                              bool animate) {
  if (bounds == bounds_)
    return;

  bool old_show_close_button = show_close_button_;
  show_close_button_ =
      bounds.width() > panel_->manager()->overflow_strip_width();
  if (show_close_button_ != old_show_close_button)
    titlebar()->UpdateCustomFrame(true);

  if (!bounds_.width() || !bounds_.height()) {
    // If the old bounds are 0 in either dimension, we need to show the
    // window as it would now be hidden.
    gtk_widget_show(GTK_WIDGET(window()));
    gdk_window_move(gtk_widget_get_window(GTK_WIDGET(window())), bounds_.x(),
                    bounds_.y());
  }

  if (!animate) {
    // If no animation is in progress, apply bounds change instantly. Otherwise,
    // continue the animation with new target bounds.
    if (!IsAnimatingBounds()) {
      if (bounds_.origin() != bounds.origin())
        gtk_window_move(window(), bounds.x(), bounds.y());
      if (bounds_.size() != bounds.size())
        ResizeWindow(bounds.width(), bounds.height());
    }
  } else if (!frame_size_.IsEmpty()) {
    StartBoundsAnimation(bounds_, bounds);
  } else {
    // Wait until the window frame has been sized before starting the animation
    // to avoid animating the window before it is shown.
  }

  bounds_ = bounds;
}

void PanelBrowserWindowGtk::ResizeWindow(int width, int height) {
  // Gtk does not allow window size to be set to 0, so make it 1 if it's 0.
  gtk_window_resize(window(), std::max(width, 1), std::max(height, 1));
}

void PanelBrowserWindowGtk::ClosePanel() {
  Close();
}

void PanelBrowserWindowGtk::ActivatePanel() {
  Activate();
}

void PanelBrowserWindowGtk::DeactivatePanel() {
  BrowserWindow* browser_window =
      panel_->manager()->GetNextBrowserWindowToActivate(panel_.get());
  if (browser_window) {
    browser_window->Activate();
  } else {
    Deactivate();
  }
}

bool PanelBrowserWindowGtk::IsPanelActive() const {
  return IsActive();
}

void PanelBrowserWindowGtk::PreventActivationByOS(bool prevent_activation) {
  gtk_window_set_accept_focus(window(), !prevent_activation);
  return;
}

gfx::NativeWindow PanelBrowserWindowGtk::GetNativePanelHandle() {
  return GetNativeHandle();
}

void PanelBrowserWindowGtk::UpdatePanelTitleBar() {
  UpdateTitleBar();
}

void PanelBrowserWindowGtk::UpdatePanelLoadingAnimations(bool should_animate) {
  UpdateLoadingAnimations(should_animate);
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

void PanelBrowserWindowGtk::PanelWebContentsFocused(WebContents* contents) {
  WebContentsFocused(contents);
}

void PanelBrowserWindowGtk::PanelCut() {
  Cut();
}

void PanelBrowserWindowGtk::PanelCopy() {
  Copy();
}

void PanelBrowserWindowGtk::PanelPaste() {
  Paste();
}

void PanelBrowserWindowGtk::DrawAttention(bool draw_attention) {
  DCHECK((panel_->attention_mode() & Panel::USE_PANEL_ATTENTION) != 0);

  if (is_drawing_attention_ == draw_attention)
    return;

  is_drawing_attention_ = draw_attention;

  GdkRectangle rect = GetTitlebarRectForDrawAttention();
  gdk_window_invalidate_rect(
      gtk_widget_get_window(GTK_WIDGET(window())), &rect, TRUE);

  UpdateTitleBar();

  if ((panel_->attention_mode() & Panel::USE_SYSTEM_ATTENTION) != 0)
    ::BrowserWindowGtk::FlashFrame(draw_attention);
}

bool PanelBrowserWindowGtk::IsDrawingAttention() const {
  return is_drawing_attention_;
}

bool PanelBrowserWindowGtk::PreHandlePanelKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return PreHandleKeyboardEvent(event, is_keyboard_shortcut);
}

void PanelBrowserWindowGtk::FullScreenModeChanged(bool is_full_screen) {
  // Nothing to do here as z-order rules for panels ensures that they're below
  // any app running in full screen mode.
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

gfx::Size PanelBrowserWindowGtk::IconOnlySize() const {
  GtkAllocation allocation;
  gtk_widget_get_allocation(titlebar_widget(), &allocation);
  return gfx::Size(titlebar()->IconOnlyWidth(), allocation.height);
}

void PanelBrowserWindowGtk::EnsurePanelFullyVisible() {
  gtk_window_present(window());
}

void PanelBrowserWindowGtk::SetPanelAppIconVisibility(bool visible) {
  return;
}

void PanelBrowserWindowGtk::SetPanelAlwaysOnTop(bool on_top) {
  gtk_window_set_keep_above(window(), on_top);
}

void PanelBrowserWindowGtk::EnableResizeByMouse(bool enable) {
}

gfx::Size PanelBrowserWindowGtk::WindowSizeFromContentSize(
    const gfx::Size& content_size) const {
  return gfx::Size(content_size.width() + frame_size_.width(),
                   content_size.height() + frame_size_.height());
}

gfx::Size PanelBrowserWindowGtk::ContentSizeFromWindowSize(
    const gfx::Size& window_size) const {
  return gfx::Size(window_size.width() - frame_size_.width(),
                   window_size.height() - frame_size_.height());
}

int PanelBrowserWindowGtk::TitleOnlyHeight() const {
  GtkAllocation allocation;
  gtk_widget_get_allocation(titlebar_widget(), &allocation);
  return allocation.height;
}

void PanelBrowserWindowGtk::StartBoundsAnimation(
    const gfx::Rect& from_bounds, const gfx::Rect& to_bounds) {
  animation_start_bounds_ = IsAnimatingBounds() ?
      last_animation_progressed_bounds_ : from_bounds;

  bounds_animator_.reset(new PanelBoundsAnimation(
      this, panel_.get(), animation_start_bounds_, to_bounds));

  bounds_animator_->Start();
  last_animation_progressed_bounds_ = animation_start_bounds_;
}

bool PanelBrowserWindowGtk::IsAnimatingBounds() const {
  return bounds_animator_.get() && bounds_animator_->is_animating();
}

void PanelBrowserWindowGtk::WillProcessEvent(GdkEvent* event) {
  // Nothing to do.
}

void PanelBrowserWindowGtk::DidProcessEvent(GdkEvent* event) {
  DCHECK(last_mouse_down_);
  if (event->type != GDK_MOTION_NOTIFY || !panel_->draggable())
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

  if (!drag_widget_ &&
      !IsAnimatingBounds() &&
      gtk_drag_check_threshold(titlebar_widget(), old_x,
                               old_y, new_x, new_y)) {
    CreateDragWidget();
    if (!system_drag_disabled_for_testing_) {
      GtkTargetList* list = ui::GetTargetListFromCodeMask(ui::CHROME_TAB);
      gtk_drag_begin(drag_widget_, list, GDK_ACTION_MOVE, 1, last_mouse_down_);
      // gtk_drag_begin increments reference count for GtkTargetList.  So unref
      // it here to reduce the reference count.
      gtk_target_list_unref(list);
    }
    panel_->manager()->StartDragging(panel_.get(), gfx::Point(old_x, old_y));
  }

  if (drag_widget_) {
    panel_->manager()->Drag(gfx::Point(new_x, new_y));
    gdk_event_free(last_mouse_down_);
    last_mouse_down_ = gdk_event_copy(event);
  }
}

void PanelBrowserWindowGtk::AnimationEnded(const ui::Animation* animation) {
  titlebar()->SendEnterNotifyToCloseButtonIfUnderMouse();

  // If the final size is 0 in either dimension, hide the window as gtk does
  // not allow the window size to be 0.
  if (!bounds_.width() || !bounds_.height())
    gtk_widget_hide(GTK_WIDGET(window()));

  panel_->manager()->OnPanelAnimationEnded(panel_.get());
}

void PanelBrowserWindowGtk::AnimationProgressed(
    const ui::Animation* animation) {
  DCHECK(!frame_size_.IsEmpty());

  gfx::Rect new_bounds = bounds_animator_->CurrentValueBetween(
      animation_start_bounds_, bounds_);

  gdk_window_move_resize(gtk_widget_get_window(GTK_WIDGET(window())),
                         new_bounds.x(), new_bounds.y(),
                         std::max(new_bounds.width(), 1),  // 0 not allowed
                         std::max(new_bounds.height(), 1));  // 0 not allowed

  last_animation_progressed_bounds_ = new_bounds;
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
  if (!system_drag_disabled_for_testing_)
    DCHECK(drag_widget_);

  // Make sure we only run EndDrag once by canceling any tasks that want
  // to call EndDrag.
  drag_end_factory_.InvalidateWeakPtrs();

  CleanupDragDrop();

  if (drag_widget_) {
    // We must let gtk clean up after we handle the drag operation, otherwise
    // there will be outstanding references to the drag widget when we try to
    // destroy it.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PanelBrowserWindowGtk::DestroyDragWidget,
                   drag_end_factory_.GetWeakPtr()));
    panel_->manager()->EndDragging(canceled);
  }
}

void PanelBrowserWindowGtk::CleanupDragDrop() {
  if (last_mouse_down_) {
    MessageLoopForUI::current()->RemoveObserver(this);
    gdk_event_free(last_mouse_down_);
    last_mouse_down_ = NULL;
  }
}

GdkRectangle PanelBrowserWindowGtk::GetTitlebarRectForDrawAttention() const {
  GdkRectangle rect;
  rect.x = 0;
  rect.y = 0;
  // We get the window width and not the titlebar_widget() width because we'd
  // like for the window borders on either side of the title bar to be the same
  // color.
  GtkAllocation window_allocation;
  gtk_widget_get_allocation(GTK_WIDGET(window()), &window_allocation);
  rect.width = window_allocation.width;

  GtkAllocation titlebar_allocation;
  gtk_widget_get_allocation(titlebar_widget(), &titlebar_allocation);
  rect.height = titlebar_allocation.height;

  return rect;
}

gboolean PanelBrowserWindowGtk::OnTitlebarButtonPressEvent(
    GtkWidget* widget, GdkEventButton* event) {
  // Early return if animation in progress.
  if (IsAnimatingBounds())
    return TRUE;

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
  if (event->button != 1) {
    DCHECK(!last_mouse_down_);
    return TRUE;
  }

  CleanupDragDrop();

  if (event->state & GDK_CONTROL_MASK) {
    panel_->OnTitlebarClicked(panel::APPLY_TO_ALL);
    return TRUE;
  }

  // TODO(jennb): Move remaining titlebar click handling out of here.
  // (http://crbug.com/118431)
  PanelStrip* panel_strip = panel_->panel_strip();
  if (!panel_strip)
    return TRUE;

  if (panel_strip->type() == PanelStrip::DOCKED &&
      panel_->expansion_state() == Panel::EXPANDED) {
    if (base::Time::Now() < disableMinimizeUntilTime_)
      return TRUE;

    panel_->SetExpansionState(Panel::MINIMIZED);
  } else {
    panel_->Activate();
  }

  return TRUE;
}

void PanelBrowserWindowGtk::HandleFocusIn(GtkWidget* widget,
                                          GdkEventFocus* event) {
  BrowserWindowGtk::HandleFocusIn(widget, event);

  if (!is_drawing_attention_)
    return;

  panel_->FlashFrame(false);
  DCHECK(panel_->expansion_state() == Panel::EXPANDED);

  disableMinimizeUntilTime_ = base::Time::Now() +
      base::TimeDelta::FromMilliseconds(kSuspendMinimizeOnClickIntervalMs);
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
      base::Bind(&PanelBrowserWindowGtk::EndDrag,
                 drag_end_factory_.GetWeakPtr(),
                 false));
  return TRUE;
}

// NativePanelTesting implementation.
class NativePanelTestingGtk : public NativePanelTesting {
 public:
  explicit NativePanelTestingGtk(
      PanelBrowserWindowGtk* panel_browser_window_gtk);

 private:
  virtual void PressLeftMouseButtonTitlebar(
      const gfx::Point& mouse_location, panel::ClickModifier modifier) OVERRIDE;
  virtual void ReleaseMouseButtonTitlebar(
      panel::ClickModifier modifier) OVERRIDE;
  virtual void DragTitlebar(const gfx::Point& mouse_location) OVERRIDE;
  virtual void CancelDragTitlebar() OVERRIDE;
  virtual void FinishDragTitlebar() OVERRIDE;
  virtual bool VerifyDrawingAttention() const OVERRIDE;
  virtual bool VerifyActiveState(bool is_active) OVERRIDE;
  virtual void WaitForWindowCreationToComplete() const OVERRIDE;
  virtual bool IsWindowSizeKnown() const OVERRIDE;
  virtual bool IsAnimatingBounds() const OVERRIDE;

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
    const gfx::Point& mouse_location, panel::ClickModifier modifier) {
  // If there is an animation, wait for it to finish as we don't handle button
  // clicks while animation is in progress.
  while (panel_browser_window_gtk_->IsAnimatingBounds())
    MessageLoopForUI::current()->RunAllPending();

  GdkEvent* event = gdk_event_new(GDK_BUTTON_PRESS);
  event->button.button = 1;
  event->button.x_root = mouse_location.x();
  event->button.y_root = mouse_location.y();
  if (modifier == panel::APPLY_TO_ALL)
    event->button.state |= GDK_CONTROL_MASK;
  panel_browser_window_gtk_->OnTitlebarButtonPressEvent(
      panel_browser_window_gtk_->titlebar_widget(),
      reinterpret_cast<GdkEventButton*>(event));
  gdk_event_free(event);
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::ReleaseMouseButtonTitlebar(
    panel::ClickModifier modifier) {
  GdkEvent* event = gdk_event_new(GDK_BUTTON_RELEASE);
  event->button.button = 1;
  if (modifier == panel::APPLY_TO_ALL)
    event->button.state |= GDK_CONTROL_MASK;
  panel_browser_window_gtk_->OnTitlebarButtonReleaseEvent(
      panel_browser_window_gtk_->titlebar_widget(),
      reinterpret_cast<GdkEventButton*>(event));
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::DragTitlebar(const gfx::Point& mouse_location) {
  // Prevent extra unwanted signals and focus grabs.
  panel_browser_window_gtk_->system_drag_disabled_for_testing_ = true;

  GdkEvent* event = gdk_event_new(GDK_MOTION_NOTIFY);
  event->motion.x_root = mouse_location.x();
  event->motion.y_root = mouse_location.y();
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

bool NativePanelTestingGtk::VerifyDrawingAttention() const {
  std::string title;
  BrowserWindowGtk::TitleDecoration decoration =
      panel_browser_window_gtk_->GetWindowTitle(&title);
  return panel_browser_window_gtk_->IsDrawingAttention() &&
         decoration == BrowserWindowGtk::PANGO_MARKUP;
}

bool NativePanelTestingGtk::VerifyActiveState(bool is_active) {
  // TODO(jianli): to be implemented. http://crbug.com/102737
  return false;
}

void NativePanelTestingGtk::WaitForWindowCreationToComplete() const {
  while (panel_browser_window_gtk_->frame_size_.IsEmpty())
    MessageLoopForUI::current()->RunAllPending();
  while (panel_browser_window_gtk_->IsAnimatingBounds())
    MessageLoopForUI::current()->RunAllPending();
}

bool NativePanelTestingGtk::IsWindowSizeKnown() const {
  return !panel_browser_window_gtk_->frame_size_.IsEmpty();
}

bool NativePanelTestingGtk::IsAnimatingBounds() const {
  return panel_browser_window_gtk_->IsAnimatingBounds();
}
