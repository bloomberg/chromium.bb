// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/theme_install_bubble_view_gtk.h"

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "chrome/browser/gtk/rounded_window.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"

// Roundedness of bubble.
static const int kBubbleCornerRadius = 4;

// Padding between border of bubble and text.
static const int kTextPadding = 8;

// The bubble is partially transparent.
static const double kBubbleOpacity = static_cast<double>(0xcc) / 0xff;

ThemeInstallBubbleViewGtk* ThemeInstallBubbleViewGtk::instance_ = NULL;

// ThemeInstallBubbleViewGtk, public -------------------------------------------

// static
void ThemeInstallBubbleViewGtk::Show(GtkWindow* parent) {
  if (instance_)
    instance_->increment_num_loading();
  else
    instance_ = new ThemeInstallBubbleViewGtk(GTK_WIDGET(parent));
}

void ThemeInstallBubbleViewGtk::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  if (--num_loads_extant_ == 0)
    delete this;
}

// ThemeInstallBubbleViewGtk, private ------------------------------------------

ThemeInstallBubbleViewGtk::ThemeInstallBubbleViewGtk(GtkWidget* parent)
    : widget_(NULL),
      parent_(parent),
      num_loads_extant_(1) {
  InitWidgets();

  // Close when theme has been installed.
  registrar_.Add(
      this,
      NotificationType::BROWSER_THEME_CHANGED,
      NotificationService::AllSources());

  // Close when we are installing an extension, not a theme.
  registrar_.Add(
      this,
      NotificationType::NO_THEME_DETECTED,
      NotificationService::AllSources());
  registrar_.Add(
      this,
      NotificationType::EXTENSION_INSTALLED,
      NotificationService::AllSources());
}

ThemeInstallBubbleViewGtk::~ThemeInstallBubbleViewGtk() {
  gtk_widget_destroy(widget_);
  instance_ = NULL;
}

void ThemeInstallBubbleViewGtk::InitWidgets() {
  // Widgematically, the bubble is just a label in a popup window.
  widget_ = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_container_set_border_width(GTK_CONTAINER(widget_), kTextPadding);
  gtk_widget_modify_bg(widget_, GTK_STATE_NORMAL, &gfx::kGdkBlack);
  GtkWidget* label = gtk_label_new(NULL);

  // Need our own copy of the "Loading..." string: http://crbug.com/24177
  gchar* markup = g_markup_printf_escaped(
      "<span size='xx-large'>%s</span>",
      l10n_util::GetStringUTF8(IDS_TAB_LOADING_TITLE).c_str());
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);

  gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &gfx::kGdkWhite);
  gtk_container_add(GTK_CONTAINER(widget_), label);

  gtk_widget_realize(widget_);
  GdkColor color;
  gtk_util::ActAsRoundedWindow(widget_, color, kBubbleCornerRadius,
                               gtk_util::ROUNDED_ALL, gtk_util::BORDER_NONE);
  gtk_window_set_opacity(GTK_WINDOW(widget_), kBubbleOpacity);
  MoveWindow();

  g_signal_connect(widget_, "configure-event",
                   G_CALLBACK(&HandleParentConfigureThunk), this);
  g_signal_connect(widget_, "unmap-event",
                   G_CALLBACK(&HandleParentUnmapThunk), this);

  gtk_widget_show_all(widget_);
}


void ThemeInstallBubbleViewGtk::MoveWindow() {
  GtkRequisition req;
  gtk_widget_size_request(widget_, &req);

  gint parent_x = 0, parent_y = 0;
  gdk_window_get_position(parent_->window, &parent_x, &parent_y);
  gint parent_width = parent_->allocation.width;
  gint parent_height = parent_->allocation.height;

  gint x = parent_x + parent_width / 2 - req.width / 2;
  gint y = parent_y + parent_height / 2 - req.height / 2;

  gtk_window_move(GTK_WINDOW(widget_), x, y);
}

gboolean ThemeInstallBubbleViewGtk::HandleParentConfigure(
    GdkEventConfigure* event) {
  MoveWindow();
  return FALSE;
}

gboolean ThemeInstallBubbleViewGtk::HandleParentUnmap() {
  delete this;
  return FALSE;
}
