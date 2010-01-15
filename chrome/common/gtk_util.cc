// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gtk_util.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <cstdarg>
#include <map>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "chrome/browser/gtk/cairo_cached_surface.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/common/renderer_preferences.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

// How large a font needs to be before we override the font family.
const double kTooLargeFontSize = 17.5;

// Callback used in RemoveAllChildren.
void RemoveWidget(GtkWidget* widget, gpointer container) {
  gtk_container_remove(GTK_CONTAINER(container), widget);
}

// These two functions are copped almost directly from gtk core. The only
// difference is that they accept middle clicks.
gboolean OnMouseButtonPressed(GtkWidget* widget, GdkEventButton* event,
                              gpointer userdata) {
  if (event->type == GDK_BUTTON_PRESS) {
    if (gtk_button_get_focus_on_click(GTK_BUTTON(widget)) &&
        !GTK_WIDGET_HAS_FOCUS(widget)) {
      gtk_widget_grab_focus(widget);
    }

    gint button_mask = GPOINTER_TO_INT(userdata);
    if (button_mask && (1 << event->button))
      gtk_button_pressed(GTK_BUTTON(widget));
  }

  return TRUE;
}

gboolean OnMouseButtonReleased(GtkWidget* widget, GdkEventButton* event,
                               gpointer userdata) {
  gint button_mask = GPOINTER_TO_INT(userdata);
  if (button_mask && (1 << event->button))
    gtk_button_released(GTK_BUTTON(widget));

  return TRUE;
}

// Ownership of |icon_list| is passed to the caller.
GList* GetIconList() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GList* icon_list = NULL;
  icon_list = g_list_append(icon_list, rb.GetPixbufNamed(IDR_PRODUCT_ICON_32));
  icon_list = g_list_append(icon_list, rb.GetPixbufNamed(IDR_PRODUCT_LOGO_16));
  return icon_list;
}

// A process wide singleton that manages our usage of gdk
// cursors. gdk_cursor_new() hits the disk in several places and GdkCursor
// instances can be reused throughout the process.
class GdkCursorCache {
 public:
  ~GdkCursorCache() {
    for (std::map<GdkCursorType, GdkCursor*>::iterator it =
             cursor_cache_.begin(); it != cursor_cache_.end(); ++it) {
      gdk_cursor_unref(it->second);
    }
    cursor_cache_.clear();
  }

  GdkCursor* GetCursorImpl(GdkCursorType type) {
    std::map<GdkCursorType, GdkCursor*>::iterator it = cursor_cache_.find(type);
    GdkCursor* cursor = NULL;
    if (it == cursor_cache_.end()) {
      cursor = gdk_cursor_new(type);
      cursor_cache_.insert(std::make_pair(type, cursor));
    } else {
      cursor = it->second;
    }

    // Add a reference to the returned cursor because our consumers mix us with
    // gdk_cursor_new(). Both the normal constructor and GetCursorImpls() need
    // to be paired with a gdk_cursor_unref() so ref it here (as we own the ref
    // that comes from gdk_cursor_new().
    gdk_cursor_ref(cursor);

    return cursor;
  }

  std::map<GdkCursorType, GdkCursor*> cursor_cache_;
};

// Expose event handler for a container that simply suppresses the default
// drawing and propagates the expose event to the container's children.
gboolean PaintNoBackground(GtkWidget* widget,
                           GdkEventExpose* event,
                           gpointer unused) {
  GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
  for (GList* item = children; item; item = item->next) {
    gtk_container_propagate_expose(GTK_CONTAINER(widget),
                                   GTK_WIDGET(item->data),
                                   event);
  }
  g_list_free(children);

  return TRUE;
}

void OnLabelAllocate(GtkWidget* label, GtkAllocation* allocation,
                     gpointer user_data) {
  gtk_widget_set_size_request(label, allocation->width, -1);
}

}  // namespace

namespace event_utils {

WindowOpenDisposition DispositionFromEventFlags(guint event_flags) {
  if ((event_flags & GDK_BUTTON2_MASK) || (event_flags & GDK_CONTROL_MASK)) {
    return (event_flags & GDK_SHIFT_MASK) ?
        NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
  }

  if (event_flags & GDK_SHIFT_MASK)
    return NEW_WINDOW;
  return false /*event.IsAltDown()*/ ? SAVE_TO_DISK : CURRENT_TAB;
}

}  // namespace event_utils

namespace gtk_util {

GtkWidget* CreateLabeledControlsGroup(std::vector<GtkWidget*>* labels,
                                      const char* text, ...) {
  va_list ap;
  va_start(ap, text);
  GtkWidget* table = gtk_table_new(0, 2, FALSE);
  gtk_table_set_col_spacing(GTK_TABLE(table), 0, kLabelSpacing);
  gtk_table_set_row_spacings(GTK_TABLE(table), kControlSpacing);

  for (guint row = 0; text; ++row) {
    gtk_table_resize(GTK_TABLE(table), row + 1, 2);
    GtkWidget* control = va_arg(ap, GtkWidget*);
    GtkWidget* label = gtk_label_new(text);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    if (labels)
      labels->push_back(label);

    gtk_table_attach(GTK_TABLE(table), label,
                 0, 1, row, row + 1,
                 GTK_FILL, GTK_FILL,
                 0, 0);
    gtk_table_attach_defaults(GTK_TABLE(table), control,
                              1, 2, row, row + 1);
    text = va_arg(ap, const char*);
  }
  va_end(ap);

  return table;
}

GtkWidget* CreateGtkBorderBin(GtkWidget* child, const GdkColor* color,
                              int top, int bottom, int left, int right) {
  // Use a GtkEventBox to get the background painted.  However, we can't just
  // use a container border, since it won't paint there.  Use an alignment
  // inside to get the sizes exactly of how we want the border painted.
  GtkWidget* ebox = gtk_event_box_new();
  if (color)
    gtk_widget_modify_bg(ebox, GTK_STATE_NORMAL, color);
  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), top, bottom, left, right);
  gtk_container_add(GTK_CONTAINER(alignment), child);
  gtk_container_add(GTK_CONTAINER(ebox), alignment);
  return ebox;
}

void GetWidgetSizeFromResources(GtkWidget* widget, int width_chars,
                                int height_lines, int* width, int* height) {
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

void GetWidgetSizeFromCharacters(GtkWidget* widget, double width_chars,
                                 double height_lines, int* width, int* height) {
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

void SetWindowSizeFromResources(GtkWindow* window,
                                int width_id, int height_id, bool resizable) {
  int width = -1;
  int height = -1;
  gtk_util::GetWidgetSizeFromResources(GTK_WIDGET(window), width_id, height_id,
                                       (width_id != -1) ? &width : NULL,
                                       (height_id != -1) ? &height : NULL);

  if (resizable) {
    gtk_window_set_default_size(window, width, height);
  } else {
    gtk_widget_set_size_request(GTK_WIDGET(window), width, height);
  }
  gtk_window_set_resizable(window, resizable ? TRUE : FALSE);
}

void RemoveAllChildren(GtkWidget* container) {
  gtk_container_foreach(GTK_CONTAINER(container), RemoveWidget, container);
}

void ForceFontSizePixels(GtkWidget* widget, double size_pixels) {
  GtkStyle* style = widget->style;
  PangoFontDescription* font_desc = style->font_desc;

  // pango_font_description_set_absolute_size sets the font size in device
  // units, which for us is pixels.
  pango_font_description_set_absolute_size(font_desc,
                                           PANGO_SCALE * size_pixels);
  gtk_widget_modify_font(widget, font_desc);

  // Check to make sure that the system font is sane.
  PangoContext* context = gtk_widget_create_pango_context(widget);
  PangoFontMetrics* metrics = pango_context_get_metrics(context,
      widget->style->font_desc, pango_context_get_language(context));
  double over_under = (pango_font_metrics_get_ascent(metrics) +
                       pango_font_metrics_get_descent(metrics)) / PANGO_SCALE;
  pango_font_metrics_unref(metrics);
  g_object_unref(context);

  // It would be awesome if we could override with better metrics, but I can't
  // think of any other than the sum of the ascent/descent that make sense.
  if (over_under > kTooLargeFontSize) {
    // Override the system font. The user's font would lead to the "jumping up
    // and down" text effect that happens because we're setting the font size
    // with a font with ridiculously large ascent/descents. This is, in the
    // end, our fault because of our abuse of gtk_widget_set_size_request(),
    // but for now we'll just override the user's font since it'll take a while
    // to fix the interface proper.
    style = widget->style;
    font_desc = style->font_desc;
    pango_font_description_set_family_static(font_desc, "Sans");
    pango_font_description_set_absolute_size(font_desc,
                                             PANGO_SCALE * size_pixels);
    gtk_widget_modify_font(widget, font_desc);
  }
}

gfx::Point GetWidgetScreenPosition(GtkWidget* widget) {
  if (!widget->window) {
    NOTREACHED() << "Must only be called on realized widgets.";
    return gfx::Point(0, 0);
  }

  gint x, y;
  gdk_window_get_origin(widget->window, &x, &y);

  if (!GTK_IS_WINDOW(widget)) {
    x += widget->allocation.x;
    y += widget->allocation.y;
  }

  return gfx::Point(x, y);
}

gfx::Rect GetWidgetScreenBounds(GtkWidget* widget) {
  gfx::Point position = GetWidgetScreenPosition(widget);
  return gfx::Rect(position.x(), position.y(),
                   widget->allocation.width, widget->allocation.height);
}

void ConvertWidgetPointToScreen(GtkWidget* widget, gfx::Point* p) {
  DCHECK(widget);
  DCHECK(p);

  gfx::Point position = GetWidgetScreenPosition(widget);
  p->SetPoint(p->x() + position.x(), p->y() + position.y());
}

void InitRCStyles() {
  static const char kRCText[] =
      // Make our dialogs styled like the GNOME HIG.
      //
      // TODO(evanm): content-area-spacing was introduced in a later
      // version of GTK, so we need to set that manually on all dialogs.
      // Perhaps it would make sense to have a shared FixupDialog() function.
      "style \"gnome-dialog\" {\n"
      "  xthickness = 12\n"
      "  GtkDialog::action-area-border = 0\n"
      "  GtkDialog::button-spacing = 6\n"
      "  GtkDialog::content-area-spacing = 18\n"
      "  GtkDialog::content-area-border = 12\n"
      "}\n"
      // Note we set it at the "application" priority, so users can override.
      "widget \"GtkDialog\" style : application \"gnome-dialog\"\n"

      // Make our about dialog special, so the image is flush with the edge.
      "style \"about-dialog\" {\n"
      "  GtkDialog::action-area-border = 12\n"
      "  GtkDialog::button-spacing = 6\n"
      "  GtkDialog::content-area-spacing = 18\n"
      "  GtkDialog::content-area-border = 0\n"
      "}\n"
      "widget \"about-dialog\" style : application \"about-dialog\"\n";

  gtk_rc_parse_string(kRCText);
}

void CenterWidgetInHBox(GtkWidget* hbox, GtkWidget* widget, bool pack_at_end,
                        int padding) {
  GtkWidget* centering_vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(centering_vbox), widget, TRUE, FALSE, 0);
  if (pack_at_end)
    gtk_box_pack_end(GTK_BOX(hbox), centering_vbox, FALSE, FALSE, padding);
  else
    gtk_box_pack_start(GTK_BOX(hbox), centering_vbox, FALSE, FALSE, padding);
}

std::string ConvertAcceleratorsFromWindowsStyle(const std::string& label) {
  std::string ret;
  ret.reserve(label.length() * 2);
  for (size_t i = 0; i < label.length(); ++i) {
    if ('_' == label[i]) {
      ret.push_back('_');
      ret.push_back('_');
    } else if ('&' == label[i]) {
      if (i + 1 < label.length() && '&' == label[i + 1]) {
        ret.push_back(label[i]);
        ++i;
      } else {
        ret.push_back('_');
      }
    } else {
      ret.push_back(label[i]);
    }
  }

  return ret;
}

bool IsScreenComposited() {
  GdkScreen* screen = gdk_screen_get_default();
  return gdk_screen_is_composited(screen) == TRUE;
}

void EnumerateTopLevelWindows(x11_util::EnumerateWindowsDelegate* delegate) {
  std::vector<XID> stack;
  if (!x11_util::GetXWindowStack(&stack)) {
    // Window Manager doesn't support _NET_CLIENT_LIST_STACKING, so fall back
    // to old school enumeration of all X windows.  Some WMs parent 'top-level'
    // windows in unnamed actual top-level windows (ion WM), so extend the
    // search depth to all children of top-level windows.
    const int kMaxSearchDepth = 1;
    x11_util::EnumerateAllWindows(delegate, kMaxSearchDepth);
    return;
  }

  std::vector<XID>::iterator iter;
  for (iter = stack.begin(); iter != stack.end(); iter++) {
    if (delegate->ShouldStopIterating(*iter))
      return;
  }
}

void SetButtonClickableByMouseButtons(GtkWidget* button,
                                      bool left, bool middle, bool right) {
  gint button_mask = 0;
  if (left)
    button_mask |= 1 << 1;
  if (middle)
    button_mask |= 1 << 2;
  if (right)
    button_mask |= 1 << 3;
  void* userdata = GINT_TO_POINTER(button_mask);

  g_signal_connect(G_OBJECT(button), "button-press-event",
                   G_CALLBACK(OnMouseButtonPressed), userdata);
  g_signal_connect(G_OBJECT(button), "button-release-event",
                   G_CALLBACK(OnMouseButtonReleased), userdata);
}

void SetButtonTriggersNavigation(GtkWidget* button) {
  SetButtonClickableByMouseButtons(button, true, true, false);
}

int MirroredLeftPointForRect(GtkWidget* widget, const gfx::Rect& bounds) {
  if (l10n_util::GetTextDirection() != l10n_util::RIGHT_TO_LEFT) {
    return bounds.x();
  }
  return widget->allocation.width - bounds.x() - bounds.width();
}

int MirroredXCoordinate(GtkWidget* widget, int x) {
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
    return widget->allocation.width - x;
  }
  return x;
}

bool WidgetContainsCursor(GtkWidget* widget) {
  gint x = 0;
  gint y = 0;
  gtk_widget_get_pointer(widget, &x, &y);

  // To quote the gtk docs:
  //
  //   Widget coordinates are a bit odd; for historical reasons, they are
  //   defined as widget->window coordinates for widgets that are not
  //   GTK_NO_WINDOW widgets, and are relative to widget->allocation.x,
  //   widget->allocation.y for widgets that are GTK_NO_WINDOW widgets.
  //
  // So the base is always (0,0).
  gfx::Rect widget_allocation(0, 0, widget->allocation.width,
                              widget->allocation.height);
  return widget_allocation.Contains(x, y);
}

void SetWindowIcon(GtkWindow* window) {
  GList* icon_list = GetIconList();
  gtk_window_set_icon_list(window, icon_list);
  g_list_free(icon_list);
}

void SetDefaultWindowIcon() {
  GList* icon_list = GetIconList();
  gtk_window_set_default_icon_list(icon_list);
  g_list_free(icon_list);
}

GtkWidget* AddButtonToDialog(GtkWidget* dialog, const gchar* text,
                             const gchar* stock_id, gint response_id) {
  GtkWidget* button = gtk_button_new_with_label(text);
  gtk_button_set_image(GTK_BUTTON(button),
                       gtk_image_new_from_stock(stock_id,
                                                GTK_ICON_SIZE_BUTTON));
  gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button,
                               response_id);
  return button;
}

void SetLabelColor(GtkWidget* label, const GdkColor* color) {
  gtk_widget_modify_fg(label, GTK_STATE_NORMAL, color);
  gtk_widget_modify_fg(label, GTK_STATE_ACTIVE, color);
  gtk_widget_modify_fg(label, GTK_STATE_PRELIGHT, color);
  gtk_widget_modify_fg(label, GTK_STATE_INSENSITIVE, color);
}

GtkWidget* IndentWidget(GtkWidget* content) {
  GtkWidget* content_alignment = gtk_alignment_new(0.0, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(content_alignment), 0, 0,
                            gtk_util::kGroupIndent, 0);
  gtk_container_add(GTK_CONTAINER(content_alignment), content);
  return content_alignment;
}

void UpdateGtkFontSettings(RendererPreferences* prefs) {
  DCHECK(prefs);

  gint antialias = 0;
  gint hinting = 0;
  gchar* hint_style = NULL;
  gchar* rgba_style = NULL;
  g_object_get(gtk_settings_get_default(),
               "gtk-xft-antialias", &antialias,
               "gtk-xft-hinting", &hinting,
               "gtk-xft-hintstyle", &hint_style,
               "gtk-xft-rgba", &rgba_style,
               NULL);

  // Set some reasonable defaults.
  prefs->should_antialias_text = true;
  prefs->hinting = RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT;
  prefs->subpixel_rendering =
      RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT;

  // g_object_get() doesn't tell us whether the properties were present or not,
  // but if they aren't (because gnome-settings-daemon isn't running), we'll get
  // NULL values for the strings.
  if (hint_style && rgba_style) {
    prefs->should_antialias_text = antialias;

    if (hinting == 0 || strcmp(hint_style, "hintnone") == 0) {
      prefs->hinting = RENDERER_PREFERENCES_HINTING_NONE;
    } else if (strcmp(hint_style, "hintslight") == 0) {
      prefs->hinting = RENDERER_PREFERENCES_HINTING_SLIGHT;
    } else if (strcmp(hint_style, "hintmedium") == 0) {
      prefs->hinting = RENDERER_PREFERENCES_HINTING_MEDIUM;
    } else if (strcmp(hint_style, "hintfull") == 0) {
      prefs->hinting = RENDERER_PREFERENCES_HINTING_FULL;
    }

    if (strcmp(rgba_style, "none") == 0) {
      prefs->subpixel_rendering = RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE;
    } else if (strcmp(rgba_style, "rgb") == 0) {
      prefs->subpixel_rendering = RENDERER_PREFERENCES_SUBPIXEL_RENDERING_RGB;
    } else if (strcmp(rgba_style, "bgr") == 0) {
      prefs->subpixel_rendering = RENDERER_PREFERENCES_SUBPIXEL_RENDERING_BGR;
    } else if (strcmp(rgba_style, "vrgb") == 0) {
      prefs->subpixel_rendering = RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VRGB;
    } else if (strcmp(rgba_style, "vbgr") == 0) {
      prefs->subpixel_rendering = RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VBGR;
    }
  }

  if (hint_style)
    g_free(hint_style);
  if (rgba_style)
    g_free(rgba_style);
}

gfx::Point ScreenPoint(GtkWidget* widget) {
  int x, y;
  gdk_display_get_pointer(gtk_widget_get_display(widget), NULL, &x, &y,
                          NULL);
  return gfx::Point(x, y);
}

gfx::Point ClientPoint(GtkWidget* widget) {
  int x, y;
  gtk_widget_get_pointer(widget, &x, &y);
  return gfx::Point(x, y);
}

GdkPoint MakeBidiGdkPoint(gint x, gint y, gint width, bool ltr) {
  GdkPoint point = {ltr ? x : width - x, y};
  return point;
}

void DrawTextEntryBackground(GtkWidget* offscreen_entry,
                             GtkWidget* widget_to_draw_on,
                             GdkRectangle* dirty_rec,
                             GdkRectangle* rec) {
  GtkStyle* gtk_owned_style = gtk_rc_get_style(offscreen_entry);
  // GTK owns the above and we're going to have to make our own copy of it
  // that we can edit.
  GtkStyle* our_style = gtk_style_copy(gtk_owned_style);
  our_style = gtk_style_attach(our_style, widget_to_draw_on->window);

  // TODO(erg): Draw the focus ring if appropriate...

  // We're using GTK rendering; draw a GTK entry widget onto the background.
  gtk_paint_shadow(our_style, widget_to_draw_on->window,
                   GTK_STATE_NORMAL, GTK_SHADOW_IN, dirty_rec,
                   widget_to_draw_on, "entry",
                   rec->x, rec->y, rec->width, rec->height);

  // Draw the interior background (not all themes draw the entry background
  // above; this is a noop on themes that do).
  gint xborder = our_style->xthickness;
  gint yborder = our_style->ythickness;
  gtk_paint_flat_box(our_style, widget_to_draw_on->window,
                     GTK_STATE_NORMAL, GTK_SHADOW_NONE, dirty_rec,
                     widget_to_draw_on, "entry_bg",
                     rec->x + xborder, rec->y + yborder,
                     rec->width - 2 * xborder,
                     rec->height - 2 * yborder);

  g_object_unref(our_style);
}

void DrawThemedToolbarBackground(GtkWidget* widget,
                                 cairo_t* cr,
                                 GdkEventExpose* event,
                                 const gfx::Point& tabstrip_origin,
                                 GtkThemeProvider* theme_provider) {
  // Fill the entire region with the toolbar color.
  GdkColor color = theme_provider->GetGdkColor(
      BrowserThemeProvider::COLOR_TOOLBAR);
  gdk_cairo_set_source_color(cr, &color);
  cairo_fill(cr);

  // The toolbar is supposed to blend in with the active tab, so we have to pass
  // coordinates for the IDR_THEME_TOOLBAR bitmap relative to the top of the
  // tab strip.
  CairoCachedSurface* background = theme_provider->GetSurfaceNamed(
      IDR_THEME_TOOLBAR, widget);
  background->SetSource(cr, tabstrip_origin.x(), tabstrip_origin.y());
  // We tile the toolbar background in both directions.
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
  cairo_rectangle(cr,
                  tabstrip_origin.x(),
                  tabstrip_origin.y(),
                  event->area.x + event->area.width - tabstrip_origin.x(),
                  event->area.y + event->area.height - tabstrip_origin.y());
  cairo_fill(cr);
}

GdkColor AverageColors(GdkColor color_one, GdkColor color_two) {
  GdkColor average_color;
  average_color.pixel = 0;
  average_color.red = (color_one.red + color_two.red) / 2;
  average_color.green = (color_one.green + color_two.green) / 2;
  average_color.blue = (color_one.blue + color_two.blue) / 2;
  return average_color;
}

void SetAlwaysShowImage(GtkWidget* image_menu_item) {
  // Compile time check: if it's available, just use the API.
  // GTK_CHECK_VERSION is TRUE if the passed version is compatible.
#if GTK_CHECK_VERSION(2, 16, 1)
  gtk_image_menu_item_set_always_show_image(
      GTK_IMAGE_MENU_ITEM(image_menu_item), TRUE);
#else
  // Run time check: if the API is not available, set the property manually.
  // This will still only work with GTK 2.16+ as the property doesn't exist
  // in earlier versions.
  // gtk_check_version() returns NULL if the passed version is compatible.
  if (!gtk_check_version(2, 16, 1)) {
    GValue true_value = { 0 };
    g_value_init(&true_value, G_TYPE_BOOLEAN);
    g_value_set_boolean(&true_value, TRUE);
    g_object_set_property(G_OBJECT(image_menu_item), "always-show-image",
                          &true_value);
  }
#endif
}

GdkCursor* GetCursor(GdkCursorType type) {
  static GdkCursorCache impl;
  return impl.GetCursorImpl(type);
}

void StackPopupWindow(GtkWidget* popup, GtkWidget* toplevel) {
  DCHECK(GTK_IS_WINDOW(popup) && GTK_WIDGET_TOPLEVEL(popup) &&
         GTK_WIDGET_REALIZED(popup));
  DCHECK(GTK_IS_WINDOW(toplevel) && GTK_WIDGET_TOPLEVEL(toplevel) &&
         GTK_WIDGET_REALIZED(toplevel));

  // Stack the |popup| window directly above the |toplevel| window.
  // The |popup| window is a direct child of the root window, so we need to
  // find a similar ancestor for the toplevel window (which might have been
  // reparented by a window manager).
  XID toplevel_window_base = x11_util::GetHighestAncestorWindow(
      x11_util::GetX11WindowFromGtkWidget(toplevel),
      x11_util::GetX11RootWindow());
  if (toplevel_window_base) {
    XID window_xid = x11_util::GetX11WindowFromGtkWidget(popup);
    XID window_parent = x11_util::GetParentWindow(window_xid);
    if (window_parent == x11_util::GetX11RootWindow()) {
      x11_util::RestackWindow(window_xid, toplevel_window_base, true);
    } else {
      // The window manager shouldn't reparent override-redirect windows.
      DLOG(ERROR) << "override-redirect window " << window_xid
                  << "'s parent is " << window_parent
                  << ", rather than root window "
                  << x11_util::GetX11RootWindow();
    }
  }
}

gfx::Rect GetWidgetRectRelativeToToplevel(GtkWidget* widget) {
  DCHECK(GTK_WIDGET_REALIZED(widget));

  GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
  DCHECK(toplevel);
  DCHECK(GTK_WIDGET_REALIZED(toplevel));

  gint x = 0, y = 0;
  gtk_widget_translate_coordinates(widget,
                                   toplevel,
                                   0, 0,
                                   &x, &y);
  return gfx::Rect(x, y, widget->allocation.width, widget->allocation.height);
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

void SuppressDefaultPainting(GtkWidget* container) {
  g_signal_connect(container, "expose-event",
                   G_CALLBACK(PaintNoBackground), NULL);
}

void WrapLabelAtAllocationHack(GtkWidget* label) {
  g_signal_connect(label, "size-allocate",
                   G_CALLBACK(OnLabelAllocate), NULL);
}

}  // namespace gtk_util
