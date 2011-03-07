// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/gtk_util.h"

#include <cairo/cairo.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <cstdarg>
#include <map>

#include "base/environment.h"
#include "base/i18n/rtl.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/cairo_cached_surface.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/common/renderer_preferences.h"
#include "content/browser/disposition_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gtk_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/native_dialog_window.h"
#include "views/window/window.h"
#else
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#endif

using WebKit::WebDragOperationsMask;
using WebKit::WebDragOperation;
using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationCopy;
using WebKit::WebDragOperationLink;
using WebKit::WebDragOperationMove;

namespace {

#if defined(GOOGLE_CHROME_BUILD)
static const char* kIconName = "google-chrome";
#else
static const char* kIconName = "chromium-browser";
#endif

const char kBoldLabelMarkup[] = "<span weight='bold'>%s</span>";

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
    if (button_mask & (1 << event->button))
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

// Returns the approximate number of characters that can horizontally fit in
// |pixel_width| pixels.
int GetCharacterWidthForPixels(GtkWidget* widget, int pixel_width) {
  DCHECK(GTK_WIDGET_REALIZED(widget))
      << " widget must be realized to compute font metrics correctly";

  PangoContext* context = gtk_widget_create_pango_context(widget);
  PangoFontMetrics* metrics = pango_context_get_metrics(context,
      widget->style->font_desc, pango_context_get_language(context));

  // This technique (max of char and digit widths) matches the code in
  // gtklabel.c.
  int char_width = pixel_width * PANGO_SCALE /
      std::max(pango_font_metrics_get_approximate_char_width(metrics),
               pango_font_metrics_get_approximate_digit_width(metrics));

  pango_font_metrics_unref(metrics);
  g_object_unref(context);

  return char_width;
}

void OnLabelRealize(GtkWidget* label, gpointer pixel_width) {
  gtk_label_set_width_chars(
      GTK_LABEL(label),
      GetCharacterWidthForPixels(label,GPOINTER_TO_INT(pixel_width)));
}

// Ownership of |icon_list| is passed to the caller.
GList* GetIconList() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GList* icon_list = NULL;
  icon_list = g_list_append(icon_list, rb.GetPixbufNamed(IDR_PRODUCT_ICON_32));
  icon_list = g_list_append(icon_list, rb.GetPixbufNamed(IDR_PRODUCT_LOGO_16));
  return icon_list;
}

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

#if defined(OS_CHROMEOS)

TabContents* GetBrowserWindowSelectedTabContents(BrowserWindow* window) {
  chromeos::BrowserView* browser_view = static_cast<chromeos::BrowserView*>(
      window);
  return browser_view->GetSelectedTabContents();
}

GtkWidget* GetBrowserWindowFocusedWidget(BrowserWindow* window) {
  gfx::NativeView widget = gtk_window_get_focus(window->GetNativeHandle());

  if (widget == NULL) {
    chromeos::BrowserView* browser_view = static_cast<chromeos::BrowserView*>(
        window);
    widget = browser_view->saved_focused_widget();
  }

  return widget;
}

#else

TabContents* GetBrowserWindowSelectedTabContents(BrowserWindow* window) {
  BrowserWindowGtk* browser_window = static_cast<BrowserWindowGtk*>(
      window);
  return browser_window->browser()->GetSelectedTabContents();
}

GtkWidget* GetBrowserWindowFocusedWidget(BrowserWindow* window) {
  return gtk_window_get_focus(window->GetNativeHandle());
}

#endif

}  // namespace

namespace event_utils {

WindowOpenDisposition DispositionFromEventFlags(guint event_flags) {
  return disposition_utils::DispositionFromClick(
      event_flags & GDK_BUTTON2_MASK,
      event_flags & GDK_MOD1_MASK,
      event_flags & GDK_CONTROL_MASK,
      event_flags & GDK_META_MASK,
      event_flags & GDK_SHIFT_MASK);
}

}  // namespace event_utils

namespace gtk_util {

const GdkColor kGdkWhite = GDK_COLOR_RGB(0xff, 0xff, 0xff);
const GdkColor kGdkGray  = GDK_COLOR_RGB(0x7f, 0x7f, 0x7f);
const GdkColor kGdkBlack = GDK_COLOR_RGB(0x00, 0x00, 0x00);
const GdkColor kGdkGreen = GDK_COLOR_RGB(0x00, 0xff, 0x00);

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

GtkWidget* LeftAlignMisc(GtkWidget* misc) {
  gtk_misc_set_alignment(GTK_MISC(misc), 0, 0.5);
  return misc;
}

GtkWidget* CreateBoldLabel(const std::string& text) {
  GtkWidget* label = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(kBoldLabelMarkup, text.c_str());
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);

  return LeftAlignMisc(label);
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

void GetWidgetSizeFromResources(
    GtkWidget* widget, int width_chars, int height_lines,
    int* width, int* height) {
  DCHECK(GTK_WIDGET_REALIZED(widget))
      << " widget must be realized to compute font metrics correctly";

  double chars = 0;
  if (width)
    base::StringToDouble(l10n_util::GetStringUTF8(width_chars), &chars);

  double lines = 0;
  if (height)
    base::StringToDouble(l10n_util::GetStringUTF8(height_lines), &lines);

  GetWidgetSizeFromCharacters(widget, chars, lines, width, height);
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
    // For a non-resizable window, GTK tries to snap the window size
    // to the minimum size around the content.  We use the sizes in
    // the resources to set *minimum* window size to allow windows
    // with long titles to be wide enough to display their titles.
    //
    // But if GTK wants to make the window *wider* due to very wide
    // controls, we should allow that too, so be careful to pick the
    // wider of the resources size and the natural window size.

    gtk_widget_show_all(GTK_BIN(window)->child);
    GtkRequisition requisition;
    gtk_widget_size_request(GTK_WIDGET(window), &requisition);
    gtk_widget_set_size_request(
        GTK_WIDGET(window),
        width == -1 ? -1 : std::max(width, requisition.width),
        height == -1 ? -1 : std::max(height, requisition.height));
  }
  gtk_window_set_resizable(window, resizable ? TRUE : FALSE);
}

void CenterOverWindow(GtkWindow* window, GtkWindow* parent) {
  gfx::Rect frame_bounds = gtk_util::GetWidgetScreenBounds(GTK_WIDGET(parent));
  gfx::Point origin = frame_bounds.origin();
  gfx::Size size = gtk_util::GetWidgetSize(GTK_WIDGET(window));
  origin.Offset(
      (frame_bounds.width() - size.width()) / 2,
      (frame_bounds.height() - size.height()) / 2);

  // Prevent moving window out of monitor bounds.
  GdkScreen* screen = gtk_window_get_screen(parent);
  if (screen) {
    // It would be better to check against workarea for given monitor
    // but getting workarea for particular monitor is tricky.
    gint monitor = gdk_screen_get_monitor_at_window(screen,
        GTK_WIDGET(parent)->window);
    GdkRectangle rect;
    gdk_screen_get_monitor_geometry(screen, monitor, &rect);

    // Check the right bottom corner.
    if (origin.x() > rect.x + rect.width - size.width())
      origin.set_x(rect.x + rect.width - size.width());
    if (origin.y() > rect.y + rect.height - size.height())
      origin.set_y(rect.y + rect.height - size.height());

    // Check the left top corner.
    if (origin.x() < rect.x)
      origin.set_x(rect.x);
    if (origin.y() < rect.y)
      origin.set_y(rect.y);
  }

  gtk_window_move(window, origin.x(), origin.y());

  // Move to user expected desktop if window is already visible.
  if (GTK_WIDGET(window)->window) {
    ui::ChangeWindowDesktop(
        ui::GetX11WindowFromGtkWidget(GTK_WIDGET(window)),
        ui::GetX11WindowFromGtkWidget(GTK_WIDGET(parent)));
  }
}

void MakeAppModalWindowGroup() {
#if GTK_CHECK_VERSION(2, 14, 0)
  // Older versions of GTK+ don't give us gtk_window_group_list() which is what
  // we need to add current non-browser modal dialogs to the list. If
  // we have 2.14+ we can do things the correct way.
  GtkWindowGroup* window_group = gtk_window_group_new();
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    // List all windows in this current group
    GtkWindowGroup* old_group =
        gtk_window_get_group((*it)->window()->GetNativeHandle());

    GList* all_windows = gtk_window_group_list_windows(old_group);
    for (GList* window = all_windows; window; window = window->next) {
      gtk_window_group_add_window(window_group, GTK_WINDOW(window->data));
    }
    g_list_free(all_windows);
  }
  g_object_unref(window_group);
#else
  // Otherwise just grab all browser windows and be slightly broken.
  GtkWindowGroup* window_group = gtk_window_group_new();
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    gtk_window_group_add_window(window_group,
                                (*it)->window()->GetNativeHandle());
  }
  g_object_unref(window_group);
#endif
}

void AppModalDismissedUngroupWindows() {
#if GTK_CHECK_VERSION(2, 14, 0)
  if (BrowserList::begin() != BrowserList::end()) {
    std::vector<GtkWindow*> transient_windows;

    // All windows should be part of one big modal group right now.
    GtkWindowGroup* window_group = gtk_window_get_group(
        (*BrowserList::begin())->window()->GetNativeHandle());
    GList* windows = gtk_window_group_list_windows(window_group);

    for (GList* item = windows; item; item = item->next) {
      GtkWindow* window = GTK_WINDOW(item->data);
      GtkWindow* transient_for = gtk_window_get_transient_for(window);
      if (transient_for) {
        transient_windows.push_back(window);
      } else {
        GtkWindowGroup* window_group = gtk_window_group_new();
        gtk_window_group_add_window(window_group, window);
        g_object_unref(window_group);
      }
    }

    // Put each transient window in the same group as its transient parent.
    for (std::vector<GtkWindow*>::iterator it = transient_windows.begin();
         it != transient_windows.end(); ++it) {
      GtkWindow* transient_parent = gtk_window_get_transient_for(*it);
      GtkWindowGroup* group = gtk_window_get_group(transient_parent);
      gtk_window_group_add_window(group, *it);
    }
  }
#else
  // This is slightly broken in the case where a different window had a dialog,
  // but its the best we can do since we don't have newer gtk stuff.
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    GtkWindowGroup* window_group = gtk_window_group_new();
    gtk_window_group_add_window(window_group,
                                (*it)->window()->GetNativeHandle());
    g_object_unref(window_group);
  }
#endif
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
}

void UndoForceFontSize(GtkWidget* widget) {
  gtk_widget_modify_font(widget, NULL);
}

gfx::Point GetWidgetScreenPosition(GtkWidget* widget) {
  if (!widget->window) {
    NOTREACHED() << "Must only be called on realized widgets.";
    return gfx::Point(0, 0);
  }

  gint x, y;
  gdk_window_get_origin(widget->window, &x, &y);

  if (GTK_WIDGET_NO_WINDOW(widget)) {
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

gfx::Size GetWidgetSize(GtkWidget* widget) {
  GtkRequisition size;
  gtk_widget_size_request(widget, &size);
  return gfx::Size(size.width, size.height);
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

GtkWidget* CenterWidgetInHBox(GtkWidget* hbox, GtkWidget* widget,
                              bool pack_at_end, int padding) {
  GtkWidget* centering_vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(centering_vbox), widget, TRUE, FALSE, 0);
  if (pack_at_end)
    gtk_box_pack_end(GTK_BOX(hbox), centering_vbox, FALSE, FALSE, padding);
  else
    gtk_box_pack_start(GTK_BOX(hbox), centering_vbox, FALSE, FALSE, padding);

  return centering_vbox;
}

bool IsScreenComposited() {
  GdkScreen* screen = gdk_screen_get_default();
  return gdk_screen_is_composited(screen) == TRUE;
}

void EnumerateTopLevelWindows(ui::EnumerateWindowsDelegate* delegate) {
  std::vector<XID> stack;
  if (!ui::GetXWindowStack(ui::GetX11RootWindow(), &stack)) {
    // Window Manager doesn't support _NET_CLIENT_LIST_STACKING, so fall back
    // to old school enumeration of all X windows.  Some WMs parent 'top-level'
    // windows in unnamed actual top-level windows (ion WM), so extend the
    // search depth to all children of top-level windows.
    const int kMaxSearchDepth = 1;
    ui::EnumerateAllWindows(delegate, kMaxSearchDepth);
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

  g_signal_connect(button, "button-press-event",
                   G_CALLBACK(OnMouseButtonPressed), userdata);
  g_signal_connect(button, "button-release-event",
                   G_CALLBACK(OnMouseButtonReleased), userdata);
}

void SetButtonTriggersNavigation(GtkWidget* button) {
  SetButtonClickableByMouseButtons(button, true, true, false);
}

int MirroredLeftPointForRect(GtkWidget* widget, const gfx::Rect& bounds) {
  if (!base::i18n::IsRTL())
    return bounds.x();
  return widget->allocation.width - bounds.x() - bounds.width();
}

int MirroredXCoordinate(GtkWidget* widget, int x) {
  if (base::i18n::IsRTL())
    return widget->allocation.width - x;
  return x;
}

bool WidgetContainsCursor(GtkWidget* widget) {
  gint x = 0;
  gint y = 0;
  gtk_widget_get_pointer(widget, &x, &y);
  return WidgetBounds(widget).Contains(x, y);
}

void SetWindowIcon(GtkWindow* window) {
  GList* icon_list = GetIconList();
  gtk_window_set_icon_list(window, icon_list);
  g_list_free(icon_list);
}

void SetDefaultWindowIcon(GtkWindow* window) {
  GtkIconTheme* theme =
      gtk_icon_theme_get_for_screen(gtk_widget_get_screen(GTK_WIDGET(window)));

  if (gtk_icon_theme_has_icon(theme, kIconName)) {
    gtk_window_set_default_icon_name(kIconName);
    // Sometimes the WM fails to update the icon when we tell it to. The above
    // line should be enough to update all existing windows, but it can fail,
    // e.g. with Lucid/metacity. The following line seems to fix the common
    // case where the first window created doesn't have an icon.
    gtk_window_set_icon_name(window, kIconName);
  } else {
    GList* icon_list = GetIconList();
    gtk_window_set_default_icon_list(icon_list);
    // Same logic applies here.
    gtk_window_set_icon_list(window, icon_list);
    g_list_free(icon_list);
  }
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

GtkWidget* BuildDialogButton(GtkWidget* dialog, int ids_id,
                             const gchar* stock_id) {
  GtkWidget* button = gtk_button_new_with_mnemonic(
      gfx::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(ids_id)).c_str());
  gtk_button_set_image(GTK_BUTTON(button),
                       gtk_image_new_from_stock(stock_id,
                                                GTK_ICON_SIZE_BUTTON));
  return button;
}

GtkWidget* CreateEntryImageHBox(GtkWidget* entry, GtkWidget* image) {
  GtkWidget* hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
  return hbox;
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

  // From http://library.gnome.org/devel/gtk/unstable/GtkSettings.html, this is
  // the default value for gtk-cursor-blink-time.
  static const gint kGtkDefaultCursorBlinkTime = 1200;

  gint cursor_blink_time = kGtkDefaultCursorBlinkTime;
  gboolean cursor_blink = TRUE;
  gint antialias = 0;
  gint hinting = 0;
  gchar* hint_style = NULL;
  gchar* rgba_style = NULL;
  g_object_get(gtk_settings_get_default(),
               "gtk-cursor-blink-time", &cursor_blink_time,
               "gtk-cursor-blink", &cursor_blink,
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

  if (cursor_blink) {
    // Dividing by 2*1000ms follows the WebKit GTK port and makes the blink
    // frequency appear similar to the omnibox.  Without this the blink is too
    // slow.
    prefs->caret_blink_interval = cursor_blink_time / 2000.;
  } else {
    prefs->caret_blink_interval = 0;
  }

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
  gint width = rec->width - 2 * xborder;
  gint height = rec->height - 2 * yborder;
  if (width > 0 && height > 0) {
    gtk_paint_flat_box(our_style, widget_to_draw_on->window,
                       GTK_STATE_NORMAL, GTK_SHADOW_NONE, dirty_rec,
                       widget_to_draw_on, "entry_bg",
                       rec->x + xborder, rec->y + yborder,
                       width, height);
  }

  gtk_style_detach(our_style);
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

void StackPopupWindow(GtkWidget* popup, GtkWidget* toplevel) {
  DCHECK(GTK_IS_WINDOW(popup) && GTK_WIDGET_TOPLEVEL(popup) &&
         GTK_WIDGET_REALIZED(popup));
  DCHECK(GTK_IS_WINDOW(toplevel) && GTK_WIDGET_TOPLEVEL(toplevel) &&
         GTK_WIDGET_REALIZED(toplevel));

  // Stack the |popup| window directly above the |toplevel| window.
  // The popup window is a direct child of the root window, so we need to
  // find a similar ancestor for the toplevel window (which might have been
  // reparented by a window manager).  We grab the server while we're doing
  // this -- otherwise, we'll get an error if the window manager reparents the
  // toplevel window right after we call GetHighestAncestorWindow().
  gdk_x11_display_grab(gtk_widget_get_display(toplevel));
  XID toplevel_window_base = ui::GetHighestAncestorWindow(
      ui::GetX11WindowFromGtkWidget(toplevel),
      ui::GetX11RootWindow());
  if (toplevel_window_base) {
    XID window_xid = ui::GetX11WindowFromGtkWidget(popup);
    XID window_parent = ui::GetParentWindow(window_xid);
    if (window_parent == ui::GetX11RootWindow()) {
      ui::RestackWindow(window_xid, toplevel_window_base, true);
    } else {
      // The window manager shouldn't reparent override-redirect windows.
      DLOG(ERROR) << "override-redirect window " << window_xid
                  << "'s parent is " << window_parent
                  << ", rather than root window "
                  << ui::GetX11RootWindow();
    }
  }
  gdk_x11_display_ungrab(gtk_widget_get_display(toplevel));
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

void SuppressDefaultPainting(GtkWidget* container) {
  g_signal_connect(container, "expose-event",
                   G_CALLBACK(PaintNoBackground), NULL);
}

WindowOpenDisposition DispositionForCurrentButtonPressEvent() {
  GdkEvent* event = gtk_get_current_event();
  if (!event) {
    NOTREACHED();
    return NEW_FOREGROUND_TAB;
  }

  guint state = event->button.state;
  gdk_event_free(event);
  return event_utils::DispositionFromEventFlags(state);
}

bool GrabAllInput(GtkWidget* widget) {
  guint time = gtk_get_current_event_time();

  if (!GTK_WIDGET_VISIBLE(widget))
    return false;

  if (!gdk_pointer_grab(widget->window, TRUE,
                        GdkEventMask(GDK_BUTTON_PRESS_MASK |
                                     GDK_BUTTON_RELEASE_MASK |
                                     GDK_ENTER_NOTIFY_MASK |
                                     GDK_LEAVE_NOTIFY_MASK |
                                     GDK_POINTER_MOTION_MASK),
                        NULL, NULL, time) == 0) {
    return false;
  }

  if (!gdk_keyboard_grab(widget->window, TRUE, time) == 0) {
    gdk_display_pointer_ungrab(gdk_drawable_get_display(widget->window), time);
    return false;
  }

  gtk_grab_add(widget);
  return true;
}

gfx::Rect WidgetBounds(GtkWidget* widget) {
  // To quote the gtk docs:
  //
  //   Widget coordinates are a bit odd; for historical reasons, they are
  //   defined as widget->window coordinates for widgets that are not
  //   GTK_NO_WINDOW widgets, and are relative to widget->allocation.x,
  //   widget->allocation.y for widgets that are GTK_NO_WINDOW widgets.
  //
  // So the base is always (0,0).
  return gfx::Rect(0, 0, widget->allocation.width, widget->allocation.height);
}

void SetWMLastUserActionTime(GtkWindow* window) {
  gdk_x11_window_set_user_time(GTK_WIDGET(window)->window, XTimeNow());
}

guint32 XTimeNow() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

bool URLFromPrimarySelection(Profile* profile, GURL* url) {
  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  DCHECK(clipboard);
  gchar* selection_text = gtk_clipboard_wait_for_text(clipboard);
  if (!selection_text)
    return false;

  // Use autocomplete to clean up the text, going so far as to turn it into
  // a search query if necessary.
  AutocompleteMatch match;
  profile->GetAutocompleteClassifier()->Classify(UTF8ToUTF16(selection_text),
      string16(), false, &match, NULL);
  g_free(selection_text);
  if (!match.destination_url.is_valid())
    return false;

  *url = match.destination_url;
  return true;
}

bool AddWindowAlphaChannel(GtkWidget* window) {
  GdkScreen* screen = gtk_widget_get_screen(window);
  GdkColormap* rgba = gdk_screen_get_rgba_colormap(screen);
  if (rgba)
    gtk_widget_set_colormap(window, rgba);

  return rgba;
}

void GetTextColors(GdkColor* normal_base,
                   GdkColor* selected_base,
                   GdkColor* normal_text,
                   GdkColor* selected_text) {
  GtkWidget* fake_entry = gtk_entry_new();
  GtkStyle* style = gtk_rc_get_style(fake_entry);

  if (normal_base)
    *normal_base = style->base[GTK_STATE_NORMAL];
  if (selected_base)
    *selected_base = style->base[GTK_STATE_SELECTED];
  if (normal_text)
    *normal_text = style->text[GTK_STATE_NORMAL];
  if (selected_text)
    *selected_text = style->text[GTK_STATE_SELECTED];

  g_object_ref_sink(fake_entry);
  g_object_unref(fake_entry);
}

#if defined(OS_CHROMEOS)

GtkWindow* GetLastActiveBrowserWindow() {
  if (Browser* b = BrowserList::GetLastActive()) {
    if (b->type() != Browser::TYPE_NORMAL) {
      b = BrowserList::FindBrowserWithType(b->profile(),
                                           Browser::TYPE_NORMAL,
                                           true);
    }

    if (b)
      return GTK_WINDOW(b->window()->GetNativeHandle());
  }

  return NULL;
}

int GetNativeDialogFlags(GtkWindow* dialog) {
  int flags = chromeos::DIALOG_FLAG_DEFAULT;

  if (gtk_window_get_resizable(dialog))
    flags |= chromeos::DIALOG_FLAG_RESIZEABLE;
  if (gtk_window_get_modal(dialog))
    flags |= chromeos::DIALOG_FLAG_MODAL;

  return flags;
}

GtkWindow* GetDialogTransientParent(GtkWindow* dialog) {
  GtkWindow* parent = gtk_window_get_transient_for(dialog);
  if (!parent)
    parent = GetLastActiveBrowserWindow();

  return parent;
}

void ShowDialog(GtkWidget* dialog) {
  // Make sure all controls are visible so that we get correct size.
  gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);

  // Get dialog window size.
  gint width = 0;
  gint height = 0;
  gtk_window_get_size(GTK_WINDOW(dialog), &width, &height);

  chromeos::ShowNativeDialog(GetDialogTransientParent(GTK_WINDOW(dialog)),
      dialog,
      GetNativeDialogFlags(GTK_WINDOW(dialog)),
      gfx::Size(width, height),
      gfx::Size());
}

void ShowDialogWithLocalizedSize(GtkWidget* dialog,
                                 int width_id,
                                 int height_id,
                                 bool resizeable) {
  int width = (width_id == -1) ? 0 :
      views::Window::GetLocalizedContentsWidth(width_id);
  int height = (height_id == -1) ? 0 :
      views::Window::GetLocalizedContentsHeight(height_id);

  chromeos::ShowNativeDialog(GetDialogTransientParent(GTK_WINDOW(dialog)),
      dialog,
      resizeable ? chromeos::DIALOG_FLAG_RESIZEABLE :
                   chromeos::DIALOG_FLAG_DEFAULT,
      gfx::Size(width, height),
      gfx::Size());
}

void ShowDialogWithMinLocalizedWidth(GtkWidget* dialog,
                                     int width_id) {
  int width = (width_id == -1) ? 0 :
      views::Window::GetLocalizedContentsWidth(width_id);

  chromeos::ShowNativeDialog(GetDialogTransientParent(GTK_WINDOW(dialog)),
      dialog,
      GetNativeDialogFlags(GTK_WINDOW(dialog)),
      gfx::Size(),
      gfx::Size(width, 0));
}

void PresentWindow(GtkWidget* window, int timestamp) {
  GtkWindow* host_window = chromeos::GetNativeDialogWindow(window);
  if (!host_window)
      host_window = GTK_WINDOW(window);
  if (timestamp)
    gtk_window_present_with_time(host_window, timestamp);
  else
    gtk_window_present(host_window);
}

GtkWindow* GetDialogWindow(GtkWidget* dialog) {
  return chromeos::GetNativeDialogWindow(dialog);
}

gfx::Rect GetDialogBounds(GtkWidget* dialog) {
  return chromeos::GetNativeDialogContentsBounds(dialog);
}

#else

void ShowDialog(GtkWidget* dialog) {
  gtk_widget_show_all(dialog);
}

void ShowDialogWithLocalizedSize(GtkWidget* dialog,
                                 int width_id,
                                 int height_id,
                                 bool resizeable) {
  gtk_widget_realize(dialog);
  SetWindowSizeFromResources(GTK_WINDOW(dialog),
                             width_id,
                             height_id,
                             resizeable);
  gtk_widget_show_all(dialog);
}

void ShowDialogWithMinLocalizedWidth(GtkWidget* dialog,
                                     int width_id) {
  gtk_widget_show_all(dialog);

  // Suggest a minimum size.
  gint width;
  GtkRequisition req;
  gtk_widget_size_request(dialog, &req);
  gtk_util::GetWidgetSizeFromResources(dialog, width_id, 0, &width, NULL);
  if (width > req.width)
    gtk_widget_set_size_request(dialog, width, -1);
}

void PresentWindow(GtkWidget* window, int timestamp) {
  if (timestamp)
    gtk_window_present_with_time(GTK_WINDOW(window), timestamp);
  else
    gtk_window_present(GTK_WINDOW(window));
}

GtkWindow* GetDialogWindow(GtkWidget* dialog) {
  return GTK_WINDOW(dialog);
}

gfx::Rect GetDialogBounds(GtkWidget* dialog) {
  gint x = 0, y = 0, width = 1, height = 1;
  gtk_window_get_position(GTK_WINDOW(dialog), &x, &y);
  gtk_window_get_size(GTK_WINDOW(dialog), &width, &height);

  return gfx::Rect(x, y, width, height);
}

#endif

string16 GetStockPreferencesMenuLabel() {
  GtkStockItem stock_item;
  string16 preferences;
  if (gtk_stock_lookup(GTK_STOCK_PREFERENCES, &stock_item)) {
    const char16 kUnderscore[] = { '_', 0 };
    RemoveChars(UTF8ToUTF16(stock_item.label), kUnderscore, &preferences);
  }
  return preferences;
}

bool IsWidgetAncestryVisible(GtkWidget* widget) {
  GtkWidget* parent = widget;
  while (parent && GTK_WIDGET_VISIBLE(parent))
    parent = parent->parent;
  return !parent;
}

void SetGtkFont(const std::string& font_name) {
  g_object_set(gtk_settings_get_default(),
               "gtk-font-name", font_name.c_str(), NULL);
}

void SetLabelWidth(GtkWidget* label, int pixel_width) {
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

  // Do the simple thing in LTR because the bug only affects right-aligned
  // text. Also, when using the workaround, the label tries to maintain
  // uniform line-length, which we don't really want.
  if (gtk_widget_get_direction(label) == GTK_TEXT_DIR_LTR) {
    gtk_widget_set_size_request(label, pixel_width, -1);
  } else {
    // The label has to be realized before we can adjust its width.
    if (GTK_WIDGET_REALIZED(label)) {
      OnLabelRealize(label, GINT_TO_POINTER(pixel_width));
    } else {
      g_signal_connect(label, "realize", G_CALLBACK(OnLabelRealize),
                       GINT_TO_POINTER(pixel_width));
    }
  }
}

void InitLabelSizeRequestAndEllipsizeMode(GtkWidget* label) {
  GtkRequisition size;
  gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_NONE);
  gtk_widget_set_size_request(label, -1, -1);
  gtk_widget_size_request(label, &size);
  gtk_widget_set_size_request(label, size.width, size.height);
  gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
}

GdkDragAction WebDragOpToGdkDragAction(WebDragOperationsMask op) {
  GdkDragAction action = static_cast<GdkDragAction>(0);
  if (op & WebDragOperationCopy)
    action = static_cast<GdkDragAction>(action | GDK_ACTION_COPY);
  if (op & WebDragOperationLink)
    action = static_cast<GdkDragAction>(action | GDK_ACTION_LINK);
  if (op & WebDragOperationMove)
    action = static_cast<GdkDragAction>(action | GDK_ACTION_MOVE);
  return action;
}

WebDragOperationsMask GdkDragActionToWebDragOp(GdkDragAction action) {
  WebDragOperationsMask op = WebDragOperationNone;
  if (action & GDK_ACTION_COPY)
    op = static_cast<WebDragOperationsMask>(op | WebDragOperationCopy);
  if (action & GDK_ACTION_LINK)
    op = static_cast<WebDragOperationsMask>(op | WebDragOperationLink);
  if (action & GDK_ACTION_MOVE)
    op = static_cast<WebDragOperationsMask>(op | WebDragOperationMove);
  return op;
}

void ApplyMessageDialogQuirks(GtkWidget* dialog) {
  if (gtk_window_get_modal(GTK_WINDOW(dialog))) {
    // Work around a KDE 3 window manager bug.
    scoped_ptr<base::Environment> env(base::Environment::Create());
    if (base::nix::DESKTOP_ENVIRONMENT_KDE3 ==
        base::nix::GetDesktopEnvironment(env.get()))
      gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), FALSE);
  }
}

// Performs Cut/Copy/Paste operation on the |window|.
// If the current render view is focused, then just call the specified |method|
// against the current render view host, otherwise emit the specified |signal|
// against the focused widget.
// TODO(suzhe): This approach does not work for plugins.
void DoCutCopyPaste(BrowserWindow* window,
                    void (RenderViewHost::*method)(),
                    const char* signal) {
  GtkWidget* widget = GetBrowserWindowFocusedWidget(window);
  if (widget == NULL)
    return;  // Do nothing if no focused widget.

  TabContents* current_tab = GetBrowserWindowSelectedTabContents(window);
  if (current_tab && widget == current_tab->GetContentNativeView()) {
    (current_tab->render_view_host()->*method)();
  } else {
    guint id;
    if ((id = g_signal_lookup(signal, G_OBJECT_TYPE(widget))) != 0)
      g_signal_emit(widget, id, 0);
  }
}

void DoCut(BrowserWindow* window) {
  DoCutCopyPaste(window, &RenderViewHost::Cut, "cut-clipboard");
}

void DoCopy(BrowserWindow* window) {
  DoCutCopyPaste(window, &RenderViewHost::Copy, "copy-clipboard");
}

void DoPaste(BrowserWindow* window) {
  DoCutCopyPaste(window, &RenderViewHost::Paste, "paste-clipboard");
}

}  // namespace gtk_util
