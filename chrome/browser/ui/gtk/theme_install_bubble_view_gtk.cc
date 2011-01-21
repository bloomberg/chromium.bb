// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/theme_install_bubble_view_gtk.h"

#include <math.h>

#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/rounded_window.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

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
  registrar_.Add(
      this,
      NotificationType::EXTENSION_INSTALL_ERROR,
      NotificationService::AllSources());

  // Don't let the bubble overlap the confirm dialog.
  registrar_.Add(
      this,
      NotificationType::EXTENSION_WILL_SHOW_CONFIRM_DIALOG,
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
  GtkWidget* label = gtk_label_new(NULL);

  gchar* markup = g_markup_printf_escaped(
      "<span size='xx-large'>%s</span>",
      l10n_util::GetStringUTF8(IDS_THEME_LOADING_TITLE).c_str());
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);

  gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &gtk_util::kGdkWhite);
  gtk_container_add(GTK_CONTAINER(widget_), label);

  // We need to show the label so we'll know the widget's actual size when we
  // call MoveWindow().
  gtk_widget_show_all(label);

  bool composited = false;
  if (gtk_util::IsScreenComposited()) {
    composited = true;
    GdkScreen* screen = gtk_widget_get_screen(widget_);
    GdkColormap* colormap = gdk_screen_get_rgba_colormap(screen);

    if (colormap)
      gtk_widget_set_colormap(widget_, colormap);
    else
      composited = false;
  }

  if (composited) {
    gtk_widget_set_app_paintable(widget_, TRUE);
    g_signal_connect(widget_, "expose-event",
                     G_CALLBACK(OnExposeThunk), this);
    gtk_widget_realize(widget_);
  } else {
    gtk_widget_modify_bg(widget_, GTK_STATE_NORMAL, &gtk_util::kGdkBlack);
    GdkColor color;
    gtk_util::ActAsRoundedWindow(widget_, color, kBubbleCornerRadius,
                                 gtk_util::ROUNDED_ALL, gtk_util::BORDER_NONE);
  }

  MoveWindow();

  g_signal_connect(widget_, "unmap-event",
                   G_CALLBACK(OnUnmapEventThunk), this);

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

gboolean ThemeInstallBubbleViewGtk::OnUnmapEvent(GtkWidget* widget) {
  delete this;
  return FALSE;
}

gboolean ThemeInstallBubbleViewGtk::OnExpose(GtkWidget* widget,
                                             GdkEventExpose* event) {
  cairo_t* cr = gdk_cairo_create(event->window);
  gdk_cairo_rectangle(cr, &event->area);
  cairo_clip(cr);

  cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

  // |inner_rect| has its corners at the centerpoints of the corner arcs.
  gfx::Rect inner_rect(widget_->allocation);
  int inset = kBubbleCornerRadius;
  inner_rect.Inset(inset, inset);

  // The positive y axis is down, so M_PI_2 is down.
  cairo_arc(cr, inner_rect.x(), inner_rect.y(), inset,
            M_PI, 3 * M_PI_2);
  cairo_arc(cr, inner_rect.right(), inner_rect.y(), inset,
            3 * M_PI_2, 0);
  cairo_arc(cr, inner_rect.right(), inner_rect.bottom(), inset,
            0, M_PI_2);
  cairo_arc(cr, inner_rect.x(), inner_rect.bottom(), inset,
            M_PI_2, M_PI);

  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, kBubbleOpacity);
  cairo_fill(cr);
  cairo_destroy(cr);

  return FALSE;
}
