// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/gtk_ui.h"

#include <X11/Xcursor/Xcursor.h>
#include <dlfcn.h>
#include <math.h>
#include <pango/pango.h>

#include <cmath>
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
#include "chrome/browser/ui/libgtkui/app_indicator_icon.h"
#include "chrome/browser/ui/libgtkui/chrome_gtk_frame.h"
#include "chrome/browser/ui/libgtkui/gtk_event_loop.h"
#include "chrome/browser/ui/libgtkui/gtk_key_bindings_handler.h"
#include "chrome/browser/ui/libgtkui/gtk_status_icon.h"
#include "chrome/browser/ui/libgtkui/gtk_util.h"
#include "chrome/browser/ui/libgtkui/print_dialog_gtk.h"
#include "chrome/browser/ui/libgtkui/printing_gtk_util.h"
#include "chrome/browser/ui/libgtkui/select_file_dialog_impl.h"
#include "chrome/browser/ui/libgtkui/skia_utils_gtk.h"
#include "chrome/browser/ui/libgtkui/unity_service.h"
#include "chrome/browser/ui/libgtkui/x11_input_method_context_impl_gtk.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "printing/features/features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/linux_ui/window_button_order_observer.h"
#include "ui/views/resources/grit/views_resources.h"

#if GTK_MAJOR_VERSION == 2
#include "chrome/browser/ui/libgtkui/native_theme_gtk2.h"  // nogncheck
#elif GTK_MAJOR_VERSION == 3
#include "chrome/browser/ui/libgtkui/native_theme_gtk3.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
#include "printing/printing_context_linux.h"
#endif
#if defined(USE_GCONF)
#include "chrome/browser/ui/libgtkui/gconf_listener.h"
#endif

// A minimized port of GtkThemeService into something that can provide colors
// and images for aura.
//
// TODO(erg): There's still a lot that needs ported or done for the first time:
//
// - Render and inject the omnibox background.
// - Make sure to test with a light on dark theme, too.

namespace libgtkui {

namespace {

const double kDefaultDPI = 96;

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

    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        static_cast<unsigned char*>(border.getAddr(0, 0)), CAIRO_FORMAT_ARGB32,
        width, height, width * 4);
    cairo_t* cr = cairo_create(surface);

#if GTK_MAJOR_VERSION == 2
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

    if (focus_)
      GTK_WIDGET_SET_FLAGS(button, GTK_HAS_FOCUS);

    int w, h;
    GdkPixmap* pixmap;

    {
      // http://crbug.com/346740
      ANNOTATE_SCOPED_MEMORY_LEAK;
      pixmap = gtk_widget_get_snapshot(button, nullptr);
    }

    gdk_drawable_get_size(GDK_DRAWABLE(pixmap), &w, &h);
    GdkColormap* colormap = gdk_drawable_get_colormap(pixmap);
    GdkPixbuf* pixbuf = gdk_pixbuf_get_from_drawable(
        nullptr, GDK_DRAWABLE(pixmap), colormap, 0, 0, 0, 0, w, h);

    gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
    cairo_paint(cr);

    g_object_unref(pixbuf);
    g_object_unref(pixmap);

    gtk_widget_destroy(window);
#else
    ScopedStyleContext context = GetStyleContextFromCss(
        is_blue_ ? "GtkButton#button.default.suggested-action"
                 : "GtkButton#button");
    GtkStateFlags state_flags = StateToStateFlags(state_);
    if (focus_) {
      state_flags =
          static_cast<GtkStateFlags>(state_flags | GTK_STATE_FLAG_FOCUSED);
    }
    gtk_style_context_set_state(context, state_flags);
    gtk_render_background(context, cr, 0, 0, width, height);
    gtk_render_frame(context, cr, 0, 0, width, height);
    if (focus_) {
      gfx::Rect focus_rect(width, height);

      if (!GtkVersionCheck(3, 14)) {
        gint focus_pad;
        gtk_style_context_get_style(context, "focus-padding", &focus_pad,
                                    nullptr);
        focus_rect.Inset(focus_pad, focus_pad);

        if (state_ == ui::NativeTheme::kPressed) {
          gint child_displacement_x, child_displacement_y;
          gboolean displace_focus;
          gtk_style_context_get_style(
              context, "child-displacement-x", &child_displacement_x,
              "child-displacement-y", &child_displacement_y, "displace-focus",
              &displace_focus, nullptr);
          if (displace_focus)
            focus_rect.Offset(child_displacement_x, child_displacement_y);
        }
      }

      if (!GtkVersionCheck(3, 20)) {
        GtkBorder border;
        gtk_style_context_get_border(context, state_flags, &border);
        focus_rect.Inset(border.left, border.top, border.right, border.bottom);
      }

      gtk_render_focus(context, cr, focus_rect.x(), focus_rect.y(),
                       focus_rect.width(), focus_rect.height());
    }
#endif

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

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
  void operator()(void* ptr) { g_object_unref(ptr); }
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

// Returns a gfx::FontRenderParams corresponding to GTK's configuration.
gfx::FontRenderParams GetGtkFontRenderParams() {
  GtkSettings* gtk_settings = gtk_settings_get_default();
  CHECK(gtk_settings);
  gint antialias = 0;
  gint hinting = 0;
  gchar* hint_style = nullptr;
  gchar* rgba = nullptr;
  g_object_get(gtk_settings, "gtk-xft-antialias", &antialias, "gtk-xft-hinting",
               &hinting, "gtk-xft-hintstyle", &hint_style, "gtk-xft-rgba",
               &rgba, nullptr);

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

float GtkDpiToScaleFactor(int dpi) {
  // GTK multiplies the DPI by 1024 before storing it.
  return dpi / (1024 * kDefaultDPI);
}

gint GetGdkScreenSettingInt(const char* setting_name) {
  GValue value = G_VALUE_INIT;
  g_value_init(&value, G_TYPE_INT);
  if (!gdk_screen_get_setting(gdk_screen_get_default(), setting_name, &value))
    return -1;
  return g_value_get_int(&value);
}

float GetScaleFromGdkScreenSettings() {
  gint window_scale = GetGdkScreenSettingInt("gdk-window-scaling-factor");
  if (window_scale <= 0)
    return -1;
  gint font_dpi = GetGdkScreenSettingInt("gdk-unscaled-dpi");
  if (font_dpi <= 0)
    return -1;
  return window_scale * GtkDpiToScaleFactor(font_dpi);
}

float GetScaleFromXftDPI() {
  GtkSettings* gtk_settings = gtk_settings_get_default();
  CHECK(gtk_settings);
  gint gtk_dpi = -1;
  g_object_get(gtk_settings, "gtk-xft-dpi", &gtk_dpi, nullptr);
  if (gtk_dpi <= 0)
    return -1;
  return GtkDpiToScaleFactor(gtk_dpi);
}

float GetRawDeviceScaleFactor() {
  if (display::Display::HasForceDeviceScaleFactor())
    return display::Display::GetForcedDeviceScaleFactor();

  float scale = GetScaleFromGdkScreenSettings();
  if (scale > 0)
    return scale;

  scale = GetScaleFromXftDPI();
  if (scale > 0)
    return scale;

  return 1;
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

#if GTK_MAJOR_VERSION > 2
// COLOR_TOOLBAR_TOP_SEPARATOR represents the border between tabs and the
// frame, as well as the border between tabs and the toolbar.  For this
// reason, it is difficult to calculate the One True Color that works well on
// all themes and is opaque.  However, we can cheat to get a good color that
// works well for both borders.  The idea is we have two variables: alpha and
// lightness.  And we have two constraints (on lightness):
// 1. the border color, when painted on |header_bg|, should give |header_fg|
// 2. the border color, when painted on |tab_bg|, should give |tab_fg|
// This gives the equations:
// alpha*lightness + (1 - alpha)*header_bg = header_fg
// alpha*lightness + (1 - alpha)*tab_bg = tab_fg
// The algorithm below is just a result of solving those equations for alpha
// and lightness.  If a problem is encountered, like division by zero, or
// |a| or |l| not in [0, 1], then fallback on |header_fg| or |tab_fg|.
SkColor GetToolbarTopSeparatorColor(SkColor header_fg,
                                    SkColor header_bg,
                                    SkColor tab_fg,
                                    SkColor tab_bg) {
  using namespace color_utils;

  SkColor default_color = SkColorGetA(header_fg) ? header_fg : tab_fg;
  if (!SkColorGetA(default_color))
    return SK_ColorTRANSPARENT;

  auto get_lightness = [](SkColor color) {
    HSL hsl;
    SkColorToHSL(color, &hsl);
    return hsl.l;
  };

  double f1 = get_lightness(GetResultingPaintColor(header_fg, header_bg));
  double b1 = get_lightness(header_bg);
  double f2 = get_lightness(GetResultingPaintColor(tab_fg, tab_bg));
  double b2 = get_lightness(tab_bg);

  if (b1 == b2)
    return default_color;
  double a = (f1 - f2 - b1 + b2) / (b2 - b1);
  if (a == 0)
    return default_color;
  double l = (f1 - (1 - a) * b1) / a;
  if (a < 0 || a > 1 || l < 0 || l > 1)
    return default_color;
  // Take the hue and saturation from |default_color|, but use the
  // calculated lightness.
  HSL border;
  SkColorToHSL(default_color, &border);
  border.l = l;
  return HSLToSkColor(border, a * 0xff);
}
#endif

}  // namespace

GtkUi::GtkUi() : middle_click_action_(GetDefaultMiddleClickAction()) {
#if GTK_MAJOR_VERSION > 2
  // Force Gtk to use Xwayland if it would have used wayland.  libgtkui assumes
  // the use of X11 (eg. X11InputMethodContextImplGtk) and will crash under
  // other backends.
  // TODO(thomasanderson): Change this logic once Wayland support is added.
  static auto* _gdk_set_allowed_backends =
      reinterpret_cast<void (*)(const gchar*)>(
          dlsym(GetGdkSharedLibrary(), "gdk_set_allowed_backends"));
  if (GtkVersionCheck(3, 10))
    DCHECK(_gdk_set_allowed_backends);
  if (_gdk_set_allowed_backends)
    _gdk_set_allowed_backends("x11");
#endif
  GtkInitFromCommandLine(*base::CommandLine::ForCurrentProcess());
#if GTK_MAJOR_VERSION == 2
  native_theme_ = NativeThemeGtk2::instance();
  fake_window_ = chrome_gtk_frame_new();
  gtk_widget_realize(fake_window_);  // Is this necessary?
#elif GTK_MAJOR_VERSION == 3
  native_theme_ = NativeThemeGtk3::instance();
  (void)fake_window_;  // Silence the unused warning.
#else
#error "Unsupported GTK version"
#endif
}

GtkUi::~GtkUi() {
#if GTK_MAJOR_VERSION == 2
  gtk_widget_destroy(fake_window_);
#endif
}

void OnThemeChanged(GObject* obj, GParamSpec* param, GtkUi* gtkui) {
  gtkui->ResetStyle();
}

void GtkUi::Initialize() {
  GtkSettings* settings = gtk_settings_get_default();
  g_signal_connect_after(settings, "notify::gtk-theme-name",
                         G_CALLBACK(OnThemeChanged), this);
  g_signal_connect_after(settings, "notify::gtk-icon-theme-name",
                         G_CALLBACK(OnThemeChanged), this);

  LoadGtkValues();

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
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

bool GtkUi::GetTint(int id, color_utils::HSL* tint) const {
  switch (id) {
    // Tints for which the cross-platform default is fine. Before adding new
    // values here, specifically verify they work well on Linux.
    case ThemeProperties::TINT_BACKGROUND_TAB:
    // TODO(estade): Return something useful for TINT_BUTTONS so that chrome://
    // page icons are colored appropriately.
    case ThemeProperties::TINT_BUTTONS:
      break;
    default:
      // Assume any tints not specifically verified on Linux aren't usable.
      // TODO(pkasting): Try to remove values from |colors_| that could just be
      // added to the group above instead.
      NOTREACHED();
  }
  return false;
}

bool GtkUi::GetColor(int id, SkColor* color) const {
  ColorMap::const_iterator it = colors_.find(id);
  if (it != colors_.end()) {
    *color = it->second;
    return true;
  }

  return false;
}

SkColor GtkUi::GetFocusRingColor() const {
  return focus_ring_color_;
}

SkColor GtkUi::GetThumbActiveColor() const {
  return thumb_active_color_;
}

SkColor GtkUi::GetThumbInactiveColor() const {
  return thumb_inactive_color_;
}

SkColor GtkUi::GetTrackColor() const {
  return track_color_;
}

SkColor GtkUi::GetActiveSelectionBgColor() const {
  return active_selection_bg_color_;
}

SkColor GtkUi::GetActiveSelectionFgColor() const {
  return active_selection_fg_color_;
}

SkColor GtkUi::GetInactiveSelectionBgColor() const {
  return inactive_selection_bg_color_;
}

SkColor GtkUi::GetInactiveSelectionFgColor() const {
  return inactive_selection_fg_color_;
}

double GtkUi::GetCursorBlinkInterval() const {
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
  g_object_get(gtk_settings_get_default(), "gtk-cursor-blink-time",
               &cursor_blink_time, "gtk-cursor-blink", &cursor_blink, nullptr);
  return cursor_blink ? (cursor_blink_time / kGtkCursorBlinkCycleFactor) : 0.0;
}

ui::NativeTheme* GtkUi::GetNativeTheme(aura::Window* window) const {
  ui::NativeTheme* native_theme_override = nullptr;
  if (!native_theme_overrider_.is_null())
    native_theme_override = native_theme_overrider_.Run(window);

  if (native_theme_override)
    return native_theme_override;

  return native_theme_;
}

void GtkUi::SetNativeThemeOverride(const NativeThemeGetter& callback) {
  native_theme_overrider_ = callback;
}

bool GtkUi::GetDefaultUsesSystemTheme() const {
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

void GtkUi::SetDownloadCount(int count) const {
  if (unity::IsRunning())
    unity::SetDownloadCount(count);
}

void GtkUi::SetProgressFraction(float percentage) const {
  if (unity::IsRunning())
    unity::SetProgressFraction(percentage);
}

bool GtkUi::IsStatusIconSupported() const {
  return true;
}

std::unique_ptr<views::StatusIconLinux> GtkUi::CreateLinuxStatusIcon(
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

gfx::Image GtkUi::GetIconForContentType(const std::string& content_type,
                                        int size) const {
  // This call doesn't take a reference.
  GtkIconTheme* theme = gtk_icon_theme_get_default();

  std::string content_types[] = {content_type, kUnknownContentType};

  for (size_t i = 0; i < arraysize(content_types); ++i) {
    ScopedGIcon icon(g_content_type_get_icon(content_types[i].c_str()));
    ScopedGtkIconInfo icon_info(gtk_icon_theme_lookup_by_gicon(
        theme, icon.get(), size,
        static_cast<GtkIconLookupFlags>(GTK_ICON_LOOKUP_FORCE_SIZE)));
    if (!icon_info)
      continue;
    ScopedGdkPixbuf pixbuf(gtk_icon_info_load_icon(icon_info.get(), nullptr));
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

std::unique_ptr<views::Border> GtkUi::CreateNativeBorder(
    views::LabelButton* owning_button,
    std::unique_ptr<views::LabelButtonBorder> border) {
  if (owning_button->GetNativeTheme() != native_theme_)
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
      {
          "IDR_BUTTON_NORMAL", "IDR_BLUE_BUTTON_NORMAL", false,
          views::Button::STATE_NORMAL,
      },
      {
          "IDR_BUTTON_HOVER", "IDR_BLUE_BUTTON_HOVER", false,
          views::Button::STATE_HOVERED,
      },
      {
          "IDR_BUTTON_PRESSED", "IDR_BLUE_BUTTON_PRESSED", false,
          views::Button::STATE_PRESSED,
      },
      {
          "IDR_BUTTON_DISABLED", "IDR_BLUE_BUTTON_DISABLED", false,
          views::Button::STATE_DISABLED,
      },

      {
          "IDR_BUTTON_FOCUSED_NORMAL", "IDR_BLUE_BUTTON_FOCUSED_NORMAL", true,
          views::Button::STATE_NORMAL,
      },
      {
          "IDR_BUTTON_FOCUSED_HOVER", "IDR_BLUE_BUTTON_FOCUSED_HOVER", true,
          views::Button::STATE_HOVERED,
      },
      {
          "IDR_BUTTON_FOCUSED_PRESSED", "IDR_BLUE_BUTTON_FOCUSED_PRESSED", true,
          views::Button::STATE_PRESSED,
      },
      {
          "IDR_BUTTON_DISABLED", "IDR_BLUE_BUTTON_DISABLED", true,
          views::Button::STATE_DISABLED,
      },
  };

  bool is_blue =
      owning_button->GetClassName() == views::BlueButton::kViewClassName;

  for (unsigned i = 0; i < arraysize(paintstate); i++) {
    std::string idr = is_blue ? paintstate[i].idr_blue : paintstate[i].idr;
    gtk_border->SetPainter(
        paintstate[i].focus, paintstate[i].state,
        border->PaintsButtonState(paintstate[i].focus, paintstate[i].state)
            ? base::MakeUnique<GtkButtonPainter>(idr)
            : nullptr);
  }

  return std::move(gtk_border);
}

void GtkUi::AddWindowButtonOrderObserver(
    views::WindowButtonOrderObserver* observer) {
  if (!leading_buttons_.empty() || !trailing_buttons_.empty()) {
    observer->OnWindowButtonOrderingChange(leading_buttons_, trailing_buttons_);
  }

  observer_list_.AddObserver(observer);
}

void GtkUi::RemoveWindowButtonOrderObserver(
    views::WindowButtonOrderObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void GtkUi::SetWindowButtonOrdering(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  leading_buttons_ = leading_buttons;
  trailing_buttons_ = trailing_buttons;

  for (views::WindowButtonOrderObserver& observer : observer_list_)
    observer.OnWindowButtonOrderingChange(leading_buttons_, trailing_buttons_);
}

void GtkUi::SetNonClientMiddleClickAction(NonClientMiddleClickAction action) {
  middle_click_action_ = action;
}

std::unique_ptr<ui::LinuxInputMethodContext> GtkUi::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate,
    bool is_simple) const {
  return std::unique_ptr<ui::LinuxInputMethodContext>(
      new X11InputMethodContextImplGtk2(delegate, is_simple));
}

gfx::FontRenderParams GtkUi::GetDefaultFontRenderParams() const {
  static gfx::FontRenderParams params = GetGtkFontRenderParams();
  return params;
}

void GtkUi::GetDefaultFontDescription(std::string* family_out,
                                      int* size_pixels_out,
                                      int* style_out,
                                      gfx::Font::Weight* weight_out,
                                      gfx::FontRenderParams* params_out) const {
  *family_out = default_font_family_;
  *size_pixels_out = default_font_size_pixels_;
  *style_out = default_font_style_;
  *weight_out = default_font_weight_;
  *params_out = default_font_render_params_;
}

ui::SelectFileDialog* GtkUi::CreateSelectFileDialog(
    ui::SelectFileDialog::Listener* listener,
    ui::SelectFilePolicy* policy) const {
  return SelectFileDialogImpl::Create(listener, policy);
}

bool GtkUi::UnityIsRunning() {
  return unity::IsRunning();
}

views::LinuxUI::NonClientMiddleClickAction
GtkUi::GetNonClientMiddleClickAction() {
  return middle_click_action_;
}

void GtkUi::NotifyWindowManagerStartupComplete() {
  // TODO(port) Implement this using _NET_STARTUP_INFO_BEGIN/_NET_STARTUP_INFO
  // from http://standards.freedesktop.org/startup-notification-spec/ instead.
  gdk_notify_startup_complete();
}

bool GtkUi::MatchEvent(const ui::Event& event,
                       std::vector<ui::TextEditCommandAuraLinux>* commands) {
  // Ensure that we have a keyboard handler.
  if (!key_bindings_handler_)
    key_bindings_handler_.reset(new Gtk2KeyBindingsHandler);

  return key_bindings_handler_->MatchEvent(event, commands);
}

void GtkUi::SetScrollbarColors() {
  thumb_active_color_ = SkColorSetRGB(244, 244, 244);
  thumb_inactive_color_ = SkColorSetRGB(234, 234, 234);
  track_color_ = SkColorSetRGB(211, 211, 211);

  GetChromeStyleColor("scrollbar-slider-prelight-color", &thumb_active_color_);
  GetChromeStyleColor("scrollbar-slider-normal-color", &thumb_inactive_color_);
  GetChromeStyleColor("scrollbar-trough-color", &track_color_);
}

void GtkUi::LoadGtkValues() {
  // TODO(erg): GtkThemeService had a comment here about having to muck with
  // the raw Prefs object to remove prefs::kCurrentThemeImages or else we'd
  // regress startup time. Figure out how to do that when we can't access the
  // prefs system from here.

  UpdateDeviceScaleFactor();
  UpdateCursorTheme();

#if GTK_MAJOR_VERSION == 2
  const color_utils::HSL kDefaultFrameShift = {-1, -1, 0.4};
  SkColor frame_color =
      native_theme_->GetSystemColor(ui::NativeTheme::kColorId_WindowBackground);
  frame_color = color_utils::HSLShift(frame_color, kDefaultFrameShift);
  GetChromeStyleColor("frame-color", &frame_color);
  colors_[ThemeProperties::COLOR_FRAME] = frame_color;

  GtkStyle* style = gtk_rc_get_style(fake_window_);
  SkColor temp_color = color_utils::HSLShift(
      GdkColorToSkColor(style->bg[GTK_STATE_INSENSITIVE]), kDefaultFrameShift);
  GetChromeStyleColor("inactive-frame-color", &temp_color);
  colors_[ThemeProperties::COLOR_FRAME_INACTIVE] = temp_color;

  temp_color = color_utils::HSLShift(frame_color, kDefaultTintFrameIncognito);
  GetChromeStyleColor("incognito-frame-color", &temp_color);
  colors_[ThemeProperties::COLOR_FRAME_INCOGNITO] = temp_color;

  temp_color =
      color_utils::HSLShift(frame_color, kDefaultTintFrameIncognitoInactive);
  GetChromeStyleColor("incognito-inactive-frame-color", &temp_color);
  colors_[ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE] = temp_color;

  SkColor tab_color =
      native_theme_->GetSystemColor(ui::NativeTheme::kColorId_DialogBackground);
  SkColor label_color = native_theme_->GetSystemColor(
      ui::NativeTheme::kColorId_LabelEnabledColor);

  colors_[ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON] =
      color_utils::DeriveDefaultIconColor(label_color);

  colors_[ThemeProperties::COLOR_TAB_TEXT] = label_color;
  colors_[ThemeProperties::COLOR_BOOKMARK_TEXT] = label_color;
  colors_[ThemeProperties::COLOR_BACKGROUND_TAB_TEXT] =
      color_utils::BlendTowardOppositeLuma(label_color, 50);

  inactive_selection_bg_color_ = native_theme_->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldReadOnlyBackground);
  inactive_selection_fg_color_ = native_theme_->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldReadOnlyColor);

  // We pick the text and background colors for the NTP out of the
  // colors for a GtkEntry. We do this because GtkEntries background
  // color is never the same as |tab_color|, is usually a white,
  // and when it isn't a white, provides sufficient contrast to
  // |tab_color|. Try this out with Darklooks, HighContrastInverse
  // or ThinIce.
  colors_[ThemeProperties::COLOR_NTP_BACKGROUND] =
      native_theme_->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldDefaultBackground);
  colors_[ThemeProperties::COLOR_NTP_TEXT] = native_theme_->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
  // The NTP header is the color that surrounds the current active
  // thumbnail on the NTP, and acts as the border of the "Recent
  // Links" box. It would be awesome if they were separated so we
  // could use GetBorderColor() for the border around the "Recent
  // Links" section, but matching the frame color is more important.
  colors_[ThemeProperties::COLOR_NTP_HEADER] =
      colors_[ThemeProperties::COLOR_FRAME];
#else
  std::string header_selector = GtkVersionCheck(3, 10)
                                    ? "#headerbar.header-bar.titlebar"
                                    : "GtkMenuBar#menubar";
  SkColor frame_color = GetBgColor(header_selector);
  SkColor frame_color_inactive = GetBgColor(header_selector + ":backdrop");
  colors_[ThemeProperties::COLOR_FRAME] = frame_color;
  colors_[ThemeProperties::COLOR_FRAME_INACTIVE] = frame_color_inactive;
  colors_[ThemeProperties::COLOR_FRAME_INCOGNITO] =
      color_utils::HSLShift(frame_color, kDefaultTintFrameIncognito);
  colors_[ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE] =
      color_utils::HSLShift(frame_color_inactive, kDefaultTintFrameIncognito);

  SkColor tab_color = GetBgColor("");
  SkColor tab_text_color = GetFgColor("GtkLabel");

  colors_[ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON] = tab_text_color;

  colors_[ThemeProperties::COLOR_TAB_TEXT] = tab_text_color;
  colors_[ThemeProperties::COLOR_BOOKMARK_TEXT] = tab_text_color;
  colors_[ThemeProperties::COLOR_BACKGROUND_TAB_TEXT] =
      color_utils::BlendTowardOppositeLuma(tab_text_color, 50);

  SkColor location_bar_border = GetBorderColor("GtkEntry#entry");
  if (SkColorGetA(location_bar_border))
    colors_[ThemeProperties::COLOR_LOCATION_BAR_BORDER] = location_bar_border;

  inactive_selection_bg_color_ = GetSelectionBgColor(
      GtkVersionCheck(3, 20) ? "GtkTextView#textview.view:backdrop "
                               "#text:backdrop #selection:backdrop"
                             : "GtkTextView.view:selected:backdrop");
  inactive_selection_fg_color_ =
      GetFgColor(GtkVersionCheck(3, 20) ? "GtkTextView#textview.view:backdrop "
                                          "#text:backdrop #selection:backdrop"
                                        : "GtkTextView.view:selected:backdrop");

  SkColor tab_border = GetBorderColor("GtkButton#button");
  colors_[ThemeProperties::COLOR_DETACHED_BOOKMARK_BAR_BACKGROUND] = tab_color;
  colors_[ThemeProperties::COLOR_BOOKMARK_BAR_INSTRUCTIONS_TEXT] =
      tab_text_color;
  // Separates the toolbar from the bookmark bar or butter bars.
  colors_[ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR] = tab_border;
  // Separates entries in the downloads bar.
  colors_[ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR] = tab_border;
  // Separates the bookmark bar from the web content.
  colors_[ThemeProperties::COLOR_DETACHED_BOOKMARK_BAR_SEPARATOR] = tab_border;

  // These colors represent the border drawn around tabs and between
  // the tabstrip and toolbar.
  SkColor toolbar_top_separator = GetToolbarTopSeparatorColor(
      GetBorderColor(header_selector + " GtkButton#button"), frame_color,
      tab_border, tab_color);
  SkColor toolbar_top_separator_inactive = GetToolbarTopSeparatorColor(
      GetBorderColor(header_selector + ":backdrop GtkButton#button"),
      frame_color_inactive, tab_border, tab_color);
  // Unlike with toolbars, we always want a border around tabs, so let
  // ThemeService choose the border color if the theme doesn't provide one.
  if (SkColorGetA(toolbar_top_separator) &&
      SkColorGetA(toolbar_top_separator_inactive)) {
    colors_[ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR] =
        toolbar_top_separator;
    colors_[ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_INACTIVE] =
        toolbar_top_separator_inactive;
  }

  colors_[ThemeProperties::COLOR_NTP_BACKGROUND] =
      native_theme_->GetSystemColor(
          ui::NativeTheme::kColorId_TextfieldDefaultBackground);
  colors_[ThemeProperties::COLOR_NTP_TEXT] = native_theme_->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
  colors_[ThemeProperties::COLOR_NTP_HEADER] =
      GetBorderColor("GtkButton#button");
#endif

  colors_[ThemeProperties::COLOR_TOOLBAR] = tab_color;
  colors_[ThemeProperties::COLOR_CONTROL_BACKGROUND] = tab_color;

  colors_[ThemeProperties::COLOR_NTP_LINK] = native_theme_->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused);

  // Generate the colors that we pass to WebKit.
  SetScrollbarColors();
  focus_ring_color_ = native_theme_->GetSystemColor(
      ui::NativeTheme::kColorId_FocusedBorderColor);

  // Some GTK themes only define the text selection colors on the GtkEntry
  // class, so we need to use that for getting selection colors.
  active_selection_bg_color_ = native_theme_->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused);
  active_selection_fg_color_ = native_theme_->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldSelectionColor);

  colors_[ThemeProperties::COLOR_TAB_THROBBER_SPINNING] =
      native_theme_->GetSystemColor(
          ui::NativeTheme::kColorId_ThrobberSpinningColor);
  colors_[ThemeProperties::COLOR_TAB_THROBBER_WAITING] =
      native_theme_->GetSystemColor(
          ui::NativeTheme::kColorId_ThrobberWaitingColor);
}

void GtkUi::UpdateCursorTheme() {
  GtkSettings* settings = gtk_settings_get_default();

  gchar* theme = nullptr;
  gint size = 0;
  g_object_get(settings, "gtk-cursor-theme-name", &theme,
               "gtk-cursor-theme-size", &size, nullptr);

  if (theme)
    XcursorSetTheme(gfx::GetXDisplay(), theme);
  if (size)
    XcursorSetDefaultSize(gfx::GetXDisplay(), size);

  g_free(theme);
}

void GtkUi::UpdateDefaultFont() {
  gfx::SetFontRenderParamsDeviceScaleFactor(device_scale_factor_);

  GtkWidget* fake_label = gtk_label_new(nullptr);
  g_object_ref_sink(fake_label);  // Remove the floating reference.
  PangoContext* pc = gtk_widget_get_pango_context(fake_label);
  const PangoFontDescription* desc = pango_context_get_font_description(pc);

  // Use gfx::FontRenderParams to select a family and determine the rendering
  // settings.
  gfx::FontRenderParamsQuery query;
  query.families =
      base::SplitString(pango_font_description_get_family(desc), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

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
    default_font_size_pixels_ =
        static_cast<int>(kDefaultDPI / 72.0 * size_points + 0.5);
    query.point_size = static_cast<int>(size_points);
  }

  query.style = gfx::Font::NORMAL;
  query.weight =
      static_cast<gfx::Font::Weight>(pango_font_description_get_weight(desc));
  // TODO(davemoore): What about PANGO_STYLE_OBLIQUE?
  if (pango_font_description_get_style(desc) == PANGO_STYLE_ITALIC)
    query.style |= gfx::Font::ITALIC;

  default_font_render_params_ =
      gfx::GetFontRenderParams(query, &default_font_family_);
  default_font_style_ = query.style;

  gtk_widget_destroy(fake_label);
  g_object_unref(fake_label);
}

bool GtkUi::GetChromeStyleColor(const char* style_property,
                                SkColor* ret_color) const {
#if GTK_MAJOR_VERSION == 2
  GdkColor* style_color = nullptr;
  gtk_widget_style_get(fake_window_, style_property, &style_color, nullptr);
  if (style_color) {
    *ret_color = GdkColorToSkColor(*style_color);
    gdk_color_free(style_color);
    return true;
  }
#endif

  return false;
}

void GtkUi::ResetStyle() {
  LoadGtkValues();
  native_theme_->NotifyObservers();
}

void GtkUi::UpdateDeviceScaleFactor() {
  // Note: Linux chrome currently does not support dynamic DPI
  // changes.  This is to allow flags to override the DPI settings
  // during startup.
  float scale = GetRawDeviceScaleFactor();
  // Blacklist scaling factors <120% (crbug.com/484400) and round
  // to 1 decimal to prevent rendering problems (crbug.com/485183).
  device_scale_factor_ = scale < 1.2f ? 1.0f : roundf(scale * 10) / 10;
  UpdateDefaultFont();
}

float GtkUi::GetDeviceScaleFactor() const {
  return device_scale_factor_;
}

}  // namespace libgtkui

views::LinuxUI* BuildGtkUi() {
  return new libgtkui::GtkUi;
}
