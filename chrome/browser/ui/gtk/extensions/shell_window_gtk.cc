// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/shell_window_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/extensions/extension_keybinding_registry_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/gtk_window_util.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/draggable_region.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/base/x/active_window_watcher_x.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"

namespace {

// The timeout in milliseconds before we'll get the true window position with
// gtk_window_get_position() after the last GTK configure-event signal.
const int kDebounceTimeoutMilliseconds = 100;

} // namespace

ShellWindowGtk::ShellWindowGtk(ShellWindow* shell_window,
                               const ShellWindow::CreateParams& params)
    : shell_window_(shell_window),
      state_(GDK_WINDOW_STATE_WITHDRAWN),
      is_active_(!ui::ActiveWindowWatcherX::WMSupportsActivation()),
      content_thinks_its_fullscreen_(false),
      frameless_(params.frame == ShellWindow::CreateParams::FRAME_NONE) {
  window_ = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));

  gfx::NativeView native_view =
      web_contents()->GetView()->GetNativeView();
  gtk_container_add(GTK_CONTAINER(window_), native_view);

  if (params.bounds.x() >= 0 && params.bounds.y() >= 0)
    gtk_window_move(window_, params.bounds.x(), params.bounds.y());

  // This is done to avoid a WM "feature" where setting the window size to
  // the monitor size causes the WM to set the EWMH for full screen mode.
  if (frameless_ &&
      gtk_window_util::BoundsMatchMonitorSize(window_, params.bounds)) {
    gtk_window_set_default_size(
        window_, params.bounds.width(), params.bounds.height() - 1);
  } else {
    gtk_window_set_default_size(
        window_, params.bounds.width(), params.bounds.height());
  }

  // make sure bounds_ and restored_bounds_ have correct values until we
  // get our first configure-event
  bounds_ = restored_bounds_ = params.bounds;
  gint x, y;
  gtk_window_get_position(window_, &x, &y);
  bounds_.set_origin(gfx::Point(x, y));

  // Hide titlebar when {frame: 'none'} specified on ShellWindow.
  if (frameless_)
    gtk_window_set_decorated(window_, false);

  int min_width = params.minimum_size.width();
  int min_height = params.minimum_size.height();
  int max_width = params.maximum_size.width();
  int max_height = params.maximum_size.height();
  GdkGeometry hints;
  int hints_mask = 0;
  if (min_width || min_height) {
    hints.min_height = min_height;
    hints.min_width = min_width;
    hints_mask |= GDK_HINT_MIN_SIZE;
  }
  if (max_width || max_height) {
    hints.max_height = max_height ? max_height : G_MAXINT;
    hints.max_width = max_width ? max_width : G_MAXINT;
    hints_mask |= GDK_HINT_MAX_SIZE;
  }
  if (hints_mask) {
    gtk_window_set_geometry_hints(
        window_,
        GTK_WIDGET(window_),
        &hints,
        static_cast<GdkWindowHints>(hints_mask));
  }

  // In some (older) versions of compiz, raising top-level windows when they
  // are partially off-screen causes them to get snapped back on screen, not
  // always even on the current virtual desktop.  If we are running under
  // compiz, suppress such raises, as they are not necessary in compiz anyway.
  if (ui::GuessWindowManager() == ui::WM_COMPIZ)
    suppress_window_raise_ = true;

  gtk_window_set_title(window_, extension()->name().c_str());

  gtk_window_util::SetWindowCustomClass(window_,
      web_app::GetWMClassFromAppName(extension()->name()));

  g_signal_connect(window_, "delete-event",
                   G_CALLBACK(OnMainWindowDeleteEventThunk), this);
  g_signal_connect(window_, "configure-event",
                   G_CALLBACK(OnConfigureThunk), this);
  g_signal_connect(window_, "window-state-event",
                   G_CALLBACK(OnWindowStateThunk), this);
  if (frameless_) {
    g_signal_connect(window_, "button-press-event",
                     G_CALLBACK(OnButtonPressThunk), this);
  }

  // Add the keybinding registry.
  extension_keybinding_registry_.reset(
      new ExtensionKeybindingRegistryGtk(shell_window_->profile(), window_,
          extensions::ExtensionKeybindingRegistry::PLATFORM_APPS_ONLY));

  ui::ActiveWindowWatcherX::AddObserver(this);
}

ShellWindowGtk::~ShellWindowGtk() {
  ui::ActiveWindowWatcherX::RemoveObserver(this);
}

bool ShellWindowGtk::IsActive() const {
  return is_active_;
}

bool ShellWindowGtk::IsMaximized() const {
  return (state_ & GDK_WINDOW_STATE_MAXIMIZED);
}

bool ShellWindowGtk::IsMinimized() const {
  return (state_ & GDK_WINDOW_STATE_ICONIFIED);
}

bool ShellWindowGtk::IsFullscreen() const {
  return false;
}

gfx::NativeWindow ShellWindowGtk::GetNativeWindow() {
  return window_;
}

gfx::Rect ShellWindowGtk::GetRestoredBounds() const {
  return restored_bounds_;
}

gfx::Rect ShellWindowGtk::GetBounds() const {
  return bounds_;
}

void ShellWindowGtk::Show() {
  gtk_window_present(window_);
}

void ShellWindowGtk::ShowInactive() {
  gtk_window_set_focus_on_map(window_, false);
  gtk_widget_show(GTK_WIDGET(window_));
}

void ShellWindowGtk::Close() {
  shell_window_->SaveWindowPosition();

  // Cancel any pending callback from the window configure debounce timer.
  window_configure_debounce_timer_.Stop();

  GtkWidget* window = GTK_WIDGET(window_);
  // To help catch bugs in any event handlers that might get fired during the
  // destruction, set window_ to NULL before any handlers will run.
  window_ = NULL;

  // OnNativeClose does a delete this so no other members should
  // be accessed after. gtk_widget_destroy is safe (and must
  // be last).
  shell_window_->OnNativeClose();
  gtk_widget_destroy(window);
}

void ShellWindowGtk::Activate() {
  gtk_window_present(window_);
}

void ShellWindowGtk::Deactivate() {
  gdk_window_lower(gtk_widget_get_window(GTK_WIDGET(window_)));
}

void ShellWindowGtk::Maximize() {
  gtk_window_maximize(window_);
}

void ShellWindowGtk::Minimize() {
  gtk_window_iconify(window_);
}

void ShellWindowGtk::Restore() {
  if (IsMaximized())
    gtk_window_unmaximize(window_);
  else if (IsMinimized())
    gtk_window_deiconify(window_);
}

void ShellWindowGtk::SetBounds(const gfx::Rect& bounds) {
  gtk_window_move(window_, bounds.x(), bounds.y());
  gtk_window_util::SetWindowSize(window_,
      gfx::Size(bounds.width(), bounds.height()));
}

void ShellWindowGtk::FlashFrame(bool flash) {
  gtk_window_set_urgency_hint(window_, flash);
}

bool ShellWindowGtk::IsAlwaysOnTop() const {
  return false;
}

void ShellWindowGtk::ActiveWindowChanged(GdkWindow* active_window) {
  // Do nothing if we're in the process of closing the browser window.
  if (!window_)
    return;

  is_active_ = gtk_widget_get_window(GTK_WIDGET(window_)) == active_window;
}

// Callback for the delete event.  This event is fired when the user tries to
// close the window (e.g., clicking on the X in the window manager title bar).
gboolean ShellWindowGtk::OnMainWindowDeleteEvent(GtkWidget* widget,
                                                 GdkEvent* event) {
  Close();

  // Return true to prevent the GTK window from being destroyed.  Close will
  // destroy it for us.
  return TRUE;
}

gboolean ShellWindowGtk::OnConfigure(GtkWidget* widget,
                                     GdkEventConfigure* event) {
  // We update |bounds_| but not |restored_bounds_| here.  The latter needs
  // to be updated conditionally when the window is non-maximized and non-
  // fullscreen, but whether those state updates have been processed yet is
  // window-manager specific.  We update |restored_bounds_| in the debounced
  // handler below, after the window state has been updated.
  bounds_.SetRect(event->x, event->y, event->width, event->height);

  // The GdkEventConfigure* we get here doesn't have quite the right
  // coordinates though (they're relative to the drawable window area, rather
  // than any window manager decorations, if enabled), so we need to call
  // gtk_window_get_position() to get the right values. (Otherwise session
  // restore, if enabled, will restore windows to incorrect positions.) That's
  // a round trip to the X server though, so we set a debounce timer and only
  // call it (in OnDebouncedBoundsChanged() below) after we haven't seen a
  // reconfigure event in a short while.
  // We don't use Reset() because the timer may not yet be running.
  // (In that case Stop() is a no-op.)
  window_configure_debounce_timer_.Stop();
  window_configure_debounce_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kDebounceTimeoutMilliseconds), this,
      &ShellWindowGtk::OnDebouncedBoundsChanged);

  return FALSE;
}

void ShellWindowGtk::OnDebouncedBoundsChanged() {
  gtk_window_util::UpdateWindowPosition(this, &bounds_, &restored_bounds_);
  shell_window_->SaveWindowPosition();
}

gboolean ShellWindowGtk::OnWindowState(GtkWidget* sender,
                                       GdkEventWindowState* event) {
  state_ = event->new_window_state;

  if (content_thinks_its_fullscreen_ &&
      !(state_ & GDK_WINDOW_STATE_FULLSCREEN)) {
    content_thinks_its_fullscreen_ = false;
    content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();
    if (rvh)
      rvh->ExitFullscreen();
  }

  return FALSE;
}

gboolean ShellWindowGtk::OnButtonPress(GtkWidget* widget,
                                       GdkEventButton* event) {
  if (!draggable_region_.isEmpty() &&
      draggable_region_.contains(event->x, event->y)) {
    if (event->button == 1) {
      if (GDK_BUTTON_PRESS == event->type) {
        if (!suppress_window_raise_)
          gdk_window_raise(GTK_WIDGET(widget)->window);

        return gtk_window_util::HandleTitleBarLeftMousePress(
            GTK_WINDOW(widget), bounds_, event);
      } else if (GDK_2BUTTON_PRESS == event->type) {
        bool is_maximized = gdk_window_get_state(GTK_WIDGET(widget)->window) &
                            GDK_WINDOW_STATE_MAXIMIZED;
        if (is_maximized) {
          gtk_window_util::UnMaximize(GTK_WINDOW(widget),
              bounds_, restored_bounds_);
        } else {
          gtk_window_maximize(GTK_WINDOW(widget));
        }
        return TRUE;
      }
    } else if (event->button == 2) {
      gdk_window_lower(GTK_WIDGET(widget)->window);
      return TRUE;
    }
  }

  return FALSE;
}

void ShellWindowGtk::SetFullscreen(bool fullscreen) {
  content_thinks_its_fullscreen_ = fullscreen;
  if (fullscreen)
    gtk_window_fullscreen(window_);
  else
    gtk_window_unfullscreen(window_);
}

bool ShellWindowGtk::IsFullscreenOrPending() const {
  return content_thinks_its_fullscreen_;
}

void ShellWindowGtk::UpdateWindowIcon() {
  Profile* profile = shell_window_->profile();
  gfx::Image app_icon = shell_window_->app_icon();
  if (!app_icon.IsEmpty())
    gtk_util::SetWindowIcon(window_, profile, app_icon.ToGdkPixbuf());
  else
    gtk_util::SetWindowIcon(window_, profile);
}

void ShellWindowGtk::UpdateWindowTitle() {
  string16 title = shell_window_->GetTitle();
  gtk_window_set_title(window_, UTF16ToUTF8(title).c_str());
}

void ShellWindowGtk::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // No-op.
}

void ShellWindowGtk::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  // Draggable region is not supported for non-frameless window.
  if (!frameless_)
    return;

  SkRegion draggable_region;

  // By default, the whole window is draggable.
  gfx::Rect bounds = GetBounds();
  draggable_region.op(0, 0, bounds.right(), bounds.bottom(),
                      SkRegion::kUnion_Op);

  // Exclude those designated as non-draggable.
  for (std::vector<extensions::DraggableRegion>::const_iterator iter =
           regions.begin();
       iter != regions.end(); ++iter) {
    const extensions::DraggableRegion& region = *iter;
    draggable_region.op(region.bounds.x(),
                        region.bounds.y(),
                        region.bounds.right(),
                        region.bounds.bottom(),
                        SkRegion::kDifference_Op);
  }

  draggable_region_ = draggable_region;
}

// static
NativeShellWindow* NativeShellWindow::Create(
    ShellWindow* shell_window, const ShellWindow::CreateParams& params) {
  return new ShellWindowGtk(shell_window, params);
}
