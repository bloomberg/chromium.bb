// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/gtk2_ui.h"

#include <math.h>
#include <pango/pango.h>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/environment.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/nix/mime_util_xdg.h"
#include "base/nix/xdg_util.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/libgtk2ui/app_indicator_icon.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_event_loop.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_key_bindings_handler.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_status_icon.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_util.h"
#include "chrome/browser/ui/libgtk2ui/native_theme_gtk2.h"
#include "chrome/browser/ui/libgtk2ui/print_dialog_gtk2.h"
#include "chrome/browser/ui/libgtk2ui/printing_gtk2_util.h"
#include "chrome/browser/ui/libgtk2ui/select_file_dialog_impl.h"
#include "chrome/browser/ui/libgtk2ui/skia_utils_gtk2.h"
#include "chrome/browser/ui/libgtk2ui/unity_service.h"
#include "chrome/browser/ui/libgtk2ui/x11_input_method_context_impl_gtk2.h"
#include "grit/components_scaled_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/linux_ui/window_button_order_observer.h"
#include "ui/views/resources/grit/views_resources.h"

#if defined(ENABLE_BASIC_PRINTING)
#include "printing/printing_context_linux.h"
#endif
#if defined(USE_GCONF)
#include "chrome/browser/ui/libgtk2ui/gconf_listener.h"
#endif

// A minimized port of GtkThemeService into something that can provide colors
// and images for aura.
//
// TODO(erg): There's still a lot that needs ported or done for the first time:
//
// - Render and inject the omnibox background.
// - Make sure to test with a light on dark theme, too.

// Work around a header bug:
// linux/debian_wheezy_i386-sysroot/usr/include/linux/stddef.h redefines NULL
// to 0, which breaks -Wsentinel. Get back the normal definition of NULL.
// TODO(thakis): Remove this once we update sysroots.
#define __need_NULL
#include <stddef.h>

namespace libgtk2ui {

namespace {

class GtkThemeIconSource : public gfx::ImageSkiaSource {
 public:
  GtkThemeIconSource(int id, const char* icon, bool enabled)
      : id_(id), icon_(icon), enabled_(enabled) {}

  ~GtkThemeIconSource() override {}

  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    SkBitmap default_icon = rb.GetImageNamed(id_).AsBitmap();

    int scalew = default_icon.width() * scale;
    int scaleh = default_icon.height() * scale;

    // Ask GTK to render the icon to a buffer, which we will steal from.
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    GdkPixbuf* gdk_icon = gtk_icon_theme_load_icon(
        icon_theme, icon_, 20 * scale, (GtkIconLookupFlags)0, NULL);

    // This can theoretically happen if an icon theme doesn't provide a
    // specific image. This should realistically never happen, but I bet there
    // are some theme authors who don't reliably provide all icons.
    if (!gdk_icon)
      return gfx::ImageSkiaRep();

#if GTK_MAJOR_VERSION == 2
    GtkIconSource* icon_source = gtk_icon_source_new();
    gtk_icon_source_set_pixbuf(icon_source, gdk_icon);

    GdkPixbuf* temp = gtk_style_render_icon(
        gtk_rc_get_style(NativeThemeGtk2::instance()->GetButton()), icon_source,
        GTK_TEXT_DIR_NONE, enabled_ ? GTK_STATE_NORMAL : GTK_STATE_INSENSITIVE,
        (GtkIconSize)-1, NativeThemeGtk2::instance()->GetButton(), NULL);

    gtk_icon_source_free(icon_source);
    g_object_unref(gdk_icon);

    gdk_icon = temp;
#endif

    SkBitmap retval;
    retval.allocN32Pixels(scalew, scaleh);
    retval.eraseColor(0);

    const SkBitmap icon = GdkPixbufToImageSkia(gdk_icon);
    g_object_unref(gdk_icon);

    SkCanvas canvas(retval);
    SkPaint paint;

#if GTK_MAJOR_VERSION > 2
    if (!enabled_)
      paint.setAlpha(128);
#endif

    canvas.drawBitmap(icon, (scalew / 2) - (icon.width() / 2),
                      (scaleh / 2) - (icon.height() / 2), &paint);

    return gfx::ImageSkiaRep(retval, scale);
  }

 private:
  int id_;
  const char* icon_;
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(GtkThemeIconSource);
};

class GtkButtonImageSource : public gfx::ImageSkiaSource {
 public:
  GtkButtonImageSource(const char* idr_string, gfx::Size size)
      : width_(size.width()), height_(size.height()) {
    is_blue_ = !!strstr(idr_string, "IDR_BLUE");
    focus_ = !!strstr(idr_string, "_FOCUSED_");

    if (strstr(idr_string, "_DISABLED")) {
      state_ = ui::NativeTheme::kDisabled;
    } else if (strstr(idr_string, "_HOVER")) {
      state_ = ui::NativeTheme::kHovered;
    } else if (strstr(idr_string, "_PRESSED")) {
      state_ = ui::NativeTheme::kPressed;
    } else {
      state_ = ui::NativeTheme::kNormal;
    }
  }

  ~GtkButtonImageSource() override {}

  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    int width = width_ * scale;
    int height = height_ * scale;

    SkBitmap border;
    border.allocN32Pixels(width, height);
    border.eraseColor(0);

    // Create a temporary GTK button to snapshot
    GtkWidget* window = gtk_offscreen_window_new();
    GtkWidget* button = gtk_toggle_button_new();

    if (state_ == ui::NativeTheme::kPressed)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), true);
    else if (state_ == ui::NativeTheme::kDisabled)
      gtk_widget_set_sensitive(button, false);

    gtk_widget_set_size_request(button, width, height);
    gtk_container_add(GTK_CONTAINER(window), button);

    if (is_blue_)
      TurnButtonBlue(button);

    gtk_widget_show_all(window);

    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(border.getAddr(0, 0)),
        CAIRO_FORMAT_ARGB32, width, height, width * 4);
    cairo_t* cr = cairo_create(surface);

#if GTK_MAJOR_VERSION == 2
    if (focus_)
      GTK_WIDGET_SET_FLAGS(button, GTK_HAS_FOCUS);

    int w, h;
    GdkPixmap* pixmap;

    {
      // http://crbug.com/346740
      ANNOTATE_SCOPED_MEMORY_LEAK;
      pixmap = gtk_widget_get_snapshot(button, NULL);
    }

    gdk_drawable_get_size(GDK_DRAWABLE(pixmap), &w, &h);
    GdkColormap* colormap = gdk_drawable_get_colormap(pixmap);
    GdkPixbuf* pixbuf = gdk_pixbuf_get_from_drawable(
        NULL, GDK_DRAWABLE(pixmap), colormap, 0, 0, 0, 0, w, h);

    gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
    cairo_paint(cr);

    g_object_unref(pixbuf);
    g_object_unref(pixmap);
#else
    gtk_widget_draw(button, cr);

    // There's probably a better way to do this
    if (focus_)
      gtk_render_focus(gtk_widget_get_style_context(button), cr, 0, 0,
                       width, height);
#endif

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    gtk_widget_destroy(window);

    return gfx::ImageSkiaRep(border, scale);
  }

 private:
  bool is_blue_;
  bool focus_;
  ui::NativeTheme::State state_;
  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(GtkButtonImageSource);
};

class GtkButtonPainter : public views::Painter {
 public:
  explicit GtkButtonPainter(std::string idr) : idr_(idr) {}
  ~GtkButtonPainter() override {}

  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    gfx::ImageSkiaSource* source = new GtkButtonImageSource(idr_.c_str(), size);
    gfx::ImageSkia image(source, 1);
    canvas->DrawImageInt(image, 0, 0);
  }

 private:
  std::string idr_;

  DISALLOW_COPY_AND_ASSIGN(GtkButtonPainter);
};

struct GObjectDeleter {
  void operator()(void* ptr) {
    g_object_unref(ptr);
  }
};
struct GtkIconInfoDeleter {
  void operator()(GtkIconInfo* ptr) {
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_icon_info_free(ptr);
    G_GNUC_END_IGNORE_DEPRECATIONS
  }
};
typedef std::unique_ptr<GIcon, GObjectDeleter> ScopedGIcon;
typedef std::unique_ptr<GtkIconInfo, GtkIconInfoDeleter> ScopedGtkIconInfo;
typedef std::unique_ptr<GdkPixbuf, GObjectDeleter> ScopedGdkPixbuf;

// Prefix for app indicator ids
const char kAppIndicatorIdPrefix[] = "chrome_app_indicator_";

// Number of app indicators used (used as part of app-indicator id).
int indicators_count;

// The unknown content type.
const char* kUnknownContentType = "application/octet-stream";

// The size of the rendered toolbar image.
const int kToolbarImageWidth = 64;
const int kToolbarImageHeight = 128;

// Values used as the new luminance and saturation values in the inactive tab
// text color.
const double kInactiveLuminance = 0.15;
const double kInactiveSaturation = 0.3;

// TODO(erg): ThemeService has a whole interface just for reading default
// constants. Figure out what to do with that more long term; for now, just
// copy the constants themselves here.
//
// Default tints.
const color_utils::HSL kDefaultTintFrameIncognito = { -1, 0.2f, 0.35f };
const color_utils::HSL kDefaultTintFrameIncognitoInactive = { -1, 0.3f, 0.6f };
const color_utils::HSL kDefaultTintBackgroundTab = { -1, 0.5, 0.75 };

#if GTK_MAJOR_VERSION == 3
const color_utils::HSL kDefaultTintFrameInactive = { -1, -1, 0.75f };
#endif

// Picks a button tint from a set of background colors. While
// |accent_color| will usually be the same color through a theme, this
// function will get called with the normal GtkLabel |text_color|/GtkWindow
// |background_color| pair and the GtkEntry |text_color|/|background_color|
// pair. While 3/4 of the time the resulting tint will be the same, themes that
// have a dark window background (with light text) and a light text entry (with
// dark text) will get better icons with this separated out.
void PickButtonTintFromColors(SkColor accent_color,
                              SkColor text_color,
                              SkColor background_color,
                              color_utils::HSL* tint) {
  color_utils::HSL accent_tint, text_tint, background_tint;
  color_utils::SkColorToHSL(accent_color, &accent_tint);
  color_utils::SkColorToHSL(text_color, &text_tint);
  color_utils::SkColorToHSL(background_color, &background_tint);

  // If the accent color is gray, then our normal HSL tomfoolery will bring out
  // whatever color is oddly dominant (for example, in rgb space [125, 128,
  // 125] will tint green instead of gray). Slight differences (+/-10 (4%) to
  // all color components) should be interpreted as this color being gray and
  // we should switch into a special grayscale mode.
  int rb_diff = abs(static_cast<int>(SkColorGetR(accent_color)) -
                    static_cast<int>(SkColorGetB(accent_color)));
  int rg_diff = abs(static_cast<int>(SkColorGetR(accent_color)) -
                    static_cast<int>(SkColorGetG(accent_color)));
  int bg_diff = abs(static_cast<int>(SkColorGetB(accent_color)) -
                    static_cast<int>(SkColorGetG(accent_color)));
  if (rb_diff < 10 && rg_diff < 10 && bg_diff < 10) {
    // Our accent is white/gray/black. Only the luminance of the accent color
    // matters.
    tint->h = -1;

    // Use the saturation of the text.
    tint->s = text_tint.s;

    // Use the luminance of the accent color UNLESS there isn't enough
    // luminance contrast between the accent color and the base color.
    if (fabs(accent_tint.l - background_tint.l) > 0.3)
      tint->l = accent_tint.l;
    else
      tint->l = text_tint.l;
  } else {
    // Our accent is a color.
    tint->h = accent_tint.h;

    // Don't modify the saturation; the amount of color doesn't matter.
    tint->s = -1;

    // If the text wants us to darken the icon, don't change the luminance (the
    // icons are already dark enough). Otherwise, lighten the icon by no more
    // than 0.9 since we don't want a pure-white icon even if the text is pure
    // white.
    if (text_tint.l < 0.5)
      tint->l = -1;
    else if (text_tint.l <= 0.9)
      tint->l = text_tint.l;
    else
      tint->l = 0.9;
  }
}

// Returns a gfx::FontRenderParams corresponding to GTK's configuration.
gfx::FontRenderParams GetGtkFontRenderParams() {
  GtkSettings* gtk_settings = gtk_settings_get_default();
  CHECK(gtk_settings);
  gint antialias = 0;
  gint hinting = 0;
  gchar* hint_style = NULL;
  gchar* rgba = NULL;
  g_object_get(gtk_settings,
               "gtk-xft-antialias", &antialias,
               "gtk-xft-hinting", &hinting,
               "gtk-xft-hintstyle", &hint_style,
               "gtk-xft-rgba", &rgba,
               NULL);

  gfx::FontRenderParams params;
  params.antialiasing = antialias != 0;

  if (hinting == 0 || !hint_style || strcmp(hint_style, "hintnone") == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_NONE;
  } else if (strcmp(hint_style, "hintslight") == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_SLIGHT;
  } else if (strcmp(hint_style, "hintmedium") == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_MEDIUM;
  } else if (strcmp(hint_style, "hintfull") == 0) {
    params.hinting = gfx::FontRenderParams::HINTING_FULL;
  } else {
    LOG(WARNING) << "Unexpected gtk-xft-hintstyle \"" << hint_style << "\"";
    params.hinting = gfx::FontRenderParams::HINTING_NONE;
  }

  if (!rgba || strcmp(rgba, "none") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE;
  } else if (strcmp(rgba, "rgb") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB;
  } else if (strcmp(rgba, "bgr") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR;
  } else if (strcmp(rgba, "vrgb") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB;
  } else if (strcmp(rgba, "vbgr") == 0) {
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR;
  } else {
    LOG(WARNING) << "Unexpected gtk-xft-rgba \"" << rgba << "\"";
    params.subpixel_rendering = gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE;
  }

  g_free(hint_style);
  g_free(rgba);

  return params;
}

double GetDPI() {
  GtkSettings* gtk_settings = gtk_settings_get_default();
  CHECK(gtk_settings);
  gint gtk_dpi = -1;
  g_object_get(gtk_settings, "gtk-xft-dpi", &gtk_dpi, NULL);

  // GTK multiplies the DPI by 1024 before storing it.
  return (gtk_dpi > 0) ? gtk_dpi / 1024.0 : 96.0;
}

// Queries GTK for its font DPI setting and returns the number of pixels in a
// point.
double GetPixelsInPoint(float device_scale_factor) {
  double dpi = GetDPI();

  // Take device_scale_factor into account — if Chrome already scales the
  // entire UI up by 2x, we should not also scale up.
  dpi /= device_scale_factor;

  // There are 72 points in an inch.
  return dpi / 72.0;
}

views::LinuxUI::NonClientMiddleClickAction GetDefaultMiddleClickAction() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
      // Starting with KDE 4.4, windows' titlebars can be dragged with the
      // middle mouse button to create tab groups. We don't support that in
      // Chrome, but at least avoid lowering windows in response to middle
      // clicks to avoid surprising users who expect the KDE behavior.
      return views::LinuxUI::MIDDLE_CLICK_ACTION_NONE;
    default:
      return views::LinuxUI::MIDDLE_CLICK_ACTION_LOWER;
  }
}

}  // namespace

Gtk2UI::Gtk2UI()
    : default_font_size_pixels_(0),
      default_font_style_(gfx::Font::NORMAL),
      middle_click_action_(GetDefaultMiddleClickAction()),
      device_scale_factor_(1.0) {
  GtkInitFromCommandLine(*base::CommandLine::ForCurrentProcess());
}



void OnThemeChanged(GObject* obj, GParamSpec* param, Gtk2UI* gtkui) {
  gtkui->ResetStyle();
}

void Gtk2UI::Initialize() {
  GtkSettings* settings = gtk_settings_get_default();
  g_signal_connect_after(settings,
                         "notify::gtk-theme-name",
                         G_CALLBACK(OnThemeChanged),
                         this);
  g_signal_connect_after(settings,
                         "notify::gtk-icon-theme-name",
                         G_CALLBACK(OnThemeChanged),
                         this);


  LoadGtkValues();

#if defined(ENABLE_BASIC_PRINTING)
  printing::PrintingContextLinux::SetCreatePrintDialogFunction(
      &PrintDialogGtk2::CreatePrintDialog);
  printing::PrintingContextLinux::SetPdfPaperSizeFunction(
      &GetPdfPaperSizeDeviceUnitsGtk);
#endif

#if defined(USE_GCONF)
  // We must build this after GTK gets initialized.
  gconf_listener_.reset(new GConfListener(this));
#endif  // defined(USE_GCONF)

  indicators_count = 0;

  // Instantiate the singleton instance of Gtk2EventLoop.
  Gtk2EventLoop::GetInstance();
}

void Gtk2UI::MaterialDesignControllerReady() {
  UpdateMaterialDesignColors();
}

Gtk2UI::~Gtk2UI() {
  ClearAllThemeData();
}

gfx::Image Gtk2UI::GetThemeImageNamed(int id) const {
  // Try to get our cached version:
  ImageCache::const_iterator it = gtk_images_.find(id);
  if (it != gtk_images_.end())
    return it->second;

  gfx::Image image = GenerateGtkThemeImage(id);

  if (image.IsEmpty()) {
    SkBitmap bitmap = GenerateGtkThemeBitmap(id);
    if (!bitmap.empty())
      image = gfx::Image::CreateFrom1xBitmap(bitmap);
  }

  gtk_images_[id] = image;
  return image;
}

bool Gtk2UI::GetTint(int id, color_utils::HSL* tint) const {
  switch (id) {
    case ThemeProperties::TINT_BACKGROUND_TAB:
      // Tints for which the cross-platform default is fine. Before adding new
      // values here, specifically verify they work well on Linux.
      break;
    default:
      // Assume any tints not specifically verified on Linux aren't usable.
      // TODO(pkasting): Try to remove values from |colors_| that could just be
      // added to the group above instead.
      NOTREACHED();
  }
  return false;
}

bool Gtk2UI::GetColor(int id, SkColor* color) const {
  ColorMap::const_iterator it = colors_.find(id);
  if (it != colors_.end()) {
    *color = it->second;
    return true;
  }

  return false;
}

bool Gtk2UI::HasCustomImage(int id) const {
  return !GetThemeImageNamed(id).IsEmpty();
}

SkColor Gtk2UI::GetFocusRingColor() const {
  return focus_ring_color_;
}

SkColor Gtk2UI::GetThumbActiveColor() const {
  return thumb_active_color_;
}

SkColor Gtk2UI::GetThumbInactiveColor() const {
  return thumb_inactive_color_;
}

SkColor Gtk2UI::GetTrackColor() const {
  return track_color_;
}

SkColor Gtk2UI::GetActiveSelectionBgColor() const {
  return active_selection_bg_color_;
}

SkColor Gtk2UI::GetActiveSelectionFgColor() const {
  return active_selection_fg_color_;
}

SkColor Gtk2UI::GetInactiveSelectionBgColor() const {
  return inactive_selection_bg_color_;
}

SkColor Gtk2UI::GetInactiveSelectionFgColor() const {
  return inactive_selection_fg_color_;
}

double Gtk2UI::GetCursorBlinkInterval() const {
  // From http://library.gnome.org/devel/gtk/unstable/GtkSettings.html, this is
  // the default value for gtk-cursor-blink-time.
  static const gint kGtkDefaultCursorBlinkTime = 1200;

  // Dividing GTK's cursor blink cycle time (in milliseconds) by this value
  // yields an appropriate value for
  // content::RendererPreferences::caret_blink_interval.  This matches the
  // logic in the WebKit GTK port.
  static const double kGtkCursorBlinkCycleFactor = 2000.0;

  gint cursor_blink_time = kGtkDefaultCursorBlinkTime;
  gboolean cursor_blink = TRUE;
  g_object_get(gtk_settings_get_default(),
               "gtk-cursor-blink-time", &cursor_blink_time,
               "gtk-cursor-blink", &cursor_blink,
               NULL);
  return cursor_blink ? (cursor_blink_time / kGtkCursorBlinkCycleFactor) : 0.0;
}

ui::NativeTheme* Gtk2UI::GetNativeTheme(aura::Window* window) const {
  ui::NativeTheme* native_theme_override = NULL;
  if (!native_theme_overrider_.is_null())
    native_theme_override = native_theme_overrider_.Run(window);

  if (native_theme_override)
    return native_theme_override;

  return NativeThemeGtk2::instance();
}

void Gtk2UI::SetNativeThemeOverride(const NativeThemeGetter& callback) {
  native_theme_overrider_ = callback;
}

bool Gtk2UI::GetDefaultUsesSystemTheme() const {
  std::unique_ptr<base::Environment> env(base::Environment::Create());

  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
    case base::nix::DESKTOP_ENVIRONMENT_UNITY:
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
      return true;
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      return false;
  }
  // Unless GetDesktopEnvironment() badly misbehaves, this should never happen.
  NOTREACHED();
  return false;
}

void Gtk2UI::SetDownloadCount(int count) const {
  if (unity::IsRunning())
    unity::SetDownloadCount(count);
}

void Gtk2UI::SetProgressFraction(float percentage) const {
  if (unity::IsRunning())
    unity::SetProgressFraction(percentage);
}

bool Gtk2UI::IsStatusIconSupported() const {
  return true;
}

std::unique_ptr<views::StatusIconLinux> Gtk2UI::CreateLinuxStatusIcon(
    const gfx::ImageSkia& image,
    const base::string16& tool_tip) const {
  if (AppIndicatorIcon::CouldOpen()) {
    ++indicators_count;
    return std::unique_ptr<views::StatusIconLinux>(new AppIndicatorIcon(
        base::StringPrintf("%s%d", kAppIndicatorIdPrefix, indicators_count),
        image, tool_tip));
  } else {
    return std::unique_ptr<views::StatusIconLinux>(
        new Gtk2StatusIcon(image, tool_tip));
  }
}

gfx::Image Gtk2UI::GetIconForContentType(
    const std::string& content_type,
    int size) const {
  // This call doesn't take a reference.
  GtkIconTheme* theme = gtk_icon_theme_get_default();

  std::string content_types[] = {
    content_type, kUnknownContentType
  };

  for (size_t i = 0; i < arraysize(content_types); ++i) {
    ScopedGIcon icon(g_content_type_get_icon(content_types[i].c_str()));
    ScopedGtkIconInfo icon_info(
        gtk_icon_theme_lookup_by_gicon(
            theme, icon.get(), size,
            static_cast<GtkIconLookupFlags>(GTK_ICON_LOOKUP_FORCE_SIZE)));
    if (!icon_info)
      continue;
    ScopedGdkPixbuf pixbuf(gtk_icon_info_load_icon(icon_info.get(), NULL));
    if (!pixbuf)
      continue;

    SkBitmap bitmap = GdkPixbufToImageSkia(pixbuf.get());
    DCHECK_EQ(size, bitmap.width());
    DCHECK_EQ(size, bitmap.height());
    gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    image_skia.MakeThreadSafe();
    return gfx::Image(image_skia);
  }
  return gfx::Image();
}

std::unique_ptr<views::Border> Gtk2UI::CreateNativeBorder(
    views::LabelButton* owning_button,
    std::unique_ptr<views::LabelButtonBorder> border) {
  if (owning_button->GetNativeTheme() != NativeThemeGtk2::instance())
    return std::move(border);

  std::unique_ptr<views::LabelButtonAssetBorder> gtk_border(
      new views::LabelButtonAssetBorder(owning_button->style()));

  gtk_border->set_insets(border->GetInsets());

  static struct {
    const char* idr;
    const char* idr_blue;
    bool focus;
    views::Button::ButtonState state;
  } const paintstate[] = {
    { "IDR_BUTTON_NORMAL",
      "IDR_BLUE_BUTTON_NORMAL",
      false, views::Button::STATE_NORMAL, },
    { "IDR_BUTTON_HOVER",
      "IDR_BLUE_BUTTON_HOVER",
      false, views::Button::STATE_HOVERED, },
    { "IDR_BUTTON_PRESSED",
      "IDR_BLUE_BUTTON_PRESSED",
      false, views::Button::STATE_PRESSED, },
    { "IDR_BUTTON_DISABLED",
      "IDR_BLUE_BUTTON_DISABLED",
      false, views::Button::STATE_DISABLED, },

    { "IDR_BUTTON_FOCUSED_NORMAL",
      "IDR_BLUE_BUTTON_FOCUSED_NORMAL",
      true, views::Button::STATE_NORMAL, },
    { "IDR_BUTTON_FOCUSED_HOVER",
      "IDR_BLUE_BUTTON_FOCUSED_HOVER",
      true, views::Button::STATE_HOVERED, },
    { "IDR_BUTTON_FOCUSED_PRESSED",
      "IDR_BLUE_BUTTON_FOCUSED_PRESSED",
      true, views::Button::STATE_PRESSED, },
    { "IDR_BUTTON_DISABLED",
      "IDR_BLUE_BUTTON_DISABLED",
      true, views::Button::STATE_DISABLED, },
  };

  bool is_blue =
      owning_button->GetClassName() == views::BlueButton::kViewClassName;

  for (unsigned i = 0; i < arraysize(paintstate); i++) {
    views::Painter* painter = nullptr;

    if (border->PaintsButtonState(paintstate[i].focus, paintstate[i].state)) {
      std::string idr = is_blue ? paintstate[i].idr_blue : paintstate[i].idr;
      painter = new GtkButtonPainter(idr);
    }

    gtk_border->SetPainter(paintstate[i].focus, paintstate[i].state, painter);
  }

  return std::move(gtk_border);
}

void Gtk2UI::AddWindowButtonOrderObserver(
    views::WindowButtonOrderObserver* observer) {
  if (!leading_buttons_.empty() || !trailing_buttons_.empty()) {
    observer->OnWindowButtonOrderingChange(leading_buttons_,
                                           trailing_buttons_);
  }

  observer_list_.AddObserver(observer);
}

void Gtk2UI::RemoveWindowButtonOrderObserver(
    views::WindowButtonOrderObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void Gtk2UI::SetWindowButtonOrdering(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  leading_buttons_ = leading_buttons;
  trailing_buttons_ = trailing_buttons;

  FOR_EACH_OBSERVER(views::WindowButtonOrderObserver, observer_list_,
                    OnWindowButtonOrderingChange(leading_buttons_,
                                                 trailing_buttons_));
}

void Gtk2UI::SetNonClientMiddleClickAction(NonClientMiddleClickAction action) {
  middle_click_action_ = action;
}

std::unique_ptr<ui::LinuxInputMethodContext> Gtk2UI::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate,
    bool is_simple) const {
  return std::unique_ptr<ui::LinuxInputMethodContext>(
      new X11InputMethodContextImplGtk2(delegate, is_simple));
}

gfx::FontRenderParams Gtk2UI::GetDefaultFontRenderParams() const {
  static gfx::FontRenderParams params = GetGtkFontRenderParams();
  return params;
}

void Gtk2UI::GetDefaultFontDescription(
    std::string* family_out,
    int* size_pixels_out,
    int* style_out,
    gfx::FontRenderParams* params_out) const {
  *family_out = default_font_family_;
  *size_pixels_out = default_font_size_pixels_;
  *style_out = default_font_style_;
  *params_out = default_font_render_params_;
}

ui::SelectFileDialog* Gtk2UI::CreateSelectFileDialog(
    ui::SelectFileDialog::Listener* listener,
    ui::SelectFilePolicy* policy) const {
  return SelectFileDialogImpl::Create(listener, policy);
}

bool Gtk2UI::UnityIsRunning() {
  return unity::IsRunning();
}

views::LinuxUI::NonClientMiddleClickAction
Gtk2UI::GetNonClientMiddleClickAction() {
  return middle_click_action_;
}

void Gtk2UI::NotifyWindowManagerStartupComplete() {
  // TODO(port) Implement this using _NET_STARTUP_INFO_BEGIN/_NET_STARTUP_INFO
  // from http://standards.freedesktop.org/startup-notification-spec/ instead.
  gdk_notify_startup_complete();
}

bool Gtk2UI::MatchEvent(const ui::Event& event,
                        std::vector<ui::TextEditCommandAuraLinux>* commands) {
  // Ensure that we have a keyboard handler.
  if (!key_bindings_handler_)
    key_bindings_handler_.reset(new Gtk2KeyBindingsHandler);

  return key_bindings_handler_->MatchEvent(event, commands);
}

void Gtk2UI::SetScrollbarColors() {
  thumb_active_color_ = SkColorSetRGB(244, 244, 244);
  thumb_inactive_color_ = SkColorSetRGB(234, 234, 234);
  track_color_ = SkColorSetRGB(211, 211, 211);

  NativeThemeGtk2::instance()->GetChromeStyleColor(
      "scrollbar-slider-prelight-color", &thumb_active_color_);
  NativeThemeGtk2::instance()->GetChromeStyleColor(
      "scrollbar-slider-normal-color", &thumb_inactive_color_);
  NativeThemeGtk2::instance()->GetChromeStyleColor("scrollbar-trough-color",
                                                   &track_color_);
}

void Gtk2UI::LoadGtkValues() {
  // TODO(erg): GtkThemeService had a comment here about having to muck with
  // the raw Prefs object to remove prefs::kCurrentThemeImages or else we'd
  // regress startup time. Figure out how to do that when we can't access the
  // prefs system from here.

  NativeThemeGtk2* theme = NativeThemeGtk2::instance();

  SkColor toolbar_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_LabelBackgroundColor);
  SkColor label_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_LabelEnabledColor);

  colors_[ThemeProperties::COLOR_CONTROL_BACKGROUND] = toolbar_color;
  colors_[ThemeProperties::COLOR_TOOLBAR] = toolbar_color;

  colors_[ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON] =
      color_utils::DeriveDefaultIconColor(label_color);

  colors_[ThemeProperties::COLOR_TAB_TEXT] = label_color;
  colors_[ThemeProperties::COLOR_BOOKMARK_TEXT] = label_color;

  UpdateDefaultFont();

  // Build the various icon tints.
  GetNormalButtonTintHSL(&button_tint_);
  GetNormalEntryForegroundHSL(&entry_tint_);
  GetSelectedEntryForegroundHSL(&selected_entry_tint_);
  SkColor frame_color = BuildFrameColors();

  // The inactive frame color never occurs naturally in the theme, as it is a
  // tinted version of |frame_color|. We generate another color based on the
  // background tab color, with the lightness and saturation moved in the
  // opposite direction. (We don't touch the hue, since there should be subtle
  // hints of the color in the text.)
  color_utils::HSL inactive_tab_text_hsl;
  color_utils::SkColorToHSL(
      theme->GetSystemColor(ui::NativeTheme::kColorId_WindowBackground),
      &inactive_tab_text_hsl);
  inactive_tab_text_hsl.s = kInactiveLuminance;
  inactive_tab_text_hsl.l = kInactiveSaturation;

  colors_[ThemeProperties::COLOR_BACKGROUND_TAB_TEXT] =
      color_utils::HSLToSkColor(inactive_tab_text_hsl, 255);

  // We pick the text and background colors for the NTP out of the colors for a
  // GtkEntry. We do this because GtkEntries background color is never the same
  // as |toolbar_color|, is usually a white, and when it isn't a white,
  // provides sufficient contrast to |toolbar_color|. Try this out with
  // Darklooks, HighContrastInverse or ThinIce.

  SkColor ntp_background =
      theme->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldDefaultBackground);
  SkColor ntp_foreground =
      theme->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldDefaultColor);

  colors_[ThemeProperties::COLOR_NTP_BACKGROUND] = ntp_background;
  colors_[ThemeProperties::COLOR_NTP_TEXT] = ntp_foreground;

  // The NTP header is the color that surrounds the current active thumbnail on
  // the NTP, and acts as the border of the "Recent Links" box. It would be
  // awesome if they were separated so we could use GetBorderColor() for the
  // border around the "Recent Links" section, but matching the frame color is
  // more important.

  colors_[ThemeProperties::COLOR_NTP_HEADER] = frame_color;
  colors_[ThemeProperties::COLOR_NTP_SECTION] = toolbar_color;
  colors_[ThemeProperties::COLOR_NTP_SECTION_TEXT] = label_color;

  SkColor link_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_LinkEnabled);
  colors_[ThemeProperties::COLOR_NTP_LINK] = link_color;
  colors_[ThemeProperties::COLOR_NTP_LINK_UNDERLINE] = link_color;
  colors_[ThemeProperties::COLOR_NTP_SECTION_LINK] = link_color;
  colors_[ThemeProperties::COLOR_NTP_SECTION_LINK_UNDERLINE] = link_color;

  // Generate the colors that we pass to WebKit.
  focus_ring_color_ = frame_color;

  SetScrollbarColors();

  // Some GTK themes only define the text selection colors on the GtkEntry
  // class, so we need to use that for getting selection colors.
  active_selection_bg_color_ =
      theme->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused);
  active_selection_fg_color_ =
      theme->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldSelectionColor);
  inactive_selection_bg_color_ =
      theme->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldReadOnlyBackground);
  inactive_selection_fg_color_ =
      theme->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldReadOnlyColor);

  colors_[ThemeProperties::COLOR_TAB_THROBBER_SPINNING] =
      theme->GetSystemColor(ui::NativeTheme::kColorId_ThrobberSpinningColor);
  colors_[ThemeProperties::COLOR_TAB_THROBBER_WAITING] =
      theme->GetSystemColor(ui::NativeTheme::kColorId_ThrobberWaitingColor);
}

void Gtk2UI::UpdateMaterialDesignColors() {
  // TODO(varkha): This should be merged back into LoadGtkValues() once Material
  // Design is on unconditionally.
  if (ui::MaterialDesignController::IsModeMaterial()) {
    NativeThemeGtk2* theme = NativeThemeGtk2::instance();
    SkColor label_color =
        theme->GetSystemColor(ui::NativeTheme::kColorId_LabelEnabledColor);
    colors_[ThemeProperties::COLOR_BACKGROUND_TAB_TEXT] =
        color_utils::BlendTowardOppositeLuma(label_color, 50);
  }
}

SkColor Gtk2UI::BuildFrameColors() {
  NativeThemeGtk2* theme = NativeThemeGtk2::instance();
  SkColor frame_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_WindowBackground);
  SkColor temp_color;

#if GTK_MAJOR_VERSION == 2
  color_utils::HSL kDefaultFrameShift = { -1, -1, 0.4 };
  GtkStyle* style = gtk_rc_get_style(theme->GetWindow());

  frame_color = color_utils::HSLShift(frame_color, kDefaultFrameShift);
  theme->GetChromeStyleColor("frame-color", &frame_color);

  temp_color = frame_color;
  colors_[ThemeProperties::COLOR_FRAME] = temp_color;

  temp_color = color_utils::HSLShift(
      GdkColorToSkColor(style->bg[GTK_STATE_INSENSITIVE]),
      kDefaultFrameShift);
  theme->GetChromeStyleColor("inactive-frame-color", &temp_color);
  colors_[ThemeProperties::COLOR_FRAME_INACTIVE] = temp_color;

  temp_color = color_utils::HSLShift(
      frame_color,
      kDefaultTintFrameIncognito);
  theme->GetChromeStyleColor("incognito-frame-color", &temp_color);
  colors_[ThemeProperties::COLOR_FRAME_INCOGNITO] = temp_color;

  temp_color = color_utils::HSLShift(
      frame_color,
      kDefaultTintFrameIncognitoInactive);
  theme->GetChromeStyleColor("incognito-inactive-frame-color", &temp_color);
  colors_[ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE] = temp_color;
#else
  const SkBitmap* bitmap;

  bitmap = GetThemeImageNamed(IDR_THEME_FRAME).ToSkBitmap();
  bitmap->lockPixels();
  temp_color = bitmap->getColor(bitmap->width() / 2, bitmap->height() - 1);
  bitmap->unlockPixels();
  colors_[ThemeProperties::COLOR_FRAME] = temp_color;

  bitmap = GetThemeImageNamed(IDR_THEME_FRAME_INACTIVE).ToSkBitmap();
  bitmap->lockPixels();
  temp_color = bitmap->getColor(bitmap->width() / 2, bitmap->height() - 1);
  bitmap->unlockPixels();
  colors_[ThemeProperties::COLOR_FRAME_INACTIVE] = temp_color;

  bitmap = GetThemeImageNamed(IDR_THEME_FRAME_INCOGNITO).ToSkBitmap();
  bitmap->lockPixels();
  temp_color = bitmap->getColor(bitmap->width() / 2, bitmap->height() - 1);
  bitmap->unlockPixels();
  colors_[ThemeProperties::COLOR_FRAME_INCOGNITO] = temp_color;

  bitmap = GetThemeImageNamed(IDR_THEME_FRAME_INCOGNITO_INACTIVE).ToSkBitmap();
  bitmap->lockPixels();
  temp_color = bitmap->getColor(bitmap->width() / 2, bitmap->height() - 1);
  bitmap->unlockPixels();
  colors_[ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE] = temp_color;
#endif

  return frame_color;
}

gfx::Image Gtk2UI::GenerateGtkThemeImage(int id) const {
  gfx::ImageSkiaSource* source = NULL;

  switch (id) {
    case IDR_BACK:
      source = new GtkThemeIconSource(id, "go-previous", true);
      break;
    case IDR_BACK_D:
      source = new GtkThemeIconSource(id, "go-previous", false);
      break;

    case IDR_FORWARD:
      source = new GtkThemeIconSource(id, "go-next", true);
      break;
    case IDR_FORWARD_D:
      source = new GtkThemeIconSource(id, "go-next", false);
      break;

    case IDR_HOME:
      source = new GtkThemeIconSource(id, "go-home", true);
      break;

    case IDR_RELOAD:
      source = new GtkThemeIconSource(id, "view-refresh", true);
      break;
    case IDR_RELOAD_D:
      source = new GtkThemeIconSource(id, "view-refresh", false);
      break;

    case IDR_STOP:
      source = new GtkThemeIconSource(id, "process-stop", true);
      break;
    case IDR_STOP_D:
      source = new GtkThemeIconSource(id, "process-stop", false);
      break;


    // The toolbar bezels don't seem to be in use anymore, remove at your
    // discretion
    case IDR_TOOLBAR_BEZEL_HOVER: {
      gfx::Size size = ResourceBundle::GetSharedInstance().
          GetImageSkiaNamed(id)->size();
      source = new GtkButtonImageSource("IDR_BUTTON_HOVER", size);
      break;
    }
    case IDR_TOOLBAR_BEZEL_PRESSED: {
      gfx::Size size = ResourceBundle::GetSharedInstance().
          GetImageSkiaNamed(id)->size();
      source = new GtkButtonImageSource("IDR_BUTTON_PRESSED", size);
      break;
    }
  }

  if (source)
    return gfx::Image(gfx::ImageSkia(source, 1));

  return gfx::Image();
}

SkBitmap Gtk2UI::GenerateGtkThemeBitmap(int id) const {
  switch (id) {
    case IDR_THEME_TOOLBAR: {
      if (ui::MaterialDesignController::IsModeMaterial())
        break;

      SkBitmap bitmap;
      bitmap.allocN32Pixels(kToolbarImageWidth, kToolbarImageHeight);
      bitmap.eraseColor(
          NativeThemeGtk2::instance()->GetSystemColor(
              ui::NativeTheme::kColorId_LabelBackgroundColor));
      return bitmap;
    }

    case IDR_THEME_TAB_BACKGROUND:
      return GenerateTabImage(IDR_THEME_FRAME);
    case IDR_THEME_TAB_BACKGROUND_INCOGNITO:
      return GenerateTabImage(IDR_THEME_FRAME_INCOGNITO);
    case IDR_FRAME:
    case IDR_THEME_FRAME:
      return GenerateFrameImage(ThemeProperties::COLOR_FRAME,
                                "frame-gradient-color");
    case IDR_FRAME_INACTIVE:
    case IDR_THEME_FRAME_INACTIVE:
      return GenerateFrameImage(ThemeProperties::COLOR_FRAME_INACTIVE,
                                "inactive-frame-gradient-color");
    case IDR_THEME_FRAME_INCOGNITO:
      return GenerateFrameImage(ThemeProperties::COLOR_FRAME_INCOGNITO,
                                "incognito-frame-gradient-color");
    case IDR_THEME_FRAME_INCOGNITO_INACTIVE: {
      return GenerateFrameImage(
          ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE,
          "incognito-inactive-frame-gradient-color");
    }
    // Icons that sit inside the omnibox shouldn't receive TINT_BUTTONS and
    // instead should tint based on the foreground text entry color in GTK+
    // mode because some themes that try to be dark *and* light have very
    // different colors between the omnibox and the normal background area.
    // TODO(erg): Decide what to do about other icons that appear in the
    // omnibox, e.g. content settings icons.
    case IDR_OMNIBOX_CALCULATOR:
    case IDR_OMNIBOX_EXTENSION_APP:
    case IDR_OMNIBOX_HTTP:
    case IDR_OMNIBOX_SEARCH:
    case IDR_OMNIBOX_STAR:
    case IDR_OMNIBOX_TTS: {
      return GenerateTintedIcon(id, entry_tint_);
    }

    // TODO(erg): The dropdown arrow should be tinted because we're injecting
    // various background GTK colors, but the code that accesses them needs to
    // be modified so that they ask their ui::ThemeProvider instead of the
    // ResourceBundle. (i.e. in a light on dark theme, the dropdown arrow will
    // be dark on dark)
    case IDR_MENU_DROPARROW:
    case IDR_BROWSER_ACTIONS_OVERFLOW:
      return GenerateTintedIcon(id, button_tint_);
  }

  return SkBitmap();
}

SkBitmap Gtk2UI::GenerateFrameImage(
    int color_id,
    const char* gradient_name) const {
#if GTK_MAJOR_VERSION == 2
  if (ui::MaterialDesignController::IsModeMaterial())
    return SkBitmap();

  ColorMap::const_iterator it = colors_.find(color_id);
  DCHECK(it != colors_.end());
  SkColor base = it->second;

  // We use two colors: the main color (passed in) and a lightened version of
  // that color (which is supposed to match the light gradient at the top of
  // several GTK+ themes, such as Ambiance, Clearlooks or Bluebird).
  const color_utils::HSL kGtkFrameShift = { -1, -1, 0.58 };
  gfx::Canvas canvas(gfx::Size(kToolbarImageWidth, kToolbarImageHeight),
      1.0f, true);

  SkColor gradient_top_color = color_utils::HSLShift(base, kGtkFrameShift);
  int gradient_size;

  NativeThemeGtk2::instance()->GetChromeStyleColor(gradient_name,
                                                   &gradient_top_color);
  gtk_widget_style_get(NativeThemeGtk2::instance()->GetWindow(),
                       "frame-gradient-size", &gradient_size,
                       NULL);

  if (gradient_size) {
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    paint.setShader(gfx::CreateGradientShader(
        0, gradient_size, gradient_top_color, base));

    canvas.DrawRect(gfx::Rect(0, 0, kToolbarImageWidth, gradient_size), paint);
  }

  canvas.FillRect(gfx::Rect(0, gradient_size, kToolbarImageWidth,
                            kToolbarImageHeight - gradient_size), base);
  return canvas.ExtractImageRep().sk_bitmap();
#else
  // Render a GtkHeaderBar as our title bar, cropping out any curved edges on
  // the left and right sides. Also remove the bottom border for good measure.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kToolbarImageWidth, 40);
  bitmap.eraseColor(0);

  static GtkWidget* title = NULL;
  if (!title) {
    title = gtk_header_bar_new();
    gtk_widget_set_size_request(title, kToolbarImageWidth * 2, 48);

    GtkWidget* window = gtk_offscreen_window_new();
    gtk_container_add(GTK_CONTAINER(window), title);

    gtk_widget_show_all(window);
  }

  cairo_surface_t* surface = cairo_image_surface_create_for_data(
    static_cast<unsigned char*>(bitmap.getAddr(0, 0)),
    CAIRO_FORMAT_ARGB32,
    bitmap.width(), bitmap.height(),
    bitmap.width() * 4);
  cairo_t* cr = cairo_create(surface);
  cairo_translate(cr, kToolbarImageWidth / -2, 0);
  gtk_widget_draw(title, cr);
  cairo_destroy(cr);
  cairo_surface_destroy(surface);

  switch (color_id) {
    case ThemeProperties::COLOR_FRAME_INACTIVE:
      return SkBitmapOperations::CreateHSLShiftedBitmap(bitmap,
          kDefaultTintFrameInactive);
    case ThemeProperties::COLOR_FRAME_INCOGNITO:
      return SkBitmapOperations::CreateHSLShiftedBitmap(bitmap,
          kDefaultTintFrameIncognito);
    case ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE:
      return SkBitmapOperations::CreateHSLShiftedBitmap(bitmap,
          kDefaultTintFrameIncognitoInactive);
  }

  return bitmap;
#endif
}

SkBitmap Gtk2UI::GenerateTabImage(int base_id) const {
  if (ui::MaterialDesignController::IsModeMaterial())
    return SkBitmap();

  const SkBitmap* base_image = GetThemeImageNamed(base_id).ToSkBitmap();
  SkBitmap bg_tint = SkBitmapOperations::CreateHSLShiftedBitmap(
      *base_image, kDefaultTintBackgroundTab);
  return SkBitmapOperations::CreateTiledBitmap(
      bg_tint, 0, 0, bg_tint.width(), bg_tint.height());
}

SkBitmap Gtk2UI::GenerateTintedIcon(
    int base_id,
    const color_utils::HSL& tint) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return SkBitmapOperations::CreateHSLShiftedBitmap(
      rb.GetImageNamed(base_id).AsBitmap(), tint);
}

void Gtk2UI::GetNormalButtonTintHSL(color_utils::HSL* tint) const {
  NativeThemeGtk2* theme = NativeThemeGtk2::instance();

  SkColor accent_color =
      theme->GetSystemColor(
          ui::NativeTheme::kColorId_ButtonHighlightColor);
  SkColor text_color =
      theme->GetSystemColor(
          ui::NativeTheme::kColorId_LabelEnabledColor);
  SkColor base_color =
      theme->GetSystemColor(
          ui::NativeTheme::kColorId_LabelBackgroundColor);

  PickButtonTintFromColors(accent_color, text_color, base_color, tint);
}

void Gtk2UI::GetNormalEntryForegroundHSL(color_utils::HSL* tint) const {
  NativeThemeGtk2* theme = NativeThemeGtk2::instance();

  SkColor accent_color =
      theme->GetSystemColor(
          ui::NativeTheme::kColorId_ButtonHighlightColor);
  SkColor text_color =
      theme->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldDefaultColor);
  SkColor base_color =
      theme->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldDefaultBackground);

  PickButtonTintFromColors(accent_color, text_color, base_color, tint);
}

void Gtk2UI::GetSelectedEntryForegroundHSL(color_utils::HSL* tint) const {
  // The simplest of all the tints. We just use the selected text in the entry
  // since the icons tinted this way will only be displayed against
  // base[GTK_STATE_SELECTED].
  SkColor color =
      NativeThemeGtk2::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldSelectionColor);

  color_utils::SkColorToHSL(color, tint);
}

void Gtk2UI::ClearAllThemeData() {
  gtk_images_.clear();
}

void Gtk2UI::UpdateDefaultFont() {
  PangoContext* pc = gtk_widget_get_pango_context(
    NativeThemeGtk2::instance()->GetLabel());
  const PangoFontDescription* desc = pango_context_get_font_description(pc);

  // Use gfx::FontRenderParams to select a family and determine the rendering
  // settings.
  gfx::FontRenderParamsQuery query;
  query.families = base::SplitString(pango_font_description_get_family(desc),
                                     ",", base::TRIM_WHITESPACE,
                                     base::SPLIT_WANT_ALL);

  if (pango_font_description_get_size_is_absolute(desc)) {
    // If the size is absolute, it's specified in Pango units. There are
    // PANGO_SCALE Pango units in a device unit (pixel).
    const int size_pixels = pango_font_description_get_size(desc) / PANGO_SCALE;
    default_font_size_pixels_ = size_pixels;
    query.pixel_size = size_pixels;
  } else {
    // Non-absolute sizes are in points (again scaled by PANGO_SIZE).
    // Round the value when converting to pixels to match GTK's logic.
    const double size_points = pango_font_description_get_size(desc) /
        static_cast<double>(PANGO_SCALE);
    default_font_size_pixels_ = static_cast<int>(
        GetPixelsInPoint(device_scale_factor_) * size_points + 0.5);
    query.point_size = static_cast<int>(size_points);
  }

  query.style = gfx::Font::NORMAL;
  // TODO(davemoore): Support weights other than bold?
  if (pango_font_description_get_weight(desc) == PANGO_WEIGHT_BOLD)
    query.style |= gfx::Font::BOLD;
  // TODO(davemoore): What about PANGO_STYLE_OBLIQUE?
  if (pango_font_description_get_style(desc) == PANGO_STYLE_ITALIC)
    query.style |= gfx::Font::ITALIC;

  default_font_render_params_ =
      gfx::GetFontRenderParams(query, &default_font_family_);
  default_font_style_ = query.style;
}

void Gtk2UI::ResetStyle() {
  ClearAllThemeData();
  LoadGtkValues();
  // TODO(varkha): There will be no need to call UpdateMaterialDesignColors()
  // once Material Design is on unconditionally.
  UpdateMaterialDesignColors();
  NativeThemeGtk2::instance()->NotifyObservers();
}

void Gtk2UI::UpdateDeviceScaleFactor(float device_scale_factor) {
  device_scale_factor_ = device_scale_factor;
  UpdateDefaultFont();
}

float Gtk2UI::GetDeviceScaleFactor() const {
  if (display::Display::HasForceDeviceScaleFactor())
    return display::Display::GetForcedDeviceScaleFactor();
  const int kCSSDefaultDPI = 96;
  const float scale = GetDPI() / kCSSDefaultDPI;

  // Blacklist scaling factors <130% (crbug.com/484400) and round
  // to 1 decimal to prevent rendering problems (crbug.com/485183).
  return scale < 1.3f ? 1.0f : roundf(scale * 10) / 10;
}

}  // namespace libgtk2ui

views::LinuxUI* BuildGtk2UI() {
  return new libgtk2ui::Gtk2UI;
}
