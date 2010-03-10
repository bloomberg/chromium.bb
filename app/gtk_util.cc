// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gtk_util.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace gtk_util {

void GetWidgetSizeFromResources(
    GtkWidget* widget, int width_chars, int height_lines,
    int* width, int* height) {
  DCHECK(GTK_WIDGET_REALIZED(widget))
      << " widget must be realized to compute font metrics correctly";

  double chars = 0;
  if (width)
    StringToDouble(l10n_util::GetStringUTF8(width_chars), &chars);

  double lines = 0;
  if (height)
    StringToDouble(l10n_util::GetStringUTF8(height_lines), &lines);

  GetWidgetSizeFromCharacters(widget, chars, lines, width, height);
}

void GetWidgetSizeFromCharacters(
    GtkWidget* widget, double width_chars, double height_lines,
    int* width, int* height) {
  DCHECK(GTK_WIDGET_REALIZED(widget))
      << " widget must be realized to compute font metrics correctly";
  PangoContext* context = gtk_widget_create_pango_context(widget);
  PangoFontMetrics* metrics = pango_context_get_metrics(context,
      widget->style->font_desc, pango_context_get_language(context));
  if (width) {
    *width = static_cast<int>(
        pango_font_metrics_get_approximate_char_width(metrics) *
        width_chars / PANGO_SCALE);
  }
  if (height) {
    *height = static_cast<int>(
        (pango_font_metrics_get_ascent(metrics) +
        pango_font_metrics_get_descent(metrics)) *
        height_lines / PANGO_SCALE);
  }
  pango_font_metrics_unref(metrics);
  g_object_unref(context);
}

void ApplyMessageDialogQuirks(GtkWidget* dialog) {
  if (gtk_window_get_modal(GTK_WINDOW(dialog))) {
    // Work around a KDE 3 window manager bug.
    scoped_ptr<base::EnvironmentVariableGetter> env(
        base::EnvironmentVariableGetter::Create());
    if (base::DESKTOP_ENVIRONMENT_KDE3 == GetDesktopEnvironment(env.get()))
      gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), FALSE);
  }
}

}  // namespace gtk_util
