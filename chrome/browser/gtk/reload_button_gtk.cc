// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/reload_button_gtk.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/location_bar_view_gtk.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

// The width of this button in GTK+ theme mode. The Stop and Refresh stock icons
// can be different sizes; this variable is used to make sure that the button
// doesn't change sizes when switching between the two.
static int GtkButtonWidth = 0;

ReloadButtonGtk::ReloadButtonGtk(LocationBarViewGtk* location_bar,
                                 Browser* browser)
    : location_bar_(location_bar),
      browser_(browser),
      button_delay_(0),
      pretend_timer_is_running_for_unittest_(false),
      intended_mode_(MODE_RELOAD),
      visible_mode_(MODE_RELOAD),
      theme_provider_(browser ?
                      GtkThemeProvider::GetFrom(browser->profile()) : NULL),
      reload_(theme_provider_, IDR_RELOAD, IDR_RELOAD_P, IDR_RELOAD_H, 0,
              IDR_BUTTON_MASK),
      stop_(theme_provider_, IDR_STOP, IDR_STOP_P, IDR_STOP_H, 0,
            IDR_BUTTON_MASK),
      widget_(gtk_chrome_button_new()) {
  gtk_widget_set_size_request(widget_.get(), reload_.Width(), reload_.Height());

  gtk_widget_set_app_paintable(widget_.get(), TRUE);

  g_signal_connect(widget_.get(), "expose-event",
                   G_CALLBACK(OnExposeThunk), this);
  g_signal_connect(widget_.get(), "leave-notify-event",
                   G_CALLBACK(OnLeaveNotifyThunk), this);
  g_signal_connect(widget_.get(), "clicked",
                   G_CALLBACK(OnClickedThunk), this);
  GTK_WIDGET_UNSET_FLAGS(widget_.get(), GTK_CAN_FOCUS);

  gtk_widget_set_has_tooltip(widget_.get(), TRUE);
  g_signal_connect(widget_.get(), "query-tooltip",
                   G_CALLBACK(OnQueryTooltipThunk), this);

  hover_controller_.Init(widget());
  gtk_util::SetButtonTriggersNavigation(widget());

  if (theme_provider_) {
    theme_provider_->InitThemesFor(this);
    registrar_.Add(this,
                   NotificationType::BROWSER_THEME_CHANGED,
                   Source<GtkThemeProvider>(theme_provider_));
  }
}

ReloadButtonGtk::~ReloadButtonGtk() {
  widget_.Destroy();
}

void ReloadButtonGtk::ChangeMode(Mode mode, bool force) {
  intended_mode_ = mode;

  // If the change is forced, or the user isn't hovering the icon, or it's safe
  // to change it to the other image type, make the change immediately;
  // otherwise we'll let it happen later.
  if (force || GTK_WIDGET_STATE(widget()) == GTK_STATE_NORMAL ||
      ((mode == MODE_STOP) ?
        !timer_running() : (visible_mode_ != MODE_STOP))) {
    timer_.Stop();
    visible_mode_ = mode;
    gtk_widget_queue_draw(widget_.get());

    UpdateThemeButtons();
  }
}

void ReloadButtonGtk::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(NotificationType::BROWSER_THEME_CHANGED == type);

  GtkThemeProvider* provider = static_cast<GtkThemeProvider*>(
      Source<GtkThemeProvider>(source).ptr());
  DCHECK_EQ(provider, theme_provider_);
  GtkButtonWidth = 0;
  UpdateThemeButtons();
}

void ReloadButtonGtk::OnButtonTimer() {
  ChangeMode(intended_mode_, true);
}

gboolean ReloadButtonGtk::OnExpose(GtkWidget* widget,
                                   GdkEventExpose* e) {
  if (theme_provider_ && theme_provider_->UseGtkTheme())
    return FALSE;
  return ((visible_mode_ == MODE_RELOAD) ? reload_ : stop_).OnExpose(
      widget, e, hover_controller_.GetCurrentValue());
}

gboolean ReloadButtonGtk::OnLeaveNotify(GtkWidget* widget,
                                        GdkEventCrossing* event) {
  ChangeMode(intended_mode_, true);
  return FALSE;
}

void ReloadButtonGtk::OnClicked(GtkWidget* sender) {
  if (visible_mode_ == MODE_STOP) {
    if (browser_)
      browser_->Stop();

    // The user has clicked, so we can feel free to update the button,
    // even if the mouse is still hovering.
    ChangeMode(MODE_RELOAD, true);
  } else if (!timer_running()) {
    // Shift-clicking or Ctrl-clicking the reload button means we should ignore
    // any cached content.
    int command;
    GdkModifierType modifier_state;
    gtk_get_current_event_state(&modifier_state);
    guint modifier_state_uint = modifier_state;
    if (modifier_state_uint & GDK_SHIFT_MASK) {
      command = IDC_RELOAD_IGNORING_CACHE;
      // Mask off shift so it isn't interpreted as affecting the disposition
      // below.
      modifier_state_uint &= ~GDK_SHIFT_MASK;
    } else {
      command = IDC_RELOAD;
    }

    WindowOpenDisposition disposition =
        event_utils::DispositionFromEventFlags(modifier_state_uint);
    if (disposition == CURRENT_TAB) {
      // Forcibly reset the location bar, since otherwise it won't discard any
      // ongoing user edits, since it doesn't realize this is a user-initiated
      // action.
      location_bar_->Revert();
    }

    if (browser_)
      browser_->ExecuteCommandWithDisposition(command, disposition);

    // Figure out the system double-click time.
    if (button_delay_ == 0) {
      GtkSettings* settings = gtk_settings_get_default();
      g_object_get(G_OBJECT(settings), "gtk-double-click-time", &button_delay_,
                   NULL);
    }

    // Stop the timer.
    timer_.Stop();

    // Start a timer - while this timer is running, the reload button cannot be
    // changed to a stop button.  We do not set |intended_mode_| to MODE_STOP
    // here as we want to wait for the browser to tell us that it has started
    // loading (and this may occur only after some delay).
    timer_.Start(base::TimeDelta::FromMilliseconds(button_delay_), this,
                 &ReloadButtonGtk::OnButtonTimer);
  }
}

gboolean ReloadButtonGtk::OnQueryTooltip(GtkWidget* sender,
                                         gint x,
                                         gint y,
                                         gboolean keyboard_mode,
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
    GdkPixbuf* pixbuf = gtk_widget_render_icon(widget(),
        (intended_mode_ == MODE_RELOAD) ? GTK_STOCK_REFRESH : GTK_STOCK_STOP,
        GTK_ICON_SIZE_SMALL_TOOLBAR, NULL);

    gtk_button_set_image(GTK_BUTTON(widget_.get()),
                         gtk_image_new_from_pixbuf(pixbuf));
    g_object_unref(pixbuf);

    gtk_widget_set_size_request(widget_.get(), -1, -1);
    GtkRequisition req;
    gtk_widget_size_request(widget(), &req);
    GtkButtonWidth = std::max(GtkButtonWidth, req.width);
    gtk_widget_set_size_request(widget_.get(), GtkButtonWidth, -1);

    gtk_widget_set_app_paintable(widget_.get(), FALSE);
    gtk_widget_set_double_buffered(widget_.get(), TRUE);
  } else {
    gtk_widget_set_size_request(widget_.get(), reload_.Width(),
                                reload_.Height());

    gtk_widget_set_app_paintable(widget_.get(), TRUE);
    // We effectively double-buffer by virtue of having only one image...
    gtk_widget_set_double_buffered(widget_.get(), FALSE);
  }

  gtk_chrome_button_set_use_gtk_rendering(
      GTK_CHROME_BUTTON(widget_.get()), use_gtk);
}
