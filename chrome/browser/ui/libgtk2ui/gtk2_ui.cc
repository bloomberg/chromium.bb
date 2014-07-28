// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/gtk2_ui.h"

#include <set>

#include <pango/pango.h>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/environment.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/nix/mime_util_xdg.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/libgtk2ui/app_indicator_icon.h"
#include "chrome/browser/ui/libgtk2ui/chrome_gtk_frame.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_border.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_event_loop.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_key_bindings_handler.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_signal_registrar.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_status_icon.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_util.h"
#include "chrome/browser/ui/libgtk2ui/native_theme_gtk2.h"
#include "chrome/browser/ui/libgtk2ui/print_dialog_gtk2.h"
#include "chrome/browser/ui/libgtk2ui/printing_gtk2_util.h"
#include "chrome/browser/ui/libgtk2ui/select_file_dialog_impl.h"
#include "chrome/browser/ui/libgtk2ui/skia_utils_gtk2.h"
#include "chrome/browser/ui/libgtk2ui/unity_service.h"
#include "chrome/browser/ui/libgtk2ui/x11_input_method_context_impl_gtk2.h"
#include "grit/component_scaled_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "printing/printing_context_linux.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/pango_util.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/linux_ui/window_button_order_observer.h"

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

namespace libgtk2ui {

namespace {

struct GObjectDeleter {
  void operator()(void* ptr) {
    g_object_unref(ptr);
  }
};
struct GtkIconInfoDeleter {
  void operator()(GtkIconInfo* ptr) {
    gtk_icon_info_free(ptr);
  }
};
typedef scoped_ptr<GIcon, GObjectDeleter> ScopedGIcon;
typedef scoped_ptr<GtkIconInfo, GtkIconInfoDeleter> ScopedGtkIconInfo;
typedef scoped_ptr<GdkPixbuf, GObjectDeleter> ScopedGdkPixbuf;

// Prefix for app indicator ids
const char kAppIndicatorIdPrefix[] = "chrome_app_indicator_";

// Number of app indicators used (used as part of app-indicator id).
int indicators_count;

// The unknown content type.
const char* kUnknownContentType = "application/octet-stream";

// The size of the rendered toolbar image.
const int kToolbarImageWidth = 64;
const int kToolbarImageHeight = 128;

// How much to tint the GTK+ color lighter at the top of the window.
const color_utils::HSL kGtkFrameShift = { -1, -1, 0.58 };

// How much to tint the GTK+ color when an explicit frame color hasn't been
// specified.
const color_utils::HSL kDefaultFrameShift = { -1, -1, 0.4 };

// Values used as the new luminance and saturation values in the inactive tab
// text color.
const double kDarkInactiveLuminance = 0.85;
const double kLightInactiveLuminance = 0.15;
const double kHeavyInactiveSaturation = 0.7;
const double kLightInactiveSaturation = 0.3;

// Default color for links on the NTP when the GTK+ theme doesn't define a
// link color. Constant taken from gtklinkbutton.c.
const GdkColor kDefaultLinkColor = { 0, 0, 0, 0xeeee };

const int kSkiaToGDKMultiplier = 257;

// TODO(erg): ThemeService has a whole interface just for reading default
// constants. Figure out what to do with that more long term; for now, just
// copy the constants themselves here.
//
// Default tints.
const color_utils::HSL kDefaultTintButtons = { -1, -1, -1 };
const color_utils::HSL kDefaultTintFrame = { -1, -1, -1 };
const color_utils::HSL kDefaultTintFrameInactive = { -1, -1, 0.75f };
const color_utils::HSL kDefaultTintFrameIncognito = { -1, 0.2f, 0.35f };
const color_utils::HSL kDefaultTintFrameIncognitoInactive = { -1, 0.3f, 0.6f };
const color_utils::HSL kDefaultTintBackgroundTab = { -1, 0.5, 0.75 };

// A list of images that we provide while in gtk mode.
//
// TODO(erg): We list both the normal and *_DESKTOP versions of some of these
// images because in some contexts, we don't go through the
// chrome::MapThemeImage interface. That should be fixed, but tracking that
// down is Hard.
const int kThemeImages[] = {
  IDR_THEME_TOOLBAR,
  IDR_THEME_TAB_BACKGROUND,
  IDR_THEME_TAB_BACKGROUND_DESKTOP,
  IDR_THEME_TAB_BACKGROUND_INCOGNITO,
  IDR_THEME_TAB_BACKGROUND_INCOGNITO_DESKTOP,
  IDR_FRAME,
  IDR_FRAME_INACTIVE,
  IDR_THEME_FRAME,
  IDR_THEME_FRAME_INACTIVE,
  IDR_THEME_FRAME_INCOGNITO,
  IDR_THEME_FRAME_INCOGNITO_INACTIVE,
};

// A list of icons used in the autocomplete view that should be tinted to the
// current gtk theme selection color so they stand out against the GtkEntry's
// base color.
// TODO(erg): Decide what to do about other icons that appear in the omnibox,
// e.g. content settings icons.
const int kAutocompleteImages[] = {
  IDR_OMNIBOX_EXTENSION_APP,
  IDR_OMNIBOX_HTTP,
  IDR_OMNIBOX_HTTP_DARK,
  IDR_OMNIBOX_SEARCH,
  IDR_OMNIBOX_SEARCH_DARK,
  IDR_OMNIBOX_STAR,
  IDR_OMNIBOX_STAR_DARK,
  IDR_OMNIBOX_TTS,
  IDR_OMNIBOX_TTS_DARK,
};

// This table converts button ids into a pair of gtk-stock id and state.
struct IDRGtkMapping {
  int idr;
  const char* stock_id;
  GtkStateType gtk_state;
} const kGtkIcons[] = {
  { IDR_BACK,      GTK_STOCK_GO_BACK,    GTK_STATE_NORMAL },
  { IDR_BACK_D,    GTK_STOCK_GO_BACK,    GTK_STATE_INSENSITIVE },

  { IDR_FORWARD,   GTK_STOCK_GO_FORWARD, GTK_STATE_NORMAL },
  { IDR_FORWARD_D, GTK_STOCK_GO_FORWARD, GTK_STATE_INSENSITIVE },

  { IDR_HOME,      GTK_STOCK_HOME,       GTK_STATE_NORMAL },

  { IDR_RELOAD,    GTK_STOCK_REFRESH,    GTK_STATE_NORMAL },
  { IDR_RELOAD_D,  GTK_STOCK_REFRESH,    GTK_STATE_INSENSITIVE },

  { IDR_STOP,      GTK_STOCK_STOP,       GTK_STATE_NORMAL },
  { IDR_STOP_D,    GTK_STOCK_STOP,       GTK_STATE_INSENSITIVE },
};

// The image resources that will be tinted by the 'button' tint value.
const int kOtherToolbarButtonIDs[] = {
  IDR_TOOLBAR_BEZEL_HOVER,
  IDR_TOOLBAR_BEZEL_PRESSED,
  IDR_BROWSER_ACTIONS_OVERFLOW,
  IDR_THROBBER,
  IDR_THROBBER_WAITING,
  IDR_THROBBER_LIGHT,

  // TODO(erg): The dropdown arrow should be tinted because we're injecting
  // various background GTK colors, but the code that accesses them needs to be
  // modified so that they ask their ui::ThemeProvider instead of the
  // ResourceBundle. (i.e. in a light on dark theme, the dropdown arrow will be
  // dark on dark)
  IDR_MENU_DROPARROW
};

bool IsOverridableImage(int id) {
  CR_DEFINE_STATIC_LOCAL(std::set<int>, images, ());
  if (images.empty()) {
    images.insert(kThemeImages, kThemeImages + arraysize(kThemeImages));
    images.insert(kAutocompleteImages,
                  kAutocompleteImages + arraysize(kAutocompleteImages));

    for (unsigned int i = 0; i < arraysize(kGtkIcons); ++i)
      images.insert(kGtkIcons[i].idr);

    images.insert(kOtherToolbarButtonIDs,
                  kOtherToolbarButtonIDs + arraysize(kOtherToolbarButtonIDs));
  }

  return images.count(id) > 0;
}

// Picks a button tint from a set of background colors. While
// |accent_gdk_color| will usually be the same color through a theme, this
// function will get called with the normal GtkLabel |text_color|/GtkWindow
// |background_color| pair and the GtkEntry |text_color|/|background_color|
// pair. While 3/4 of the time the resulting tint will be the same, themes that
// have a dark window background (with light text) and a light text entry (with
// dark text) will get better icons with this separated out.
void PickButtonTintFromColors(const GdkColor& accent_gdk_color,
                              const GdkColor& text_color,
                              const GdkColor& background_color,
                              color_utils::HSL* tint) {
  SkColor accent_color = GdkColorToSkColor(accent_gdk_color);
  color_utils::HSL accent_tint;
  color_utils::SkColorToHSL(accent_color, &accent_tint);

  color_utils::HSL text_tint;
  color_utils::SkColorToHSL(GdkColorToSkColor(text_color),
                            &text_tint);

  color_utils::HSL background_tint;
  color_utils::SkColorToHSL(GdkColorToSkColor(background_color),
                            &background_tint);

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

// Applies an HSL shift to a GdkColor (instead of an SkColor)
void GdkColorHSLShift(const color_utils::HSL& shift, GdkColor* frame_color) {
  SkColor shifted = color_utils::HSLShift(
      GdkColorToSkColor(*frame_color), shift);

  frame_color->pixel = 0;
  frame_color->red = SkColorGetR(shifted) * kSkiaToGDKMultiplier;
  frame_color->green = SkColorGetG(shifted) * kSkiaToGDKMultiplier;
  frame_color->blue = SkColorGetB(shifted) * kSkiaToGDKMultiplier;
}

// Copied Default blah sections from ThemeService.
color_utils::HSL GetDefaultTint(int id) {
  switch (id) {
    case ThemeProperties::TINT_FRAME:
      return kDefaultTintFrame;
    case ThemeProperties::TINT_FRAME_INACTIVE:
      return kDefaultTintFrameInactive;
    case ThemeProperties::TINT_FRAME_INCOGNITO:
      return kDefaultTintFrameIncognito;
    case ThemeProperties::TINT_FRAME_INCOGNITO_INACTIVE:
      return kDefaultTintFrameIncognitoInactive;
    case ThemeProperties::TINT_BUTTONS:
      return kDefaultTintButtons;
    case ThemeProperties::TINT_BACKGROUND_TAB:
      return kDefaultTintBackgroundTab;
    default:
      color_utils::HSL result = {-1, -1, -1};
      return result;
  }
}

// Returns a FontRenderParams corresponding to GTK's configuration.
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

}  // namespace

Gtk2UI::Gtk2UI() : middle_click_action_(MIDDLE_CLICK_ACTION_LOWER) {
  GtkInitFromCommandLine(*CommandLine::ForCurrentProcess());
}

void Gtk2UI::Initialize() {
  signals_.reset(new Gtk2SignalRegistrar);

  // Create our fake widgets.
  fake_window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  fake_frame_ = chrome_gtk_frame_new();
  fake_label_.Own(gtk_label_new(""));
  fake_entry_.Own(gtk_entry_new());

  // Only realized widgets receive style-set notifications, which we need to
  // broadcast new theme images and colors. Only realized widgets have style
  // properties, too, which we query for some colors.
  gtk_widget_realize(fake_frame_);
  gtk_widget_realize(fake_window_);

  signals_->Connect(fake_frame_, "style-set",
                    G_CALLBACK(&OnStyleSetThunk), this);

  LoadGtkValues();

  printing::PrintingContextLinux::SetCreatePrintDialogFunction(
      &PrintDialogGtk2::CreatePrintDialog);
  printing::PrintingContextLinux::SetPdfPaperSizeFunction(
      &GetPdfPaperSizeDeviceUnitsGtk);

#if defined(USE_GCONF)
  // We must build this after GTK gets initialized.
  gconf_listener_.reset(new GConfListener(this));
#endif  // defined(USE_GCONF)

  indicators_count = 0;

  // Instantiate the singleton instance of Gtk2EventLoop.
  Gtk2EventLoop::GetInstance();
}

Gtk2UI::~Gtk2UI() {
  gtk_widget_destroy(fake_window_);
  gtk_widget_destroy(fake_frame_);
  fake_label_.Destroy();
  fake_entry_.Destroy();

  ClearAllThemeData();
}

gfx::Image Gtk2UI::GetThemeImageNamed(int id) const {
  // Try to get our cached version:
  ImageCache::const_iterator it = gtk_images_.find(id);
  if (it != gtk_images_.end())
    return it->second;

  if (IsOverridableImage(id)) {
    gfx::Image image = gfx::Image(
        gfx::ImageSkia::CreateFrom1xBitmap(GenerateGtkThemeBitmap(id)));
    gtk_images_[id] = image;
    return image;
  }

  return gfx::Image();
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
  return IsOverridableImage(id);
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
  scoped_ptr<base::Environment> env(base::Environment::Create());

  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
    case base::nix::DESKTOP_ENVIRONMENT_UNITY:
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
      return true;
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
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

scoped_ptr<views::StatusIconLinux> Gtk2UI::CreateLinuxStatusIcon(
    const gfx::ImageSkia& image,
    const base::string16& tool_tip) const {
  if (AppIndicatorIcon::CouldOpen()) {
    ++indicators_count;
    return scoped_ptr<views::StatusIconLinux>(new AppIndicatorIcon(
        base::StringPrintf("%s%d", kAppIndicatorIdPrefix, indicators_count),
        image,
        tool_tip));
  } else {
    return scoped_ptr<views::StatusIconLinux>(new Gtk2StatusIcon(
        image, tool_tip));
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

scoped_ptr<views::Border> Gtk2UI::CreateNativeBorder(
    views::LabelButton* owning_button,
    scoped_ptr<views::LabelButtonBorder> border) {
  if (owning_button->GetNativeTheme() != NativeThemeGtk2::instance())
    return border.PassAs<views::Border>();

  return scoped_ptr<views::Border>(
      new Gtk2Border(this, owning_button, border.Pass()));
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

scoped_ptr<ui::LinuxInputMethodContext> Gtk2UI::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate) const {
  return scoped_ptr<ui::LinuxInputMethodContext>(
      new X11InputMethodContextImplGtk2(delegate));
}

gfx::FontRenderParams Gtk2UI::GetDefaultFontRenderParams() const {
  static gfx::FontRenderParams params = GetGtkFontRenderParams();
  return params;
}

scoped_ptr<gfx::ScopedPangoFontDescription>
Gtk2UI::GetDefaultPangoFontDescription() const {
  return scoped_ptr<gfx::ScopedPangoFontDescription>(
      new gfx::ScopedPangoFontDescription(
          pango_font_description_copy(default_font_description_->get())));
}

double Gtk2UI::GetFontDPI() const {
  GtkSettings* gtk_settings = gtk_settings_get_default();
  CHECK(gtk_settings);
  gint dpi = -1;
  g_object_get(gtk_settings, "gtk-xft-dpi", &dpi, NULL);

  // GTK multiplies the DPI by 1024 before storing it.
  return (dpi > 0) ? dpi / 1024.0 : dpi;
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

void Gtk2UI::GetScrollbarColors(GdkColor* thumb_active_color,
                                GdkColor* thumb_inactive_color,
                                GdkColor* track_color) {
  GdkColor* theme_thumb_active = NULL;
  GdkColor* theme_thumb_inactive = NULL;
  GdkColor* theme_trough_color = NULL;
  gtk_widget_style_get(GTK_WIDGET(fake_frame_),
                       "scrollbar-slider-prelight-color", &theme_thumb_active,
                       "scrollbar-slider-normal-color", &theme_thumb_inactive,
                       "scrollbar-trough-color", &theme_trough_color,
                       NULL);

  // Ask the theme if the theme specifies all the scrollbar colors and short
  // circuit the expensive painting/compositing if we have all of them.
  if (theme_thumb_active && theme_thumb_inactive && theme_trough_color) {
    *thumb_active_color = *theme_thumb_active;
    *thumb_inactive_color = *theme_thumb_inactive;
    *track_color = *theme_trough_color;

    gdk_color_free(theme_thumb_active);
    gdk_color_free(theme_thumb_inactive);
    gdk_color_free(theme_trough_color);
    return;
  }

  // Create window containing scrollbar elements
  GtkWidget* window    = gtk_window_new(GTK_WINDOW_POPUP);
  GtkWidget* fixed     = gtk_fixed_new();
  GtkWidget* scrollbar = gtk_hscrollbar_new(NULL);
  gtk_container_add(GTK_CONTAINER(window), fixed);
  gtk_container_add(GTK_CONTAINER(fixed),  scrollbar);
  gtk_widget_realize(window);
  gtk_widget_realize(scrollbar);

  // Draw scrollbar thumb part and track into offscreen image
  const int kWidth  = 100;
  const int kHeight = 20;
  GtkStyle* style   = gtk_rc_get_style(scrollbar);
  GdkWindow* gdk_window = gtk_widget_get_window(window);
  GdkPixmap* pm     = gdk_pixmap_new(gdk_window, kWidth, kHeight, -1);
  GdkRectangle rect = { 0, 0, kWidth, kHeight };
  unsigned char data[3 * kWidth * kHeight];
  for (int i = 0; i < 3; ++i) {
    if (i < 2) {
      // Thumb part
      gtk_paint_slider(style, pm,
                       i == 0 ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL,
                       GTK_SHADOW_OUT, &rect, scrollbar, "slider", 0, 0,
                       kWidth, kHeight, GTK_ORIENTATION_HORIZONTAL);
    } else {
      // Track
      gtk_paint_box(style, pm, GTK_STATE_ACTIVE, GTK_SHADOW_IN, &rect,
                    scrollbar, "trough-upper", 0, 0, kWidth, kHeight);
    }
    GdkPixbuf* pb = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB,
                                             FALSE, 8, kWidth, kHeight,
                                             3 * kWidth, 0, 0);
    gdk_pixbuf_get_from_drawable(pb, pm, NULL, 0, 0, 0, 0, kWidth, kHeight);

    // Sample pixels
    int components[3] = { 0 };
    for (int y = 2; y < kHeight - 2; ++y) {
      for (int c = 0; c < 3; ++c) {
        // Sample a vertical slice of pixels at about one-thirds from the
        // left edge. This allows us to avoid any fixed graphics that might be
        // located at the edges or in the center of the scrollbar.
        // Each pixel is made up of a red, green, and blue component; taking up
        // a total of three bytes.
        components[c] += data[3 * (kWidth / 3 + y * kWidth) + c];
      }
    }
    GdkColor* color = i == 0 ? thumb_active_color :
                      i == 1 ? thumb_inactive_color :
                               track_color;
    color->pixel = 0;
    // We sampled pixels across the full height of the image, ignoring a two
    // pixel border. In some themes, the border has a completely different
    // color which we do not want to factor into our average color computation.
    //
    // We now need to scale the colors from the 0..255 range, to the wider
    // 0..65535 range, and we need to actually compute the average color; so,
    // we divide by the total number of pixels in the sample.
    color->red   = components[0] * 65535 / (255 * (kHeight - 4));
    color->green = components[1] * 65535 / (255 * (kHeight - 4));
    color->blue  = components[2] * 65535 / (255 * (kHeight - 4));

    g_object_unref(pb);
  }
  g_object_unref(pm);

  gtk_widget_destroy(window);

  // Override any of the default colors with ones that were specified by the
  // theme.
  if (theme_thumb_active) {
    *thumb_active_color = *theme_thumb_active;
    gdk_color_free(theme_thumb_active);
  }

  if (theme_thumb_inactive) {
    *thumb_inactive_color = *theme_thumb_inactive;
    gdk_color_free(theme_thumb_inactive);
  }

  if (theme_trough_color) {
    *track_color = *theme_trough_color;
    gdk_color_free(theme_trough_color);
  }
}

void Gtk2UI::LoadGtkValues() {
  // TODO(erg): GtkThemeService had a comment here about having to muck with
  // the raw Prefs object to remove prefs::kCurrentThemeImages or else we'd
  // regress startup time. Figure out how to do that when we can't access the
  // prefs system from here.

  GtkStyle* frame_style = gtk_rc_get_style(fake_frame_);

  GtkStyle* window_style = gtk_rc_get_style(fake_window_);
  SetThemeColorFromGtk(ThemeProperties::COLOR_CONTROL_BACKGROUND,
                       &window_style->bg[GTK_STATE_NORMAL]);

  GdkColor toolbar_color = window_style->bg[GTK_STATE_NORMAL];
  SetThemeColorFromGtk(ThemeProperties::COLOR_TOOLBAR, &toolbar_color);

  GdkColor button_color = window_style->bg[GTK_STATE_SELECTED];
  SetThemeTintFromGtk(ThemeProperties::TINT_BUTTONS, &button_color);

  GtkStyle* label_style = gtk_rc_get_style(fake_label_.get());
  GdkColor label_color = label_style->fg[GTK_STATE_NORMAL];
  SetThemeColorFromGtk(ThemeProperties::COLOR_TAB_TEXT, &label_color);
  SetThemeColorFromGtk(ThemeProperties::COLOR_BOOKMARK_TEXT, &label_color);
  SetThemeColorFromGtk(ThemeProperties::COLOR_STATUS_BAR_TEXT, &label_color);

  default_font_description_.reset(new gfx::ScopedPangoFontDescription(
      pango_font_description_copy(label_style->font_desc)));

  // Build the various icon tints.
  GetNormalButtonTintHSL(&button_tint_);
  GetNormalEntryForegroundHSL(&entry_tint_);
  GetSelectedEntryForegroundHSL(&selected_entry_tint_);
  GdkColor frame_color = BuildFrameColors(frame_style);

  // The inactive frame color never occurs naturally in the theme, as it is a
  // tinted version of |frame_color|. We generate another color based on the
  // background tab color, with the lightness and saturation moved in the
  // opposite direction. (We don't touch the hue, since there should be subtle
  // hints of the color in the text.)
  color_utils::HSL inactive_tab_text_hsl =
      tints_[ThemeProperties::TINT_BACKGROUND_TAB];
  if (inactive_tab_text_hsl.l < 0.5)
    inactive_tab_text_hsl.l = kDarkInactiveLuminance;
  else
    inactive_tab_text_hsl.l = kLightInactiveLuminance;

  if (inactive_tab_text_hsl.s < 0.5)
    inactive_tab_text_hsl.s = kHeavyInactiveSaturation;
  else
    inactive_tab_text_hsl.s = kLightInactiveSaturation;

  colors_[ThemeProperties::COLOR_BACKGROUND_TAB_TEXT] =
      color_utils::HSLToSkColor(inactive_tab_text_hsl, 255);

  // We pick the text and background colors for the NTP out of the colors for a
  // GtkEntry. We do this because GtkEntries background color is never the same
  // as |toolbar_color|, is usually a white, and when it isn't a white,
  // provides sufficient contrast to |toolbar_color|. Try this out with
  // Darklooks, HighContrastInverse or ThinIce.
  GtkStyle* entry_style = gtk_rc_get_style(fake_entry_.get());
  GdkColor ntp_background = entry_style->base[GTK_STATE_NORMAL];
  GdkColor ntp_foreground = entry_style->text[GTK_STATE_NORMAL];
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_BACKGROUND,
                       &ntp_background);
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_TEXT,
                       &ntp_foreground);

  // The NTP header is the color that surrounds the current active thumbnail on
  // the NTP, and acts as the border of the "Recent Links" box. It would be
  // awesome if they were separated so we could use GetBorderColor() for the
  // border around the "Recent Links" section, but matching the frame color is
  // more important.
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_HEADER,
                       &frame_color);
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_SECTION,
                       &toolbar_color);
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_SECTION_TEXT,
                       &label_color);

  // Override the link color if the theme provides it.
  const GdkColor* link_color = NULL;
  gtk_widget_style_get(GTK_WIDGET(fake_window_),
                       "link-color", &link_color, NULL);

  bool is_default_link_color = false;
  if (!link_color) {
    link_color = &kDefaultLinkColor;
    is_default_link_color = true;
  }

  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_LINK,
                       link_color);
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_LINK_UNDERLINE,
                       link_color);
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_SECTION_LINK,
                       link_color);
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_SECTION_LINK_UNDERLINE,
                       link_color);

  if (!is_default_link_color)
    gdk_color_free(const_cast<GdkColor*>(link_color));

  // Generate the colors that we pass to WebKit.
  focus_ring_color_ = GdkColorToSkColor(frame_color);

  GdkColor thumb_active_color, thumb_inactive_color, track_color;
  Gtk2UI::GetScrollbarColors(&thumb_active_color,
                             &thumb_inactive_color,
                             &track_color);
  thumb_active_color_ = GdkColorToSkColor(thumb_active_color);
  thumb_inactive_color_ = GdkColorToSkColor(thumb_inactive_color);
  track_color_ = GdkColorToSkColor(track_color);

  // Some GTK themes only define the text selection colors on the GtkEntry
  // class, so we need to use that for getting selection colors.
  active_selection_bg_color_ =
      GdkColorToSkColor(entry_style->base[GTK_STATE_SELECTED]);
  active_selection_fg_color_ =
      GdkColorToSkColor(entry_style->text[GTK_STATE_SELECTED]);
  inactive_selection_bg_color_ =
      GdkColorToSkColor(entry_style->base[GTK_STATE_ACTIVE]);
  inactive_selection_fg_color_ =
      GdkColorToSkColor(entry_style->text[GTK_STATE_ACTIVE]);
}

GdkColor Gtk2UI::BuildFrameColors(GtkStyle* frame_style) {
  GdkColor* theme_frame = NULL;
  GdkColor* theme_inactive_frame = NULL;
  GdkColor* theme_incognito_frame = NULL;
  GdkColor* theme_incognito_inactive_frame = NULL;
  gtk_widget_style_get(GTK_WIDGET(fake_frame_),
                       "frame-color", &theme_frame,
                       "inactive-frame-color", &theme_inactive_frame,
                       "incognito-frame-color", &theme_incognito_frame,
                       "incognito-inactive-frame-color",
                       &theme_incognito_inactive_frame,
                       NULL);

  GdkColor frame_color = BuildAndSetFrameColor(
      &frame_style->bg[GTK_STATE_SELECTED],
      theme_frame,
      kDefaultFrameShift,
      ThemeProperties::COLOR_FRAME,
      ThemeProperties::TINT_FRAME);
  if (theme_frame)
    gdk_color_free(theme_frame);
  SetThemeTintFromGtk(ThemeProperties::TINT_BACKGROUND_TAB, &frame_color);

  BuildAndSetFrameColor(
      &frame_style->bg[GTK_STATE_INSENSITIVE],
      theme_inactive_frame,
      kDefaultFrameShift,
      ThemeProperties::COLOR_FRAME_INACTIVE,
      ThemeProperties::TINT_FRAME_INACTIVE);
  if (theme_inactive_frame)
    gdk_color_free(theme_inactive_frame);

  BuildAndSetFrameColor(
      &frame_color,
      theme_incognito_frame,
      GetDefaultTint(ThemeProperties::TINT_FRAME_INCOGNITO),
      ThemeProperties::COLOR_FRAME_INCOGNITO,
      ThemeProperties::TINT_FRAME_INCOGNITO);
  if (theme_incognito_frame)
    gdk_color_free(theme_incognito_frame);

  BuildAndSetFrameColor(
      &frame_color,
      theme_incognito_inactive_frame,
      GetDefaultTint(ThemeProperties::TINT_FRAME_INCOGNITO_INACTIVE),
      ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE,
      ThemeProperties::TINT_FRAME_INCOGNITO_INACTIVE);
  if (theme_incognito_inactive_frame)
    gdk_color_free(theme_incognito_inactive_frame);

  return frame_color;
}

void Gtk2UI::SetThemeColorFromGtk(int id, const GdkColor* color) {
  colors_[id] = GdkColorToSkColor(*color);
}

void Gtk2UI::SetThemeTintFromGtk(int id, const GdkColor* color) {
  color_utils::HSL default_tint = GetDefaultTint(id);
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(GdkColorToSkColor(*color), &hsl);

  if (default_tint.s != -1)
    hsl.s = default_tint.s;

  if (default_tint.l != -1)
    hsl.l = default_tint.l;

  tints_[id] = hsl;
}

GdkColor Gtk2UI::BuildAndSetFrameColor(const GdkColor* base,
                                       const GdkColor* gtk_base,
                                       const color_utils::HSL& tint,
                                       int color_id,
                                       int tint_id) {
  GdkColor out_color = *base;
  if (gtk_base) {
    // The theme author specified a color to use, use it without modification.
    out_color = *gtk_base;
  } else {
    // Tint the basic color since this is a heuristic color instead of one
    // specified by the theme author.
    GdkColorHSLShift(tint, &out_color);
  }
  SetThemeColorFromGtk(color_id, &out_color);
  SetThemeTintFromGtk(tint_id, &out_color);

  return out_color;
}

SkBitmap Gtk2UI::GenerateGtkThemeBitmap(int id) const {
  switch (id) {
    case IDR_THEME_TOOLBAR: {
      GtkStyle* style = gtk_rc_get_style(fake_window_);
      GdkColor* color = &style->bg[GTK_STATE_NORMAL];
      SkBitmap bitmap;
      bitmap.allocN32Pixels(kToolbarImageWidth, kToolbarImageHeight);
      bitmap.eraseARGB(0xff, color->red >> 8, color->green >> 8,
                       color->blue >> 8);
      return bitmap;
    }
    case IDR_THEME_TAB_BACKGROUND:
    case IDR_THEME_TAB_BACKGROUND_DESKTOP:
      return GenerateTabImage(IDR_THEME_FRAME);
    case IDR_THEME_TAB_BACKGROUND_INCOGNITO:
    case IDR_THEME_TAB_BACKGROUND_INCOGNITO_DESKTOP:
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
    case IDR_OMNIBOX_EXTENSION_APP:
    case IDR_OMNIBOX_HTTP:
    case IDR_OMNIBOX_SEARCH:
    case IDR_OMNIBOX_STAR:
    case IDR_OMNIBOX_TTS: {
      return GenerateTintedIcon(id, entry_tint_);
    }
    // In GTK mode, the dark versions of the omnibox icons only ever appear in
    // the autocomplete popup and only against the current theme's GtkEntry
    // base[GTK_STATE_SELECTED] color, so tint the icons so they won't collide
    // with the selected color.
    case IDR_OMNIBOX_EXTENSION_APP_DARK:
    case IDR_OMNIBOX_HTTP_DARK:
    case IDR_OMNIBOX_SEARCH_DARK:
    case IDR_OMNIBOX_STAR_DARK:
    case IDR_OMNIBOX_TTS_DARK: {
      return GenerateTintedIcon(id, selected_entry_tint_);
    }
    // In GTK mode, we need to manually render several icons.
    case IDR_BACK:
    case IDR_BACK_D:
    case IDR_FORWARD:
    case IDR_FORWARD_D:
    case IDR_HOME:
    case IDR_RELOAD:
    case IDR_RELOAD_D:
    case IDR_STOP:
    case IDR_STOP_D: {
      return GenerateGTKIcon(id);
    }
    case IDR_TOOLBAR_BEZEL_HOVER:
      return GenerateToolbarBezel(GTK_STATE_PRELIGHT, IDR_TOOLBAR_BEZEL_HOVER);
    case IDR_TOOLBAR_BEZEL_PRESSED:
      return GenerateToolbarBezel(GTK_STATE_ACTIVE, IDR_TOOLBAR_BEZEL_PRESSED);
    default: {
      return GenerateTintedIcon(id, button_tint_);
    }
  }

  return SkBitmap();
}

SkBitmap Gtk2UI::GenerateFrameImage(
    int color_id,
    const char* gradient_name) const {
  // We use two colors: the main color (passed in) and a lightened version of
  // that color (which is supposed to match the light gradient at the top of
  // several GTK+ themes, such as Ambiance, Clearlooks or Bluebird).
  ColorMap::const_iterator it = colors_.find(color_id);
  DCHECK(it != colors_.end());
  SkColor base = it->second;

  gfx::Canvas canvas(gfx::Size(kToolbarImageWidth, kToolbarImageHeight),
      1.0f, true);

  int gradient_size;
  GdkColor* gradient_top_color = NULL;
  gtk_widget_style_get(GTK_WIDGET(fake_frame_),
                       "frame-gradient-size", &gradient_size,
                       gradient_name, &gradient_top_color,
                       NULL);
  if (gradient_size) {
    SkColor lighter = gradient_top_color ?
        GdkColorToSkColor(*gradient_top_color) :
        color_utils::HSLShift(base, kGtkFrameShift);
    if (gradient_top_color)
      gdk_color_free(gradient_top_color);
    skia::RefPtr<SkShader> shader = gfx::CreateGradientShader(
        0, gradient_size, lighter, base);
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    paint.setShader(shader.get());

    canvas.DrawRect(gfx::Rect(0, 0, kToolbarImageWidth, gradient_size), paint);
  }

  canvas.FillRect(gfx::Rect(0, gradient_size, kToolbarImageWidth,
                            kToolbarImageHeight - gradient_size), base);
  return canvas.ExtractImageRep().sk_bitmap();
}

SkBitmap Gtk2UI::GenerateTabImage(int base_id) const {
  const SkBitmap* base_image = GetThemeImageNamed(base_id).ToSkBitmap();
  SkBitmap bg_tint = SkBitmapOperations::CreateHSLShiftedBitmap(
      *base_image, GetDefaultTint(ThemeProperties::TINT_BACKGROUND_TAB));
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

SkBitmap Gtk2UI::GenerateGTKIcon(int base_id) const {
  const char* stock_id = NULL;
  GtkStateType gtk_state = GTK_STATE_NORMAL;
  for (unsigned int i = 0; i < arraysize(kGtkIcons); ++i) {
    if (kGtkIcons[i].idr == base_id) {
      stock_id = kGtkIcons[i].stock_id;
      gtk_state = kGtkIcons[i].gtk_state;
      break;
    }
  }
  DCHECK(stock_id);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SkBitmap default_bitmap = rb.GetImageNamed(base_id).AsBitmap();

  gtk_widget_ensure_style(fake_frame_);
  GtkStyle* style = gtk_widget_get_style(fake_frame_);
  GtkIconSet* icon_set = gtk_style_lookup_icon_set(style, stock_id);
  if (!icon_set)
    return default_bitmap;

  // Ask GTK to render the icon to a buffer, which we will steal from.
  GdkPixbuf* gdk_icon = gtk_icon_set_render_icon(
      icon_set,
      style,
      base::i18n::IsRTL() ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR,
      gtk_state,
      GTK_ICON_SIZE_SMALL_TOOLBAR,
      fake_frame_,
      NULL);

  if (!gdk_icon) {
    // This can theoretically happen if an icon theme doesn't provide a
    // specific image. This should realistically never happen, but I bet there
    // are some theme authors who don't reliably provide all icons.
    return default_bitmap;
  }

  SkBitmap retval;
  retval.allocN32Pixels(default_bitmap.width(), default_bitmap.height());
  retval.eraseColor(0);

  const SkBitmap icon = GdkPixbufToImageSkia(gdk_icon);
  g_object_unref(gdk_icon);

  SkCanvas canvas(retval);

  if (gtk_state == GTK_STATE_ACTIVE || gtk_state == GTK_STATE_PRELIGHT) {
    SkBitmap border = DrawGtkButtonBorder(gtk_state,
                                          false,
                                          false,
                                          default_bitmap.width(),
                                          default_bitmap.height());
    canvas.drawBitmap(border, 0, 0);
  }

  canvas.drawBitmap(icon,
                    (default_bitmap.width() / 2) - (icon.width() / 2),
                    (default_bitmap.height() / 2) - (icon.height() / 2));

  return retval;
}

SkBitmap Gtk2UI::GenerateToolbarBezel(int gtk_state, int sizing_idr) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SkBitmap default_bitmap =
      rb.GetImageNamed(sizing_idr).AsBitmap();

  SkBitmap retval;
  retval.allocN32Pixels(default_bitmap.width(), default_bitmap.height());
  retval.eraseColor(0);

  SkCanvas canvas(retval);
  SkBitmap border = DrawGtkButtonBorder(
      gtk_state,
      false,
      false,
      default_bitmap.width(),
      default_bitmap.height());
  canvas.drawBitmap(border, 0, 0);

  return retval;
}

void Gtk2UI::GetNormalButtonTintHSL(color_utils::HSL* tint) const {
  GtkStyle* window_style = gtk_rc_get_style(fake_window_);
  const GdkColor accent_gdk_color = window_style->bg[GTK_STATE_SELECTED];
  const GdkColor base_color = window_style->base[GTK_STATE_NORMAL];

  GtkStyle* label_style = gtk_rc_get_style(fake_label_.get());
  const GdkColor text_color = label_style->fg[GTK_STATE_NORMAL];

  PickButtonTintFromColors(accent_gdk_color, text_color, base_color, tint);
}

void Gtk2UI::GetNormalEntryForegroundHSL(color_utils::HSL* tint) const {
  GtkStyle* window_style = gtk_rc_get_style(fake_window_);
  const GdkColor accent_gdk_color = window_style->bg[GTK_STATE_SELECTED];

  GtkStyle* style = gtk_rc_get_style(fake_entry_.get());
  const GdkColor text_color = style->text[GTK_STATE_NORMAL];
  const GdkColor base_color = style->base[GTK_STATE_NORMAL];

  PickButtonTintFromColors(accent_gdk_color, text_color, base_color, tint);
}

void Gtk2UI::GetSelectedEntryForegroundHSL(color_utils::HSL* tint) const {
  // The simplest of all the tints. We just use the selected text in the entry
  // since the icons tinted this way will only be displayed against
  // base[GTK_STATE_SELECTED].
  GtkStyle* style = gtk_rc_get_style(fake_entry_.get());
  const GdkColor color = style->text[GTK_STATE_SELECTED];
  color_utils::SkColorToHSL(GdkColorToSkColor(color), tint);
}

SkBitmap Gtk2UI::DrawGtkButtonBorder(int gtk_state,
                                     bool focused,
                                     bool call_to_action,
                                     int width,
                                     int height) const {
  // Create a temporary GTK button to snapshot
  GtkWidget* window = gtk_offscreen_window_new();
  GtkWidget* button = gtk_button_new();
  gtk_widget_set_size_request(button, width, height);
  gtk_container_add(GTK_CONTAINER(window), button);
  gtk_widget_realize(window);
  gtk_widget_realize(button);
  gtk_widget_show(button);
  gtk_widget_show(window);

  if (call_to_action)
    GTK_WIDGET_SET_FLAGS(button, GTK_HAS_DEFAULT);

  if (focused) {
    // We can't just use gtk_widget_grab_focus() here because that sets
    // gtk_widget_is_focus(), but not gtk_widget_has_focus(), which is what the
    // GtkButton's paint checks.
    GTK_WIDGET_SET_FLAGS(button, GTK_HAS_FOCUS);
  }

  gtk_widget_set_state(button, static_cast<GtkStateType>(gtk_state));

  GdkPixmap* pixmap;
  {
    // http://crbug.com/346740
    ANNOTATE_SCOPED_MEMORY_LEAK;
    pixmap = gtk_widget_get_snapshot(button, NULL);
  }
  int w, h;
  gdk_drawable_get_size(GDK_DRAWABLE(pixmap), &w, &h);
  DCHECK_EQ(w, width);
  DCHECK_EQ(h, height);

  // We render the Pixmap to a Pixbuf. This can be slow, as we're scrapping
  // bits from X.
  GdkColormap* colormap = gdk_drawable_get_colormap(pixmap);
  GdkPixbuf* pixbuf = gdk_pixbuf_get_from_drawable(NULL,
                                                   GDK_DRAWABLE(pixmap),
                                                   colormap,
                                                   0, 0, 0, 0, w, h);

  // Finally, we convert our pixbuf into a type we can use.
  SkBitmap border = GdkPixbufToImageSkia(pixbuf);
  g_object_unref(pixbuf);
  g_object_unref(pixmap);
  gtk_widget_destroy(window);

  return border;
}

void Gtk2UI::ClearAllThemeData() {
  gtk_images_.clear();
}

void Gtk2UI::OnStyleSet(GtkWidget* widget, GtkStyle* previous_style) {
  ClearAllThemeData();
  LoadGtkValues();
  NativeThemeGtk2::instance()->NotifyObservers();
}

}  // namespace libgtk2ui

views::LinuxUI* BuildGtk2UI() {
  return new libgtk2ui::Gtk2UI;
}
