// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/reload_button_gtk.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/gtk_chrome_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/common/notification_source.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

// The width of this button in GTK+ theme mode. The Stop and Refresh stock icons
// can be different sizes; this variable is used to make sure that the button
// doesn't change sizes when switching between the two.
static int GtkButtonWidth = 0;

////////////////////////////////////////////////////////////////////////////////
// ReloadButton, public:

ReloadButtonGtk::ReloadButtonGtk(LocationBarViewGtk* location_bar,
                                 Browser* browser)
    : location_bar_(location_bar),
      browser_(browser),
      intended_mode_(MODE_RELOAD),
      visible_mode_(MODE_RELOAD),
      theme_provider_(browser ?
                      GtkThemeProvider::GetFrom(browser->profile()) : NULL),
      reload_(theme_provider_, IDR_RELOAD, IDR_RELOAD_P, IDR_RELOAD_H, 0),
      stop_(theme_provider_, IDR_STOP, IDR_STOP_P, IDR_STOP_H, IDR_STOP_D),
      widget_(gtk_chrome_button_new()),
      stop_to_reload_timer_delay_(base::TimeDelta::FromMilliseconds(1350)),
      testing_mouse_hovered_(false),
      testing_reload_count_(0) {
  gtk_widget_set_size_request(widget(), reload_.Width(), reload_.Height());

  gtk_widget_set_app_paintable(widget(), TRUE);

  g_signal_connect(widget(), "clicked", G_CALLBACK(OnClickedThunk), this);
  g_signal_connect(widget(), "expose-event", G_CALLBACK(OnExposeThunk), this);
  g_signal_connect(widget(), "leave-notify-event",
                   G_CALLBACK(OnLeaveNotifyThunk), this);
  GTK_WIDGET_UNSET_FLAGS(widget(), GTK_CAN_FOCUS);

  gtk_widget_set_has_tooltip(widget(), TRUE);
  g_signal_connect(widget(), "query-tooltip", G_CALLBACK(OnQueryTooltipThunk),
                   this);

  hover_controller_.Init(widget());
  gtk_util::SetButtonTriggersNavigation(widget());

  if (theme_provider_) {
    theme_provider_->InitThemesFor(this);
    registrar_.Add(this,
                   NotificationType::BROWSER_THEME_CHANGED,
                   Source<GtkThemeProvider>(theme_provider_));
  }

  // Set the default double-click timer delay to the system double-click time.
  int timer_delay_ms;
  GtkSettings* settings = gtk_settings_get_default();
  g_object_get(G_OBJECT(settings), "gtk-double-click-time", &timer_delay_ms,
               NULL);
  double_click_timer_delay_ = base::TimeDelta::FromMilliseconds(timer_delay_ms);
}

ReloadButtonGtk::~ReloadButtonGtk() {
  widget_.Destroy();
}

void ReloadButtonGtk::ChangeMode(Mode mode, bool force) {
  intended_mode_ = mode;

  // If the change is forced, or the user isn't hovering the icon, or it's safe
  // to change it to the other image type, make the change immediately;
  // otherwise we'll let it happen later.
  if (force || ((GTK_WIDGET_STATE(widget()) == GTK_STATE_NORMAL) &&
      !testing_mouse_hovered_) || ((mode == MODE_STOP) ?
          !double_click_timer_.IsRunning() : (visible_mode_ != MODE_STOP))) {
    double_click_timer_.Stop();
    stop_to_reload_timer_.Stop();
    visible_mode_ = mode;

    stop_.set_paint_override(-1);
    gtk_chrome_button_unset_paint_state(GTK_CHROME_BUTTON(widget_.get()));

    UpdateThemeButtons();
    gtk_widget_queue_draw(widget());
  } else if (visible_mode_ != MODE_RELOAD) {
    // If you read the views implementation of reload_button.cc, you'll see
    // that instead of screwing with paint states, the views implementation
    // just changes whether the view is enabled. We can't do that here because
    // changing the widget state to GTK_STATE_INSENSITIVE will cause a cascade
    // of messages on all its children and will also trigger a synthesized
    // leave notification and prevent the real leave notification from turning
    // the button back to normal. So instead, override the stop_ paint state
    // for chrome-theme mode, and use this as a flag to discard click events.
    stop_.set_paint_override(GTK_STATE_INSENSITIVE);

    // Also set the gtk_chrome_button paint state to insensitive to hide
    // the border drawn around the stop icon.
    gtk_chrome_button_set_paint_state(GTK_CHROME_BUTTON(widget_.get()),
                                      GTK_STATE_INSENSITIVE);

    // If we're in GTK theme mode, we need to also render the correct icon for
    // the stop/insensitive since we won't be using |stop_| to render the icon.
    UpdateThemeButtons();

    // Go ahead and change to reload after a bit, which allows repeated reloads
    // without moving the mouse.
    if (!stop_to_reload_timer_.IsRunning()) {
      stop_to_reload_timer_.Start(stop_to_reload_timer_delay_, this,
                                  &ReloadButtonGtk::OnStopToReloadTimer);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ReloadButtonGtk, NotificationObserver implementation:

void ReloadButtonGtk::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& /* details */) {
  DCHECK(NotificationType::BROWSER_THEME_CHANGED == type);

  GtkThemeProvider* provider = static_cast<GtkThemeProvider*>(
      Source<GtkThemeProvider>(source).ptr());
  DCHECK_EQ(provider, theme_provider_);
  GtkButtonWidth = 0;
  UpdateThemeButtons();
}

////////////////////////////////////////////////////////////////////////////////
// ReloadButtonGtk, private:

void ReloadButtonGtk::OnClicked(GtkWidget* /* sender */) {
  if (visible_mode_ == MODE_STOP) {
    // Do nothing if Stop was disabled due to an attempt to change back to
    // RELOAD mode while hovered.
    if (stop_.paint_override() == GTK_STATE_INSENSITIVE)
      return;

    if (browser_)
      browser_->Stop();

    // The user has clicked, so we can feel free to update the button,
    // even if the mouse is still hovering.
    ChangeMode(MODE_RELOAD, true);
  } else if (!double_click_timer_.IsRunning()) {
    // Shift-clicking or Ctrl-clicking the reload button means we should ignore
    // any cached content.
    int command;
    GdkModifierType modifier_state;
    gtk_get_current_event_state(&modifier_state);
    guint modifier_state_uint = modifier_state;
    if (modifier_state_uint & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) {
      command = IDC_RELOAD_IGNORING_CACHE;
      // Mask off Shift and Control so they don't affect the disposition below.
      modifier_state_uint &= ~(GDK_SHIFT_MASK | GDK_CONTROL_MASK);
    } else {
      command = IDC_RELOAD;
    }

    WindowOpenDisposition disposition =
        event_utils::DispositionFromEventFlags(modifier_state_uint);
    if ((disposition == CURRENT_TAB) && location_bar_) {
      // Forcibly reset the location bar, since otherwise it won't discard any
      // ongoing user edits, since it doesn't realize this is a user-initiated
      // action.
      location_bar_->Revert();
    }

    // Start a timer - while this timer is running, the reload button cannot be
    // changed to a stop button.  We do not set |intended_mode_| to MODE_STOP
    // here as the browser will do that when it actually starts loading (which
    // may happen synchronously, thus the need to do this before telling the
    // browser to execute the reload command).
    double_click_timer_.Start(double_click_timer_delay_, this,
                              &ReloadButtonGtk::OnDoubleClickTimer);

    if (browser_)
      browser_->ExecuteCommandWithDisposition(command, disposition);
    ++testing_reload_count_;
  }
}

gboolean ReloadButtonGtk::OnExpose(GtkWidget* widget,
                                   GdkEventExpose* e) {
  if (theme_provider_ && theme_provider_->UseGtkTheme())
    return FALSE;
  return ((visible_mode_ == MODE_RELOAD) ? reload_ : stop_).OnExpose(
      widget, e, hover_controller_.GetCurrentValue());
}

gboolean ReloadButtonGtk::OnLeaveNotify(GtkWidget* /* widget */,
                                        GdkEventCrossing* /* event */) {
  ChangeMode(intended_mode_, true);
  return FALSE;
}

gboolean ReloadButtonGtk::OnQueryTooltip(GtkWidget* /* sender */,
                                         gint /* x */,
                                         gint /* y */,
                                         gboolean /* keyboard_mode */,
                                         GtkTooltip* tooltip) {
  // |location_bar_| can be NULL in tests.
  if (!location_bar_)
    return FALSE;

  gtk_tooltip_set_text(tooltip, l10n_util::GetStringUTF8(
      (visible_mode_ == MODE_RELOAD) ?
      IDS_TOOLTIP_RELOAD : IDS_TOOLTIP_STOP).c_str());
  return TRUE;
}

void ReloadButtonGtk::UpdateThemeButtons() {
  bool use_gtk = theme_provider_ && theme_provider_->UseGtkTheme();

  if (use_gtk) {
    gtk_widget_ensure_style(widget());
    GtkIconSet* icon_set = gtk_style_lookup_icon_set(
        widget()->style,
        (visible_mode_ == MODE_RELOAD) ? GTK_STOCK_REFRESH : GTK_STOCK_STOP);
    if (icon_set) {
      GtkStateType state = static_cast<GtkStateType>(
          GTK_WIDGET_STATE(widget()));
      if (visible_mode_ == MODE_STOP && stop_.paint_override() != -1)
        state = static_cast<GtkStateType>(stop_.paint_override());

      GdkPixbuf* pixbuf = gtk_icon_set_render_icon(
          icon_set,
          widget()->style,
          gtk_widget_get_direction(widget()),
          state,
          GTK_ICON_SIZE_SMALL_TOOLBAR,
          widget(),
          NULL);

      gtk_button_set_image(GTK_BUTTON(widget()),
                           gtk_image_new_from_pixbuf(pixbuf));
      g_object_unref(pixbuf);
    }

    gtk_widget_set_size_request(widget(), -1, -1);
    GtkRequisition req;
    gtk_widget_size_request(widget(), &req);
    GtkButtonWidth = std::max(GtkButtonWidth, req.width);
    gtk_widget_set_size_request(widget(), GtkButtonWidth, -1);

    gtk_widget_set_app_paintable(widget(), FALSE);
    gtk_widget_set_double_buffered(widget(), TRUE);
  } else {
    gtk_button_set_image(GTK_BUTTON(widget()), NULL);

    gtk_widget_set_size_request(widget(), reload_.Width(), reload_.Height());

    gtk_widget_set_app_paintable(widget(), TRUE);
    // We effectively double-buffer by virtue of having only one image...
    gtk_widget_set_double_buffered(widget(), FALSE);
  }

  gtk_chrome_button_set_use_gtk_rendering(GTK_CHROME_BUTTON(widget()), use_gtk);
}

void ReloadButtonGtk::OnDoubleClickTimer() {
  ChangeMode(intended_mode_, false);
}

void ReloadButtonGtk::OnStopToReloadTimer() {
  ChangeMode(intended_mode_, true);
}
