// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/gtk_theme_provider.h"

#include <gtk/gtk.h>

#include "app/gfx/color_utils.h"
#include "app/gfx/gtk_util.h"
#include "app/gfx/skbitmap_operations.h"
#include "app/resource_bundle.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/gtk/cairo_cached_surface.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "skia/ext/skia_utils_gtk.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"

namespace {

// The size of the rendered toolbar image.
const int kToolbarImageWidth = 64;
const int kToolbarImageHeight = 128;

const color_utils::HSL kExactColor = { -1, -1, -1 };

const color_utils::HSL kDefaultFrameShift = { -1, -1, 0.4 };

// Values used as the new luminance and saturation values in the inactive tab
// text color.
const double kDarkInactiveLuminance = 0.85;
const double kLightInactiveLuminance = 0.15;
const double kHeavyInactiveSaturation = 0.7;
const double kLightInactiveSaturation = 0.3;

// Minimum difference between the toolbar and the button color before we try a
// different color.
const double kMinimumLuminanceDifference = 0.1;

// Number of times that the background color should be counted when trying to
// calculate the border color in GTK theme mode.
const int kBgWeight = 3;

// Default color for links on the NTP when the GTK+ theme doesn't define a
// link color. Constant taken from gtklinkbutton.c.
const GdkColor kDefaultLinkColor = { 0, 0, 0, 0xeeee };

// Converts a GdkColor to a SkColor.
SkColor GdkToSkColor(const GdkColor* color) {
  return SkColorSetRGB(color->red >> 8,
                       color->green >> 8,
                       color->blue >> 8);
}

// A list of images that we provide while in gtk mode.
const int kThemeImages[] = {
  IDR_THEME_TOOLBAR,
  IDR_THEME_TAB_BACKGROUND,
  IDR_THEME_TAB_BACKGROUND_INCOGNITO,
  IDR_THEME_FRAME,
  IDR_THEME_FRAME_INACTIVE,
  IDR_THEME_FRAME_INCOGNITO,
  IDR_THEME_FRAME_INCOGNITO_INACTIVE,
};

bool IsOverridableImage(int id) {
  static std::set<int> images;
  if (images.empty()) {
    images.insert(kThemeImages, kThemeImages + arraysize(kThemeImages));

    const std::set<int>& buttons =
        BrowserThemeProvider::GetTintableToolbarButtons();
    images.insert(buttons.begin(), buttons.end());
  }

  return images.count(id) > 0;
}

}  // namespace

GtkWidget* GtkThemeProvider::icon_widget_ = NULL;
GdkPixbuf* GtkThemeProvider::default_folder_icon_ = NULL;
GdkPixbuf* GtkThemeProvider::default_bookmark_icon_ = NULL;

// static
GtkThemeProvider* GtkThemeProvider::GetFrom(Profile* profile) {
  return static_cast<GtkThemeProvider*>(profile->GetThemeProvider());
}

GtkThemeProvider::GtkThemeProvider()
    : BrowserThemeProvider(),
      fake_window_(gtk_window_new(GTK_WINDOW_TOPLEVEL)) {
  fake_label_.Own(gtk_label_new(""));
  fake_entry_.Own(gtk_entry_new());

  // Only realized widgets receive style-set notifications, which we need to
  // broadcast new theme images and colors.
  gtk_widget_realize(fake_window_);
  g_signal_connect(fake_window_, "style-set", G_CALLBACK(&OnStyleSet), this);
}

GtkThemeProvider::~GtkThemeProvider() {
  profile()->GetPrefs()->RemovePrefObserver(prefs::kUsesSystemTheme, this);
  gtk_widget_destroy(fake_window_);
  fake_label_.Destroy();
  fake_entry_.Destroy();

  // We have to call this because FreePlatformCached() in ~BrowserThemeProvider
  // doesn't call the right virutal FreePlatformCaches.
  FreePlatformCaches();

  // Disconnect from the destroy signal of any redisual widgets in
  // |chrome_buttons_|.
  for (std::vector<GtkWidget*>::iterator it = chrome_buttons_.begin();
       it != chrome_buttons_.end(); ++it) {
    gtk_signal_disconnect_by_data(GTK_OBJECT(*it), this);
  }
}

void GtkThemeProvider::Init(Profile* profile) {
  profile->GetPrefs()->AddPrefObserver(prefs::kUsesSystemTheme, this);
  use_gtk_ = profile->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme);

  BrowserThemeProvider::Init(profile);
}

SkBitmap* GtkThemeProvider::GetBitmapNamed(int id) const {
  if (use_gtk_ && IsOverridableImage(id)) {
    // Try to get our cached version:
    ImageCache::const_iterator it = gtk_images_.find(id);
    if (it != gtk_images_.end())
      return it->second;

    // We haven't built this image yet:
    SkBitmap* bitmap = GenerateGtkThemeBitmap(id);
    gtk_images_[id] = bitmap;
    return bitmap;
  }

  return BrowserThemeProvider::GetBitmapNamed(id);
}

SkColor GtkThemeProvider::GetColor(int id) const {
  if (use_gtk_) {
    ColorMap::const_iterator it = colors_.find(id);
    if (it != colors_.end())
      return it->second;
  }

  return BrowserThemeProvider::GetColor(id);
}

bool GtkThemeProvider::HasCustomImage(int id) const {
  if (use_gtk_)
    return IsOverridableImage(id);

  return BrowserThemeProvider::HasCustomImage(id);
}

void GtkThemeProvider::InitThemesFor(NotificationObserver* observer) {
  observer->Observe(NotificationType::BROWSER_THEME_CHANGED,
                    Source<ThemeProvider>(this),
                    NotificationService::NoDetails());
}

void GtkThemeProvider::SetTheme(Extension* extension) {
  profile()->GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, false);
  LoadDefaultValues();
  BrowserThemeProvider::SetTheme(extension);
}

void GtkThemeProvider::UseDefaultTheme() {
  profile()->GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, false);
  LoadDefaultValues();
  BrowserThemeProvider::UseDefaultTheme();
}

void GtkThemeProvider::SetNativeTheme() {
  profile()->GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, true);
  ClearAllThemeData();
  LoadGtkValues();
  NotifyThemeChanged();
}

void GtkThemeProvider::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  if ((type == NotificationType::PREF_CHANGED) &&
      (*Details<std::wstring>(details).ptr() == prefs::kUsesSystemTheme))
    use_gtk_ = profile()->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme);
}

GtkWidget* GtkThemeProvider::BuildChromeButton() {
  GtkWidget* button = gtk_chrome_button_new();
  gtk_chrome_button_set_use_gtk_rendering(GTK_CHROME_BUTTON(button), use_gtk_);
  chrome_buttons_.push_back(button);

  g_signal_connect(button, "destroy", G_CALLBACK(OnDestroyChromeButton),
                   this);
  return button;
}

bool GtkThemeProvider::UseGtkTheme() const {
  return use_gtk_;
}

GdkColor GtkThemeProvider::GetGdkColor(int id) const {
  return skia::SkColorToGdkColor(GetColor(id));
}

GdkColor GtkThemeProvider::GetBorderColor() const {
  GtkStyle* style = gtk_rc_get_style(fake_window_);

  GdkColor text;
  GdkColor bg;
  if (use_gtk_) {
    text = style->text[GTK_STATE_NORMAL];
    bg = style->bg[GTK_STATE_NORMAL];
  } else {
    text = GetGdkColor(COLOR_BOOKMARK_TEXT);
    bg = GetGdkColor(COLOR_TOOLBAR);
  }

  // Creates a weighted average between the text and base color where
  // the base color counts more than once.
  GdkColor color;
  color.pixel = 0;
  color.red = (text.red + (bg.red * kBgWeight)) / (1 + kBgWeight);
  color.green = (text.green + (bg.green * kBgWeight)) / (1 + kBgWeight);
  color.blue = (text.blue + (bg.blue * kBgWeight)) / (1 + kBgWeight);

  return color;
}

void GtkThemeProvider::GetScrollbarColors(GdkColor* thumb_active_color,
                                          GdkColor* thumb_inactive_color,
                                          GdkColor* track_color) {
  // Create window containing scrollbar elements
  GtkWidget* window    = gtk_window_new(GTK_WINDOW_POPUP);
  GtkWidget* fixed     = gtk_fixed_new();
  GtkWidget* scrollbar = gtk_hscrollbar_new(NULL);
  gtk_container_add(GTK_CONTAINER(window), fixed);
  gtk_container_add(GTK_CONTAINER(fixed),  scrollbar);
  gtk_widget_realize(window);
  gtk_widget_realize(scrollbar);

  // Draw scrollbar thumb part and track into offscreen image
  int kWidth        = 100;
  int kHeight       = 20;
  GtkStyle*  style  = gtk_rc_get_style(scrollbar);
  GdkPixmap* pm     = gdk_pixmap_new(window->window, kWidth, kHeight, -1);
  GdkRectangle rect = { 0, 0, kWidth, kHeight };
  unsigned char data[3*kWidth*kHeight];
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
                                             3*kWidth, 0, 0);
    gdk_pixbuf_get_from_drawable(pb, pm, NULL, 0, 0, 0, 0, kWidth, kHeight);

    // Sample pixels
    int components[3] = { 0 };
    for (int y = 2; y < kHeight-2; ++y) {
      for (int c = 0; c < 3; ++c) {
        // Sample a vertical slice of pixels at about one-thirds from the
        // left edge. This allows us to avoid any fixed graphics that might be
        // located at the edges or in the center of the scrollbar.
        // Each pixel is made up of a red, green, and blue component; taking up
        // a total of three bytes.
        components[c] += data[3*(kWidth/3 + y*kWidth) + c];
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
    color->red   = components[0] * 65535 / (255*(kHeight-4));
    color->green = components[1] * 65535 / (255*(kHeight-4));
    color->blue  = components[2] * 65535 / (255*(kHeight-4));

    g_object_unref(pb);
  }
  g_object_unref(pm);

  gtk_widget_destroy(window);
}

CairoCachedSurface* GtkThemeProvider::GetSurfaceNamed(
    int id, GtkWidget* widget_on_display) {
  GdkDisplay* display = gtk_widget_get_display(widget_on_display);
  CairoCachedSurfaceMap& surface_map = per_display_surfaces_[display];

  // Check to see if we already have the pixbuf in the cache.
  CairoCachedSurfaceMap::const_iterator found = surface_map.find(id);
  if (found != surface_map.end())
    return found->second;

  GdkPixbuf* pixbuf = GetPixbufNamed(id);
  CairoCachedSurface* surface = new CairoCachedSurface;
  surface->UsePixbuf(pixbuf);

  surface_map[id] = surface;

  return surface;
}

// static
GdkPixbuf* GtkThemeProvider::GetFolderIcon(bool native) {
  if (native) {
    if (!icon_widget_)
      icon_widget_ = gtk_fixed_new();
    // We never release our ref, so we will leak this on program shutdown.
    if (!default_folder_icon_) {
      default_folder_icon_ =
          gtk_widget_render_icon(icon_widget_, GTK_STOCK_DIRECTORY,
                                 GTK_ICON_SIZE_MENU, NULL);
    }
    if (default_folder_icon_)
      return default_folder_icon_;
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  static GdkPixbuf* default_folder_icon_ = rb.GetPixbufNamed(
      IDR_BOOKMARK_BAR_FOLDER);
  return default_folder_icon_;
}

// static
GdkPixbuf* GtkThemeProvider::GetDefaultFavicon(bool native) {
  if (native) {
    if (!icon_widget_)
      icon_widget_ = gtk_fixed_new();
    // We never release our ref, so we will leak this on program shutdown.
    if (!default_bookmark_icon_) {
      default_bookmark_icon_ =
          gtk_widget_render_icon(icon_widget_, GTK_STOCK_FILE,
                                 GTK_ICON_SIZE_MENU, NULL);
    }
    if (default_bookmark_icon_)
      return default_bookmark_icon_;
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  static GdkPixbuf* default_bookmark_icon_ = rb.GetPixbufNamed(
      IDR_DEFAULT_FAVICON);
  return default_bookmark_icon_;
}

void GtkThemeProvider::ClearAllThemeData() {
  colors_.clear();
  tints_.clear();

  BrowserThemeProvider::ClearAllThemeData();
}

void GtkThemeProvider::LoadThemePrefs() {
  if (use_gtk_) {
    LoadGtkValues();
  } else {
    LoadDefaultValues();
    BrowserThemeProvider::LoadThemePrefs();
  }
}

void GtkThemeProvider::NotifyThemeChanged() {
  BrowserThemeProvider::NotifyThemeChanged();

  // Notify all GtkChromeButtons of their new rendering mode:
  for (std::vector<GtkWidget*>::iterator it = chrome_buttons_.begin();
       it != chrome_buttons_.end(); ++it) {
    gtk_chrome_button_set_use_gtk_rendering(
        GTK_CHROME_BUTTON(*it), use_gtk_);
  }
}

void GtkThemeProvider::FreePlatformCaches() {
  BrowserThemeProvider::FreePlatformCaches();
  FreePerDisplaySurfaces();
  STLDeleteValues(&gtk_images_);
}

// static
void GtkThemeProvider::OnStyleSet(GtkWidget* widget,
                                  GtkStyle* previous_style,
                                  GtkThemeProvider* provider) {
  GdkPixbuf* default_folder_icon = default_folder_icon_;
  GdkPixbuf* default_bookmark_icon = default_bookmark_icon_;
  default_folder_icon_ = NULL;
  default_bookmark_icon_ = NULL;

  if (provider->profile()->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme)) {
    provider->ClearAllThemeData();
    provider->LoadGtkValues();
    provider->NotifyThemeChanged();
  }

  // Free the old icons only after the theme change notification has gone
  // through.
  if (default_folder_icon)
    g_object_unref(default_folder_icon);
  if (default_bookmark_icon)
    g_object_unref(default_bookmark_icon);
}

void GtkThemeProvider::LoadGtkValues() {
  // Before we start setting images and values, we have to clear out old, stale
  // values. (If we don't do this, we'll regress startup time in the case where
  // someone installs a heavyweight theme, then goes back to GTK.)
  DictionaryValue* pref_images =
      profile()->GetPrefs()->GetMutableDictionary(prefs::kCurrentThemeImages);
  pref_images->Clear();

  GtkStyle* window_style = gtk_rc_get_style(fake_window_);
  GtkStyle* label_style = gtk_rc_get_style(fake_label_.get());

  GdkColor frame_color = window_style->bg[GTK_STATE_SELECTED];
  GdkColor inactive_frame_color = window_style->bg[GTK_STATE_INSENSITIVE];
  GdkColor toolbar_color = window_style->bg[GTK_STATE_NORMAL];
  GdkColor button_color = window_style->bg[GTK_STATE_SELECTED];
  GdkColor label_color = label_style->text[GTK_STATE_NORMAL];

  GtkSettings* settings = gtk_settings_get_default();
  bool theme_has_frame_color = false;
  if (settings) {
    GHashTable* color_scheme = NULL;
    g_object_get(settings, "color-hash", &color_scheme, NULL);

    if (color_scheme) {
      // If we have a "gtk-color-scheme" set in this theme, mine it for hints
      // about what we should actually set the frame color to.
      GdkColor* color = NULL;
      if ((color = static_cast<GdkColor*>(
          g_hash_table_lookup(color_scheme, "frame_color")))) {
        frame_color = *color;
        theme_has_frame_color = true;
      }

      if ((color = static_cast<GdkColor*>(
          g_hash_table_lookup(color_scheme, "inactive_frame_color")))) {
        inactive_frame_color = *color;
      }
    }
  }

  if (!theme_has_frame_color) {
    // If the theme's gtkrc doesn't explicitly tell us to use a specific frame
    // color, change the luminosity of the frame color downwards to 80% of what
    // it currently is. This is in a futile attempt to match the default
    // metacity and xfwm themes.
    SkColor shifted = color_utils::HSLShift(GdkToSkColor(&frame_color),
                                            kDefaultFrameShift);
    frame_color.pixel = 0;
    frame_color.red = SkColorGetR(shifted) * kSkiaToGDKMultiplier;
    frame_color.green = SkColorGetG(shifted) * kSkiaToGDKMultiplier;
    frame_color.blue = SkColorGetB(shifted) * kSkiaToGDKMultiplier;
  }

  // By default, the button tint color is the background selection color. But
  // this can be unreadable in some dark themes, so we set a minimum contrast
  // between the button color and the toolbar color.
  color_utils::HSL button_hsl;
  color_utils::SkColorToHSL(GdkToSkColor(&button_color), &button_hsl);
  color_utils::HSL toolbar_hsl;
  color_utils::SkColorToHSL(GdkToSkColor(&toolbar_color), &toolbar_hsl);
  double hsl_difference = fabs(button_hsl.l - toolbar_hsl.l);
  if (hsl_difference <= kMinimumLuminanceDifference) {
    // Not enough contrast. Try the text color instead.
    color_utils::HSL label_hsl;
    color_utils::SkColorToHSL(GdkToSkColor(&label_color), &label_hsl);
    double label_difference = fabs(label_hsl.l - toolbar_hsl.l);
    if (label_difference >= kMinimumLuminanceDifference) {
      button_color = label_color;
    }
  }

  SetThemeTintFromGtk(BrowserThemeProvider::TINT_BUTTONS, &button_color);
  SetThemeTintFromGtk(BrowserThemeProvider::TINT_FRAME, &frame_color);
  SetThemeTintFromGtk(BrowserThemeProvider::TINT_FRAME_INCOGNITO, &frame_color);
  SetThemeTintFromGtk(BrowserThemeProvider::TINT_BACKGROUND_TAB, &frame_color);

  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_FRAME, &frame_color);
  BuildTintedFrameColor(BrowserThemeProvider::COLOR_FRAME_INACTIVE,
                        BrowserThemeProvider::TINT_FRAME_INACTIVE);
  BuildTintedFrameColor(BrowserThemeProvider::COLOR_FRAME_INCOGNITO,
                        BrowserThemeProvider::TINT_FRAME_INCOGNITO);
  BuildTintedFrameColor(BrowserThemeProvider::COLOR_FRAME_INCOGNITO_INACTIVE,
                        BrowserThemeProvider::TINT_FRAME_INCOGNITO_INACTIVE);

  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_TOOLBAR, &toolbar_color);
  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_TAB_TEXT, &label_color);
  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_BOOKMARK_TEXT, &label_color);
  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_CONTROL_BACKGROUND,
                       &window_style->bg[GTK_STATE_NORMAL]);
  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_BUTTON_BACKGROUND,
                       &window_style->bg[GTK_STATE_NORMAL]);

  // The inactive frame color never occurs naturally in the theme, as it is a
  // tinted version of |frame_color|. We generate another color based on the
  // background tab color, with the lightness and saturation moved in the
  // opposite direction. (We don't touch the hue, since there should be subtle
  // hints of the color in the text.)
  color_utils::HSL inactive_tab_text_hsl = tints_[TINT_BACKGROUND_TAB];
  if (inactive_tab_text_hsl.l < 0.5)
    inactive_tab_text_hsl.l = kDarkInactiveLuminance;
  else
    inactive_tab_text_hsl.l = kLightInactiveLuminance;

  if (inactive_tab_text_hsl.s < 0.5)
    inactive_tab_text_hsl.s = kHeavyInactiveSaturation;
  else
    inactive_tab_text_hsl.s = kLightInactiveSaturation;

  colors_[BrowserThemeProvider::COLOR_BACKGROUND_TAB_TEXT] =
      color_utils::HSLToSkColor(inactive_tab_text_hsl, 255);

  // The inactive color/tint is special: We *must* use the exact insensitive
  // color for all inactive windows, otherwise we end up neon pink half the
  // time.
  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_FRAME_INACTIVE,
                       &inactive_frame_color);
  SetTintToExactColor(BrowserThemeProvider::TINT_FRAME_INACTIVE,
                      &inactive_frame_color);
  SetTintToExactColor(BrowserThemeProvider::TINT_FRAME_INCOGNITO_INACTIVE,
                      &inactive_frame_color);

  focus_ring_color_ = GdkToSkColor(&button_color);
  GdkColor thumb_active_color, thumb_inactive_color, track_color;
  GtkThemeProvider::GetScrollbarColors(&thumb_active_color,
                                       &thumb_inactive_color,
                                       &track_color);
  thumb_active_color_ = GdkToSkColor(&thumb_active_color);
  thumb_inactive_color_ = GdkToSkColor(&thumb_inactive_color);
  track_color_ = GdkToSkColor(&track_color);

  // We pick the text and background colors for the NTP out of the colors for a
  // GtkEntry. We do this because GtkEntries background color is never the same
  // as |toolbar_color|, is usually a white, and when it isn't a white,
  // provides sufficient contrast to |toolbar_color|. Try this out with
  // Darklooks, HighContrastInverse or ThinIce.
  GtkStyle* entry_style = gtk_rc_get_style(fake_entry_.get());
  GdkColor ntp_background = entry_style->base[GTK_STATE_NORMAL];
  GdkColor ntp_foreground = entry_style->fg[GTK_STATE_NORMAL];
  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_NTP_BACKGROUND,
                       &ntp_background);
  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_NTP_TEXT,
                       &ntp_foreground);

  // The NTP header is the color that surrounds the current active thumbnail on
  // the NTP, and acts as the border of the "Recent Links" box. It would be
  // awesome if they were separated so we could use GetBorderColor() for the
  // border around the "Recent Links" section, but matching the frame color is
  // more important.
  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_NTP_HEADER,
                       &frame_color);
  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_NTP_SECTION,
                       &toolbar_color);
  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_NTP_SECTION_TEXT,
                       &label_color);

  // Override the link color if the theme provides it.
  const GdkColor* link_color = NULL;
  gtk_widget_style_get(GTK_WIDGET(fake_window_),
                       "link-color", &link_color, NULL);
  if (!link_color)
    link_color = &kDefaultLinkColor;

  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_NTP_LINK,
                       link_color);
  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_NTP_LINK_UNDERLINE,
                       link_color);
  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_NTP_SECTION_LINK,
                       link_color);
  SetThemeColorFromGtk(BrowserThemeProvider::COLOR_NTP_SECTION_LINK_UNDERLINE,
                       link_color);
}

void GtkThemeProvider::LoadDefaultValues() {
  focus_ring_color_ = SkColorSetARGB(255, 229, 151, 0);
  thumb_active_color_ = SkColorSetRGB(250, 248, 245);
  thumb_inactive_color_ = SkColorSetRGB(240, 235, 229);
  track_color_ = SkColorSetRGB(227, 221, 216);
}

void GtkThemeProvider::SetThemeColorFromGtk(int id, const GdkColor* color) {
  colors_[id] = GdkToSkColor(color);
}

void GtkThemeProvider::SetThemeTintFromGtk(int id, const GdkColor* color) {
  color_utils::HSL default_tint = GetDefaultTint(id);
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(GdkToSkColor(color), &hsl);

  if (default_tint.s != -1)
    hsl.s = default_tint.s;

  if (default_tint.l != -1)
    hsl.l = default_tint.l;

  tints_[id] = hsl;
}

void GtkThemeProvider::BuildTintedFrameColor(int color_id, int tint_id) {
  colors_[color_id] = HSLShift(colors_[BrowserThemeProvider::COLOR_FRAME],
                               tints_[tint_id]);
}

void GtkThemeProvider::SetTintToExactColor(int id, const GdkColor* color) {
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(GdkToSkColor(color), &hsl);
  tints_[id] = hsl;
}

void GtkThemeProvider::FreePerDisplaySurfaces() {
  for (PerDisplaySurfaceMap::iterator it = per_display_surfaces_.begin();
       it != per_display_surfaces_.end(); ++it) {
    for (CairoCachedSurfaceMap::iterator jt = it->second.begin();
         jt != it->second.end(); ++jt) {
      delete jt->second;
    }
  }
  per_display_surfaces_.clear();
}

SkBitmap* GtkThemeProvider::GenerateGtkThemeBitmap(int id) const {
  switch (id) {
    case IDR_THEME_TOOLBAR: {
      GtkStyle* style = gtk_rc_get_style(fake_window_);
      GdkColor* color = &style->bg[GTK_STATE_NORMAL];
      SkBitmap* bitmap = new SkBitmap;
      bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                        kToolbarImageWidth, kToolbarImageHeight);
      bitmap->allocPixels();
      bitmap->eraseRGB(color->red >> 8, color->green >> 8, color->blue >> 8);
      return bitmap;
    }
    case IDR_THEME_TAB_BACKGROUND:
      return GenerateTabImage(IDR_THEME_FRAME);
    case IDR_THEME_TAB_BACKGROUND_INCOGNITO:
      return GenerateTabImage(IDR_THEME_FRAME_INCOGNITO);
    case IDR_THEME_FRAME:
      return GenerateFrameImage(BrowserThemeProvider::TINT_FRAME);
    case IDR_THEME_FRAME_INACTIVE:
      return GenerateFrameImage(BrowserThemeProvider::TINT_FRAME_INACTIVE);
    case IDR_THEME_FRAME_INCOGNITO:
      return GenerateFrameImage(BrowserThemeProvider::TINT_FRAME_INCOGNITO);
    case IDR_THEME_FRAME_INCOGNITO_INACTIVE: {
      return GenerateFrameImage(
          BrowserThemeProvider::TINT_FRAME_INCOGNITO_INACTIVE);
    }
    default: {
      // This is a tinted button. Tint it and return it.
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      scoped_ptr<SkBitmap> button(new SkBitmap(*rb.GetBitmapNamed(id)));
      TintMap::const_iterator it = tints_.find(
          BrowserThemeProvider::TINT_BUTTONS);
      DCHECK(it != tints_.end());
      return new SkBitmap(SkBitmapOperations::CreateHSLShiftedBitmap(
          *button, it->second));
    }
  }
}

SkBitmap* GtkThemeProvider::GenerateFrameImage(int tint_id) const {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  scoped_ptr<SkBitmap> frame(new SkBitmap(*rb.GetBitmapNamed(IDR_THEME_FRAME)));
  TintMap::const_iterator it = tints_.find(tint_id);
  DCHECK(it != tints_.end());
  return new SkBitmap(SkBitmapOperations::CreateHSLShiftedBitmap(*frame,
                                                                 it->second));
}

SkBitmap* GtkThemeProvider::GenerateTabImage(int base_id) const {
  SkBitmap* base_image = GetBitmapNamed(base_id);
  SkBitmap bg_tint = SkBitmapOperations::CreateHSLShiftedBitmap(
      *base_image, GetTint(BrowserThemeProvider::TINT_BACKGROUND_TAB));
  return new SkBitmap(SkBitmapOperations::CreateTiledBitmap(
      bg_tint, 0, 0, bg_tint.width(), bg_tint.height()));
}

void GtkThemeProvider::OnDestroyChromeButton(GtkWidget* button,
                                             GtkThemeProvider* provider) {
  std::vector<GtkWidget*>::iterator it =
      find(provider->chrome_buttons_.begin(), provider->chrome_buttons_.end(),
           button);
  if (it != provider->chrome_buttons_.end())
    provider->chrome_buttons_.erase(it);
}
