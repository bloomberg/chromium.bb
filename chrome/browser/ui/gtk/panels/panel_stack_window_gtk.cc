// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/panels/panel_stack_window_gtk.h"

#include <gdk/gdkkeysyms.h>
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "ui/base/x/active_window_watcher_x.h"

// static
NativePanelStackWindow* NativePanelStackWindow::Create(
    NativePanelStackWindowDelegate* delegate) {
  return new PanelStackWindowGtk(delegate);
}

PanelStackWindowGtk::PanelStackWindowGtk(
   NativePanelStackWindowDelegate* delegate)
   : delegate_(delegate),
     window_(NULL),
     is_minimized_(false),
     bounds_updates_started_(false) {
  ui::ActiveWindowWatcherX::AddObserver(this);
}

PanelStackWindowGtk::~PanelStackWindowGtk() {
  ui::ActiveWindowWatcherX::RemoveObserver(this);
}

void PanelStackWindowGtk::Close() {
  if (!window_)
    return;
  gtk_widget_destroy(GTK_WIDGET(window_));
  window_ = NULL;
}

void PanelStackWindowGtk::AddPanel(Panel* panel) {
  panels_.push_back(panel);

  EnsureWindowCreated();
  SetStackWindowBounds();

  // The panel being stacked should not appear on the taskbar.
  gtk_window_set_skip_taskbar_hint(panel->GetNativeWindow(), true);
}

void PanelStackWindowGtk::RemovePanel(Panel* panel) {
  panels_.remove(panel);

  SetStackWindowBounds();

  // The panel being unstacked should re-appear on the taskbar.
  // Note that the underlying gtk window is gone when the panel is being
  // closed.
  GtkWindow* gtk_window = panel->GetNativeWindow();
  if (gtk_window)
    gtk_window_set_skip_taskbar_hint(gtk_window, false);
}

void PanelStackWindowGtk::MergeWith(NativePanelStackWindow* another) {
  PanelStackWindowGtk* another_stack =
      static_cast<PanelStackWindowGtk*>(another);
  for (Panels::const_iterator iter = another_stack->panels_.begin();
       iter != another_stack->panels_.end(); ++iter) {
    Panel* panel = *iter;
    panels_.push_back(panel);
  }
  another_stack->panels_.clear();

  SetStackWindowBounds();
}

bool PanelStackWindowGtk::IsEmpty() const {
  return panels_.empty();
}

bool PanelStackWindowGtk::HasPanel(Panel* panel) const {
  return std::find(panels_.begin(), panels_.end(), panel) != panels_.end();
}

void PanelStackWindowGtk::MovePanelsBy(const gfx::Vector2d& delta) {
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    gfx::Rect bounds = panel->GetBounds();
    bounds.Offset(delta);
    panel->SetPanelBoundsInstantly(bounds);
  }

  SetStackWindowBounds();
}

void PanelStackWindowGtk::BeginBatchUpdatePanelBounds(bool animate) {
  // Bounds animation is not supported on GTK.
  bounds_updates_started_ = true;
}

void PanelStackWindowGtk::AddPanelBoundsForBatchUpdate(
    Panel* panel, const gfx::Rect& new_bounds) {
  DCHECK(bounds_updates_started_);

  // No need to track it if no change is needed.
  if (panel->GetBounds() == new_bounds)
    return;

  // New bounds are stored as the map value.
  bounds_updates_[panel] = new_bounds;
}

void PanelStackWindowGtk::EndBatchUpdatePanelBounds() {
  DCHECK(bounds_updates_started_);

  bounds_updates_started_ = false;

  for (BoundsUpdates::const_iterator iter = bounds_updates_.begin();
       iter != bounds_updates_.end(); ++iter) {
    iter->first->SetPanelBoundsInstantly(iter->second);
  }
  bounds_updates_.clear();

  SetStackWindowBounds();

  delegate_->PanelBoundsBatchUpdateCompleted();
}

bool PanelStackWindowGtk::IsAnimatingPanelBounds() const {
  return bounds_updates_started_;
}

void PanelStackWindowGtk::Minimize() {
  gtk_window_iconify(window_);
}

bool PanelStackWindowGtk::IsMinimized() const {
  return is_minimized_;
}

void PanelStackWindowGtk::DrawSystemAttention(bool draw_attention) {
  gtk_window_set_urgency_hint(window_, draw_attention);
}

void PanelStackWindowGtk::OnPanelActivated(Panel* panel) {
  // If a panel in a stack is activated, make sure all other panels in the stack
  // are brought to the top in the z-order.
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    GtkWindow* gtk_window = (*iter)->GetNativeWindow();
    if (gtk_window) {
      GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(gtk_window));
      gdk_window_raise(gdk_window);
    }
  }
}

void PanelStackWindowGtk::ActiveWindowChanged(GdkWindow* active_window) {
  // Bail out if icewm is detected. This is because icewm always creates a
  // window as active and we do not want to perform the logic here to
  // activate a panel window when the background window is being created.
  if (ui::GuessWindowManager() == ui::WM_ICE_WM)
    return;

  if (!window_ || panels_.empty())
    return;

  // The background stack window is activated when its taskbar icon is clicked.
  // When this occurs, we need to activate the most recently active panel.
  if (gtk_widget_get_window(GTK_WIDGET(window_)) == active_window) {
    Panel* panel_to_focus =
        panels_.front()->stack()->most_recently_active_panel();
    if (panel_to_focus)
      panel_to_focus->Activate();
  }
}

gboolean PanelStackWindowGtk::OnWindowDeleteEvent(GtkWidget* widget,
                                                  GdkEvent* event) {
  DCHECK(!panels_.empty());

  // Make a copy since closing a panel could modify the list.
  Panels panels_copy = panels_;
  for (Panels::const_iterator iter = panels_copy.begin();
       iter != panels_copy.end(); ++iter) {
    (*iter)->Close();
  }

  // Return true to prevent the gtk window from being destroyed.  Close will
  // destroy it for us.
  return TRUE;
}

gboolean PanelStackWindowGtk::OnWindowState(GtkWidget* widget,
                                            GdkEventWindowState* event) {
  bool is_minimized = event->new_window_state & GDK_WINDOW_STATE_ICONIFIED;
  if (is_minimized_ == is_minimized)
    return FALSE;
  is_minimized_ = is_minimized;

  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    GtkWindow* gtk_window = (*iter)->GetNativeWindow();
    if (is_minimized_)
      gtk_window_iconify(gtk_window);
    else
      gtk_window_deiconify(gtk_window);
  }

  return FALSE;
}

void PanelStackWindowGtk::EnsureWindowCreated() {
  if (window_)
    return;

  DCHECK(!panels_.empty());
  Panel* panel = panels_.front();

  // Create a small window that stays behinds the panels.
  window_ = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  gtk_window_set_decorated(window_, false);
  gtk_window_set_resizable(window_, false);
  gtk_window_set_focus_on_map(window_, false);
  gtk_widget_show(GTK_WIDGET(window_));
  gdk_window_move_resize(gtk_widget_get_window(GTK_WIDGET(window_)),
      panel->GetBounds().x(), panel->GetBounds().y(), 1, 1);
  gdk_window_lower(gtk_widget_get_window(GTK_WIDGET(window_)));

  // Connect signal handlers to the window.
  g_signal_connect(window_, "delete-event",
                   G_CALLBACK(OnWindowDeleteEventThunk), this);
  g_signal_connect(window_, "window-state-event",
                   G_CALLBACK(OnWindowStateThunk), this);

  // Should appear on the taskbar.
  gtk_window_set_skip_taskbar_hint(window_, false);

  // Set the window icon and title.
  string16 title = delegate_->GetTitle();
  gtk_window_set_title(window_, UTF16ToUTF8(title).c_str());

  gfx::Image app_icon = delegate_->GetIcon();
  if (!app_icon.IsEmpty())
    gtk_window_set_icon(window_, app_icon.ToGdkPixbuf());
}

void PanelStackWindowGtk::SetStackWindowBounds() {
  if (panels_.empty())
    return;
  Panel* panel = panels_.front();
  // Position the small background window a bit away from the left-top corner
  // such that it will be completely invisible.
  gdk_window_move_resize(gtk_widget_get_window(GTK_WIDGET(window_)),
      panel->GetBounds().x() + 5, panel->GetBounds().y() + 5, 1, 1);
}
