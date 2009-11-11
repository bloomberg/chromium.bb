// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/gtk_chrome_link_button.h"

#include <stdlib.h>

#include "base/logging.h"
#include "chrome/common/gtk_util.h"

static const gchar* kLinkMarkup = "<u><span color=\"%s\">%s</span></u>";

namespace {

// Set the GTK style on our custom link button. We don't want any border around
// the link text.
void SetLinkButtonStyle() {
  static bool style_was_set = false;

  if (style_was_set)
    return;
  style_was_set = true;

  gtk_rc_parse_string(
      "style \"chrome-link-button\" {"
      "  GtkButton::inner-border = {0, 0, 0, 0}"
      "  GtkButton::child-displacement-x = 0"
      "  GtkButton::child-displacement-y = 0"
      "  xthickness = 0"
      "  ythickness = 0"
      "}"
      "widget_class \"*.<GtkChromeLinkButton>\" style \"chrome-link-button\"");
}

}  // namespace

G_BEGIN_DECLS
G_DEFINE_TYPE(GtkChromeLinkButton, gtk_chrome_link_button, GTK_TYPE_BUTTON)

// Should be called after we are realized so that the "link-color" property
// can be read.
static void gtk_chrome_link_button_set_text(GtkChromeLinkButton* button) {
  // We only set the markup once.
  if (button->blue_markup)
    return;

  gchar* text = button->text;
  gboolean uses_markup = button->uses_markup;

  if (!uses_markup) {
    button->blue_markup = g_markup_printf_escaped(kLinkMarkup, "blue", text);
    button->red_markup = g_markup_printf_escaped(kLinkMarkup, "red", text);
  } else {
    button->blue_markup = static_cast<gchar*>(
        g_malloc(strlen(kLinkMarkup) + strlen("blue") + strlen(text) + 1));
    sprintf(button->blue_markup, kLinkMarkup, "blue", text);

    button->red_markup = static_cast<gchar*>(
        g_malloc(strlen(kLinkMarkup) + strlen("red") + strlen(text) + 1));
    sprintf(button->red_markup, kLinkMarkup, "red", text);
  }

  // Get the current GTK theme's link button text color.
  GdkColor* native_color = NULL;
  gtk_widget_style_get(GTK_WIDGET(button), "link-color", &native_color, NULL);

  if (native_color) {
    gchar color_spec[9];
    sprintf(color_spec, "#%02X%02X%02X", native_color->red / 257,
            native_color->green / 257, native_color->blue / 257);
    gdk_color_free(native_color);

    if (!uses_markup) {
      button->native_markup = g_markup_printf_escaped(kLinkMarkup,
          color_spec, text);
    } else {
      button->native_markup = static_cast<gchar*>(
          g_malloc(strlen(kLinkMarkup) + strlen(color_spec) + strlen(text) +
                   1));
      sprintf(button->native_markup, kLinkMarkup, color_spec, text);
    }
  } else {
    // If the theme doesn't have a link color, just use blue. This matches the
    // default for GtkLinkButton.
    button->native_markup = button->blue_markup;
  }

  gtk_label_set_markup(GTK_LABEL(button->label),
      button->using_native_theme ? button->native_markup : button->blue_markup);
}

static gboolean gtk_chrome_link_button_expose(GtkWidget* widget,
                                              GdkEventExpose* event) {
  GtkChromeLinkButton* button = GTK_CHROME_LINK_BUTTON(widget);
  GtkWidget* label = button->label;

  gtk_chrome_link_button_set_text(button);

  if (GTK_WIDGET_STATE(widget) == GTK_STATE_ACTIVE && button->is_blue) {
    gtk_label_set_markup(GTK_LABEL(label), button->red_markup);
    button->is_blue = FALSE;
  } else if (GTK_WIDGET_STATE(widget) != GTK_STATE_ACTIVE && !button->is_blue) {
    gtk_label_set_markup(GTK_LABEL(label),
        button->using_native_theme ? button->native_markup :
                                     button->blue_markup);
    button->is_blue = TRUE;
  }

  // Draw the link inside the button.
  gtk_container_propagate_expose(GTK_CONTAINER(widget), label, event);

  // Draw the focus rectangle.
  if (GTK_WIDGET_HAS_FOCUS(widget)) {
    gtk_paint_focus(widget->style, widget->window,
                    static_cast<GtkStateType>(GTK_WIDGET_STATE(widget)),
                    &event->area, widget, NULL,
                    widget->allocation.x, widget->allocation.y,
                    widget->allocation.width, widget->allocation.height);
  }

  return TRUE;
}

static void gtk_chrome_link_button_enter(GtkButton* button) {
  GtkWidget* widget = GTK_WIDGET(button);
  GtkChromeLinkButton* link_button = GTK_CHROME_LINK_BUTTON(button);
  gdk_window_set_cursor(widget->window, link_button->hand_cursor);
}

static void gtk_chrome_link_button_leave(GtkButton* button) {
  GtkWidget* widget = GTK_WIDGET(button);
  gdk_window_set_cursor(widget->window, NULL);
}

static void gtk_chrome_link_button_destroy(GtkObject* object) {
  GtkChromeLinkButton* button = GTK_CHROME_LINK_BUTTON(object);
  if (button->native_markup && (button->native_markup != button->blue_markup))
    g_free(button->native_markup);
  button->native_markup = NULL;
  if (button->blue_markup) {
    g_free(button->blue_markup);
    button->blue_markup = NULL;
  }
  if (button->red_markup) {
    g_free(button->red_markup);
    button->red_markup = NULL;
  }
  if (button->hand_cursor) {
    gdk_cursor_unref(button->hand_cursor);
    button->hand_cursor = NULL;
  }

  free(button->text);
  button->text = NULL;

  GTK_OBJECT_CLASS(gtk_chrome_link_button_parent_class)->destroy(object);
}

static void gtk_chrome_link_button_class_init(
    GtkChromeLinkButtonClass* link_button_class) {
  GtkWidgetClass* widget_class =
      reinterpret_cast<GtkWidgetClass*>(link_button_class);
  GtkButtonClass* button_class =
      reinterpret_cast<GtkButtonClass*>(link_button_class);
  GtkObjectClass* object_class =
      reinterpret_cast<GtkObjectClass*>(link_button_class);
  widget_class->expose_event = &gtk_chrome_link_button_expose;
  button_class->enter = &gtk_chrome_link_button_enter;
  button_class->leave = &gtk_chrome_link_button_leave;
  object_class->destroy = &gtk_chrome_link_button_destroy;
}

static void gtk_chrome_link_button_init(GtkChromeLinkButton* button) {
  SetLinkButtonStyle();

  // We put a label in a button so we can connect to the click event. We don't
  // let the button draw itself; catch all expose events to the button and pass
  // them through to the label.
  button->label = gtk_label_new(NULL);
  button->blue_markup = NULL;
  button->red_markup = NULL;
  button->is_blue = TRUE;
  button->native_markup = NULL;
  button->using_native_theme = TRUE;
  button->hand_cursor = gtk_util::GetCursor(GDK_HAND2);
  button->text = NULL;

  gtk_container_add(GTK_CONTAINER(button), button->label);
  gtk_widget_set_app_paintable(GTK_WIDGET(button), TRUE);
}

GtkWidget* gtk_chrome_link_button_new(const char* text) {
  GtkWidget* lb = GTK_WIDGET(g_object_new(GTK_TYPE_CHROME_LINK_BUTTON, NULL));
  GTK_CHROME_LINK_BUTTON(lb)->text = strdup(text);
  GTK_CHROME_LINK_BUTTON(lb)->uses_markup = FALSE;
  return lb;
}

GtkWidget* gtk_chrome_link_button_new_with_markup(const char* markup) {
  GtkWidget* lb = GTK_WIDGET(g_object_new(GTK_TYPE_CHROME_LINK_BUTTON, NULL));
  GTK_CHROME_LINK_BUTTON(lb)->text = strdup(markup);
  GTK_CHROME_LINK_BUTTON(lb)->uses_markup = TRUE;
  return lb;
}

void gtk_chrome_link_button_set_use_gtk_theme(GtkChromeLinkButton* button,
                                              gboolean use_gtk) {
  if (use_gtk != button->using_native_theme) {
    button->using_native_theme = use_gtk;
    if (GTK_WIDGET_VISIBLE(button))
      gtk_widget_queue_draw(GTK_WIDGET(button));
  }
}

void gtk_chrome_link_button_set_label(GtkChromeLinkButton* button,
                                      const char* text) {
  if (button->text) {
    free(button->text);
  }
  button->text = strdup(text);

  // Clear the markup so we can redraw.
  if (button->native_markup && (button->native_markup != button->blue_markup))
    g_free(button->native_markup);
  button->native_markup = NULL;
  if (button->blue_markup) {
    g_free(button->blue_markup);
    button->blue_markup = NULL;
  }
  if (button->red_markup) {
    g_free(button->red_markup);
    button->red_markup = NULL;
  }

  if (GTK_WIDGET_VISIBLE(button))
    gtk_widget_queue_draw(GTK_WIDGET(button));
}

G_END_DECLS
