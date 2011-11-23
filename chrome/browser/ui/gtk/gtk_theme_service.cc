
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/gtk_theme_service.h"

#include <gtk/gtk.h>

#include <set>
#include <string>

#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "base/stl_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/cairo_cached_surface.h"
#include "chrome/browser/ui/gtk/chrome_gtk_frame.h"
#include "chrome/browser/ui/gtk/gtk_chrome_button.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/hover_controller_gtk.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/skia_utils_gtk.h"

namespace {

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

// Number of times that the background color should be counted when trying to
// calculate the border color in GTK theme mode.
const int kBgWeight = 3;

// Padding to left, top and bottom of vertical separators.
const int kSeparatorPadding = 2;

// Default color for links on the NTP when the GTK+ theme doesn't define a
// link color. Constant taken from gtklinkbutton.c.
const GdkColor kDefaultLinkColor = { 0, 0, 0, 0xeeee };

// Middle color of the separator gradient.
const double kMidSeparatorColor[] =
    { 194.0 / 255.0, 205.0 / 255.0, 212.0 / 212.0 };
// Top color of the separator gradient.
const double kTopSeparatorColor[] =
    { 222.0 / 255.0, 234.0 / 255.0, 248.0 / 255.0 };

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

// A list of icons used in the autocomplete view that should be tinted to the
// current gtk theme selection color so they stand out against the GtkEntry's
// base color.
const int kAutocompleteImages[] = {
  IDR_OMNIBOX_EXTENSION_APP,
  IDR_OMNIBOX_HTTP,
  IDR_OMNIBOX_HTTP_DARK,
  IDR_OMNIBOX_HISTORY,
  IDR_OMNIBOX_HISTORY_DARK,
  IDR_OMNIBOX_SEARCH,
  IDR_OMNIBOX_SEARCH_DARK,
  IDR_OMNIBOX_STAR,
  IDR_OMNIBOX_STAR_DARK,
  IDR_GEOLOCATION_ALLOWED_LOCATIONBAR_ICON,
  IDR_GEOLOCATION_DENIED_LOCATIONBAR_ICON,
};

bool IsOverridableImage(int id) {
  CR_DEFINE_STATIC_LOCAL(std::set<int>, images, ());
  if (images.empty()) {
    images.insert(kThemeImages, kThemeImages + arraysize(kThemeImages));
    images.insert(kAutocompleteImages,
                  kAutocompleteImages + arraysize(kAutocompleteImages));

    const std::set<int>& buttons = ThemeService::GetTintableToolbarButtons();
    images.insert(buttons.begin(), buttons.end());
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
  SkColor accent_color = GdkToSkColor(&accent_gdk_color);
  color_utils::HSL accent_tint;
  color_utils::SkColorToHSL(accent_color, &accent_tint);

  color_utils::HSL text_tint;
  color_utils::SkColorToHSL(GdkToSkColor(&text_color), &text_tint);

  color_utils::HSL background_tint;
  color_utils::SkColorToHSL(GdkToSkColor(&background_color), &background_tint);

  // If the accent color is gray, then our normal HSL tomfoolery will bring out
  // whatever color is oddly dominant (for example, in rgb space [125, 128,
  // 125] will tint green instead of gray). Slight differences (+/-10 (4%) to
  // all color components) should be interpreted as this color being gray and
  // we should switch into a special grayscale mode.
  int rb_diff = abs(SkColorGetR(accent_color) - SkColorGetB(accent_color));
  int rg_diff = abs(SkColorGetR(accent_color) - SkColorGetG(accent_color));
  int bg_diff = abs(SkColorGetB(accent_color) - SkColorGetG(accent_color));
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


// Builds and tints the image with |id| to the GtkStateType |state| and
// places the result in |icon_set|.
void BuildIconFromIDRWithColor(int id,
                               GtkStyle* style,
                               GtkStateType state,
                               GtkIconSet* icon_set) {
  SkColor color = GdkToSkColor(&style->fg[state]);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap original = *rb.GetBitmapNamed(id);

  SkBitmap fill_color;
  fill_color.setConfig(SkBitmap::kARGB_8888_Config,
                       original.width(), original.height(), 0);
  fill_color.allocPixels();
  fill_color.eraseColor(color);
  SkBitmap masked = SkBitmapOperations::CreateMaskedBitmap(
      fill_color, original);

  GtkIconSource* icon = gtk_icon_source_new();
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&masked);
  gtk_icon_source_set_pixbuf(icon, pixbuf);
  g_object_unref(pixbuf);

  gtk_icon_source_set_direction_wildcarded(icon, TRUE);
  gtk_icon_source_set_size_wildcarded(icon, TRUE);

  gtk_icon_source_set_state(icon, state);
  // All fields default to wildcarding being on and setting a property doesn't
  // turn off wildcarding. You need to do this yourself. This is stated once in
  // the documentation in the gtk_icon_source_new() function, and no where else.
  gtk_icon_source_set_state_wildcarded(
      icon, state == GTK_STATE_NORMAL);

  gtk_icon_set_add_source(icon_set, icon);
  gtk_icon_source_free(icon);
}

// Applies an HSL shift to a GdkColor (instead of an SkColor)
void GdkColorHSLShift(const color_utils::HSL& shift, GdkColor* frame_color) {
  SkColor shifted = color_utils::HSLShift(GdkToSkColor(frame_color), shift);
  frame_color->pixel = 0;
  frame_color->red = SkColorGetR(shifted) * ui::kSkiaToGDKMultiplier;
  frame_color->green = SkColorGetG(shifted) * ui::kSkiaToGDKMultiplier;
  frame_color->blue = SkColorGetB(shifted) * ui::kSkiaToGDKMultiplier;
}

}  // namespace

GtkWidget* GtkThemeService::icon_widget_ = NULL;
GdkPixbuf* GtkThemeService::default_folder_icon_ = NULL;
GdkPixbuf* GtkThemeService::default_bookmark_icon_ = NULL;

// static
GtkThemeService* GtkThemeService::GetFrom(Profile* profile) {
  return static_cast<GtkThemeService*>(
      ThemeServiceFactory::GetForProfile(profile));
}

GtkThemeService::GtkThemeService()
    : ThemeService(),
      use_gtk_(false),
      fake_window_(gtk_window_new(GTK_WINDOW_TOPLEVEL)),
      fake_frame_(chrome_gtk_frame_new()),
      signals_(new ui::GtkSignalRegistrar),
      fullscreen_icon_set_(NULL) {
  fake_label_.Own(gtk_label_new(""));
  fake_entry_.Own(gtk_entry_new());
  fake_menu_item_.Own(gtk_menu_item_new());

  // Only realized widgets receive style-set notifications, which we need to
  // broadcast new theme images and colors. Only realized widgets have style
  // properties, too, which we query for some colors.
  gtk_widget_realize(fake_frame_);
  gtk_widget_realize(fake_window_);
  signals_->Connect(fake_frame_, "style-set",
                    G_CALLBACK(&OnStyleSetThunk), this);
}

GtkThemeService::~GtkThemeService() {
  gtk_widget_destroy(fake_window_);
  gtk_widget_destroy(fake_frame_);
  fake_label_.Destroy();
  fake_entry_.Destroy();
  fake_menu_item_.Destroy();

  FreeIconSets();

  // We have to call this because FreePlatformCached() in ~ThemeService
  // doesn't call the right virutal FreePlatformCaches.
  FreePlatformCaches();
}

void GtkThemeService::Init(Profile* profile) {
  registrar_.Init(profile->GetPrefs());
  registrar_.Add(prefs::kUsesSystemTheme, this);
#if defined(OS_CHROMEOS)
  use_gtk_ = false;
#else
  use_gtk_ = profile->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme);
#endif
  ThemeService::Init(profile);
}

SkBitmap* GtkThemeService::GetBitmapNamed(int id) const {
  // Try to get our cached version:
  ImageCache::const_iterator it = gtk_images_.find(id);
  if (it != gtk_images_.end())
    return it->second;

  if (use_gtk_ && IsOverridableImage(id)) {
    // We haven't built this image yet:
    SkBitmap* bitmap = GenerateGtkThemeBitmap(id);
    gtk_images_[id] = bitmap;
    return bitmap;
  }

  return ThemeService::GetBitmapNamed(id);
}

SkColor GtkThemeService::GetColor(int id) const {
  if (use_gtk_) {
    ColorMap::const_iterator it = colors_.find(id);
    if (it != colors_.end())
      return it->second;
  }

  return ThemeService::GetColor(id);
}

bool GtkThemeService::HasCustomImage(int id) const {
  if (use_gtk_)
    return IsOverridableImage(id);

  return ThemeService::HasCustomImage(id);
}

void GtkThemeService::InitThemesFor(NotificationObserver* observer) {
  observer->Observe(chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                    content::Source<ThemeService>(this),
                    content::NotificationService::NoDetails());
}

void GtkThemeService::SetTheme(const Extension* extension) {
  profile()->GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, false);
  LoadDefaultValues();
  ThemeService::SetTheme(extension);
}

void GtkThemeService::UseDefaultTheme() {
  profile()->GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, false);
  LoadDefaultValues();
  ThemeService::UseDefaultTheme();
}

void GtkThemeService::SetNativeTheme() {
  profile()->GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, true);
  ClearAllThemeData();
  LoadGtkValues();
  NotifyThemeChanged();
}

bool GtkThemeService::UsingDefaultTheme() const {
  return !use_gtk_ && ThemeService::UsingDefaultTheme();
}

bool GtkThemeService::UsingNativeTheme() const {
  return use_gtk_;
}

void GtkThemeService::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  if ((type == chrome::NOTIFICATION_PREF_CHANGED) &&
      (*content::Details<std::string>(details).ptr() ==
          prefs::kUsesSystemTheme)) {
#if !defined(OS_CHROMEOS)
    use_gtk_ = profile()->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme);
#endif
  } else {
    ThemeService::Observe(type, source, details);
  }
}

GtkWidget* GtkThemeService::BuildChromeButton() {
  GtkWidget* button = HoverControllerGtk::CreateChromeButton();
  gtk_chrome_button_set_use_gtk_rendering(GTK_CHROME_BUTTON(button), use_gtk_);
  chrome_buttons_.push_back(button);

  signals_->Connect(button, "destroy", G_CALLBACK(OnDestroyChromeButtonThunk),
                    this);
  return button;
}

GtkWidget* GtkThemeService::BuildChromeLinkButton(const std::string& text) {
  GtkWidget* link_button = gtk_chrome_link_button_new(text.c_str());
  gtk_chrome_link_button_set_use_gtk_theme(
      GTK_CHROME_LINK_BUTTON(link_button),
      use_gtk_);
  link_buttons_.push_back(link_button);

  signals_->Connect(link_button, "destroy",
                    G_CALLBACK(OnDestroyChromeLinkButtonThunk), this);

  return link_button;
}

GtkWidget* GtkThemeService::BuildLabel(const std::string& text,
                                       GdkColor color) {
  GtkWidget* label = gtk_label_new(text.empty() ? NULL : text.c_str());
  if (!use_gtk_)
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &color);
  labels_.insert(std::make_pair(label, color));

  signals_->Connect(label, "destroy", G_CALLBACK(OnDestroyLabelThunk), this);

  return label;
}

GtkWidget* GtkThemeService::CreateToolbarSeparator() {
  GtkWidget* separator = gtk_vseparator_new();
  GtkWidget* alignment = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
      kSeparatorPadding, kSeparatorPadding, kSeparatorPadding, 0);
  gtk_container_add(GTK_CONTAINER(alignment), separator);

  signals_->Connect(separator, "expose-event",
                    G_CALLBACK(OnSeparatorExposeThunk), this);
  return alignment;
}

GdkColor GtkThemeService::GetGdkColor(int id) const {
  return gfx::SkColorToGdkColor(GetColor(id));
}

GdkColor GtkThemeService::GetBorderColor() const {
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

GtkIconSet* GtkThemeService::GetIconSetForId(int id) const {
  if (id == IDR_FULLSCREEN_MENU_BUTTON)
    return fullscreen_icon_set_;

  return NULL;
}

void GtkThemeService::GetScrollbarColors(GdkColor* thumb_active_color,
                                         GdkColor* thumb_inactive_color,
                                         GdkColor* track_color) {
  const GdkColor* theme_thumb_active = NULL;
  const GdkColor* theme_thumb_inactive = NULL;
  const GdkColor* theme_trough_color = NULL;
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
  GtkStyle*  style  = gtk_rc_get_style(scrollbar);
  GdkPixmap* pm     = gdk_pixmap_new(window->window, kWidth, kHeight, -1);
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
  if (theme_thumb_active)
    *thumb_active_color = *theme_thumb_active;

  if (theme_thumb_inactive)
    *thumb_inactive_color = *theme_thumb_inactive;

  if (theme_trough_color)
    *track_color = *theme_trough_color;
}

CairoCachedSurface* GtkThemeService::GetSurfaceNamed(
    int id,
    GtkWidget* widget_on_display) {
  return GetSurfaceNamedImpl(
      id,
      &per_display_surfaces_,
      &GtkThemeService::GetPixbufNamed,
      widget_on_display);
}

CairoCachedSurface* GtkThemeService::GetRTLEnabledSurfaceNamed(
    int id,
    GtkWidget* widget_on_display) {
  // We flip the sign of |id| when passing it to GetSurfaceNamedImpl() for the
  // same reason that ThemeService::GetPixbufImpl() does: so that if one
  // location calls this function with a resource ID, and another place calls
  // GetSurfaceNamed() with the same ID, they'll correctly get different
  // surfaces in RTL mode.
  return GetSurfaceNamedImpl(
      -id,
      &per_display_surfaces_,
      &GtkThemeService::GetRTLEnabledPixbufNamedWrapper,
      widget_on_display);
}

CairoCachedSurface* GtkThemeService::GetUnthemedSurfaceNamed(
    int id,
    GtkWidget* widget_on_display) {
  return GetSurfaceNamedImpl(id,
      &per_display_unthemed_surfaces_,
      &GtkThemeService::GetUnthemedNativePixbuf,
      widget_on_display);
}

CairoCachedSurface* GtkThemeService::GetCairoIcon(
    int id,
    GtkWidget* widget_on_display) {
  return GetSurfaceNamedImpl(id,
                             &per_display_icon_surfaces_,
                             &GtkThemeService::GetPixbufForIconId,
                             widget_on_display);
}

// static
GdkPixbuf* GtkThemeService::GetFolderIcon(bool native) {
  if (native) {
    if (!icon_widget_)
      icon_widget_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
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
  static GdkPixbuf* default_folder_icon_ = rb.GetNativeImageNamed(
      IDR_BOOKMARK_BAR_FOLDER);
  return default_folder_icon_;
}

// static
GdkPixbuf* GtkThemeService::GetDefaultFavicon(bool native) {
  if (native) {
    if (!icon_widget_)
      icon_widget_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
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
  static GdkPixbuf* default_bookmark_icon_ = rb.GetNativeImageNamed(
      IDR_DEFAULT_FAVICON);
  return default_bookmark_icon_;
}

// static
bool GtkThemeService::DefaultUsesSystemTheme() {
#if defined(OS_CHROMEOS)
  return false;
#else
  scoped_ptr<base::Environment> env(base::Environment::Create());

  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
      return true;
    default:
      return false;
  }
#endif
}

void GtkThemeService::ClearAllThemeData() {
  colors_.clear();
  tints_.clear();

  ThemeService::ClearAllThemeData();
}

void GtkThemeService::LoadThemePrefs() {
  if (use_gtk_) {
    LoadGtkValues();
  } else {
    LoadDefaultValues();
    ThemeService::LoadThemePrefs();
  }

  RebuildMenuIconSets();
}

void GtkThemeService::NotifyThemeChanged() {
  ThemeService::NotifyThemeChanged();

  // Notify all GtkChromeButtons of their new rendering mode:
  for (std::vector<GtkWidget*>::iterator it = chrome_buttons_.begin();
       it != chrome_buttons_.end(); ++it) {
    gtk_chrome_button_set_use_gtk_rendering(
        GTK_CHROME_BUTTON(*it), use_gtk_);
  }

  for (std::vector<GtkWidget*>::iterator it = link_buttons_.begin();
       it != link_buttons_.end(); ++it) {
    gtk_chrome_link_button_set_use_gtk_theme(
        GTK_CHROME_LINK_BUTTON(*it), use_gtk_);
  }

  for (std::map<GtkWidget*, GdkColor>::iterator it = labels_.begin();
       it != labels_.end(); ++it) {
    const GdkColor* color = use_gtk_ ? NULL : &it->second;
    gtk_util::SetLabelColor(it->first, color);
  }

  Browser* browser = BrowserList::GetLastActive();
  if (browser && browser->window()) {
    gtk_util::SetDefaultWindowIcon(browser->window()->GetNativeHandle());
  }
}

void GtkThemeService::FreePlatformCaches() {
  ThemeService::FreePlatformCaches();
  FreePerDisplaySurfaces(&per_display_surfaces_);
  FreePerDisplaySurfaces(&per_display_unthemed_surfaces_);
  FreePerDisplaySurfaces(&per_display_icon_surfaces_);
  STLDeleteValues(&gtk_images_);
}

void GtkThemeService::OnStyleSet(GtkWidget* widget,
                                 GtkStyle* previous_style) {
  GdkPixbuf* default_folder_icon = default_folder_icon_;
  GdkPixbuf* default_bookmark_icon = default_bookmark_icon_;
  default_folder_icon_ = NULL;
  default_bookmark_icon_ = NULL;

  if (profile()->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme)) {
    ClearAllThemeData();
    LoadGtkValues();
    NotifyThemeChanged();
  }

  RebuildMenuIconSets();

  // Free the old icons only after the theme change notification has gone
  // through.
  if (default_folder_icon)
    g_object_unref(default_folder_icon);
  if (default_bookmark_icon)
    g_object_unref(default_bookmark_icon);
}

void GtkThemeService::LoadGtkValues() {
  // Before we start setting images and values, we have to clear out old, stale
  // values. (If we don't do this, we'll regress startup time in the case where
  // someone installs a heavyweight theme, then goes back to GTK.)
  profile()->GetPrefs()->ClearPref(prefs::kCurrentThemeImages);

  GtkStyle* frame_style = gtk_rc_get_style(fake_frame_);

  GtkStyle* window_style = gtk_rc_get_style(fake_window_);
  SetThemeColorFromGtk(ThemeService::COLOR_CONTROL_BACKGROUND,
                       &window_style->bg[GTK_STATE_NORMAL]);

  GdkColor toolbar_color = window_style->bg[GTK_STATE_NORMAL];
  SetThemeColorFromGtk(ThemeService::COLOR_TOOLBAR, &toolbar_color);

  GdkColor button_color = window_style->bg[GTK_STATE_SELECTED];
  SetThemeTintFromGtk(ThemeService::TINT_BUTTONS, &button_color);

  GtkStyle* label_style = gtk_rc_get_style(fake_label_.get());
  GdkColor label_color = label_style->fg[GTK_STATE_NORMAL];
  SetThemeColorFromGtk(ThemeService::COLOR_TAB_TEXT, &label_color);
  SetThemeColorFromGtk(ThemeService::COLOR_BOOKMARK_TEXT, &label_color);

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
  color_utils::HSL inactive_tab_text_hsl = tints_[TINT_BACKGROUND_TAB];
  if (inactive_tab_text_hsl.l < 0.5)
    inactive_tab_text_hsl.l = kDarkInactiveLuminance;
  else
    inactive_tab_text_hsl.l = kLightInactiveLuminance;

  if (inactive_tab_text_hsl.s < 0.5)
    inactive_tab_text_hsl.s = kHeavyInactiveSaturation;
  else
    inactive_tab_text_hsl.s = kLightInactiveSaturation;

  colors_[ThemeService::COLOR_BACKGROUND_TAB_TEXT] =
      color_utils::HSLToSkColor(inactive_tab_text_hsl, 255);

  // We pick the text and background colors for the NTP out of the colors for a
  // GtkEntry. We do this because GtkEntries background color is never the same
  // as |toolbar_color|, is usually a white, and when it isn't a white,
  // provides sufficient contrast to |toolbar_color|. Try this out with
  // Darklooks, HighContrastInverse or ThinIce.
  GtkStyle* entry_style = gtk_rc_get_style(fake_entry_.get());
  GdkColor ntp_background = entry_style->base[GTK_STATE_NORMAL];
  GdkColor ntp_foreground = entry_style->text[GTK_STATE_NORMAL];
  SetThemeColorFromGtk(ThemeService::COLOR_NTP_BACKGROUND,
                       &ntp_background);
  SetThemeColorFromGtk(ThemeService::COLOR_NTP_TEXT,
                       &ntp_foreground);

  // The NTP header is the color that surrounds the current active thumbnail on
  // the NTP, and acts as the border of the "Recent Links" box. It would be
  // awesome if they were separated so we could use GetBorderColor() for the
  // border around the "Recent Links" section, but matching the frame color is
  // more important.
  SetThemeColorFromGtk(ThemeService::COLOR_NTP_HEADER,
                       &frame_color);
  SetThemeColorFromGtk(ThemeService::COLOR_NTP_SECTION,
                       &toolbar_color);
  SetThemeColorFromGtk(ThemeService::COLOR_NTP_SECTION_TEXT,
                       &label_color);

  // Override the link color if the theme provides it.
  const GdkColor* link_color = NULL;
  gtk_widget_style_get(GTK_WIDGET(fake_window_),
                       "link-color", &link_color, NULL);
  if (!link_color)
    link_color = &kDefaultLinkColor;

  SetThemeColorFromGtk(ThemeService::COLOR_NTP_LINK,
                       link_color);
  SetThemeColorFromGtk(ThemeService::COLOR_NTP_LINK_UNDERLINE,
                       link_color);
  SetThemeColorFromGtk(ThemeService::COLOR_NTP_SECTION_LINK,
                       link_color);
  SetThemeColorFromGtk(ThemeService::COLOR_NTP_SECTION_LINK_UNDERLINE,
                       link_color);

  // Generate the colors that we pass to WebKit.
  focus_ring_color_ = GdkToSkColor(&frame_color);
  GdkColor thumb_active_color, thumb_inactive_color, track_color;
  GtkThemeService::GetScrollbarColors(&thumb_active_color,
                                      &thumb_inactive_color,
                                      &track_color);
  thumb_active_color_ = GdkToSkColor(&thumb_active_color);
  thumb_inactive_color_ = GdkToSkColor(&thumb_inactive_color);
  track_color_ = GdkToSkColor(&track_color);

  // Some GTK themes only define the text selection colors on the GtkEntry
  // class, so we need to use that for getting selection colors.
  active_selection_bg_color_ =
      GdkToSkColor(&entry_style->base[GTK_STATE_SELECTED]);
  active_selection_fg_color_ =
      GdkToSkColor(&entry_style->text[GTK_STATE_SELECTED]);
  inactive_selection_bg_color_ =
      GdkToSkColor(&entry_style->base[GTK_STATE_ACTIVE]);
  inactive_selection_fg_color_ =
      GdkToSkColor(&entry_style->text[GTK_STATE_ACTIVE]);
}

GdkColor GtkThemeService::BuildFrameColors(GtkStyle* frame_style) {
  const GdkColor* theme_frame = NULL;
  const GdkColor* theme_inactive_frame = NULL;
  const GdkColor* theme_incognito_frame = NULL;
  const GdkColor* theme_incognito_inactive_frame = NULL;
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
      ThemeService::COLOR_FRAME,
      ThemeService::TINT_FRAME);
  SetThemeTintFromGtk(ThemeService::TINT_BACKGROUND_TAB, &frame_color);

  BuildAndSetFrameColor(
      &frame_style->bg[GTK_STATE_INSENSITIVE],
      theme_inactive_frame,
      kDefaultFrameShift,
      ThemeService::COLOR_FRAME_INACTIVE,
      ThemeService::TINT_FRAME_INACTIVE);

  BuildAndSetFrameColor(
      &frame_color,
      theme_incognito_frame,
      GetDefaultTint(ThemeService::TINT_FRAME_INCOGNITO),
      ThemeService::COLOR_FRAME_INCOGNITO,
      ThemeService::TINT_FRAME_INCOGNITO);

  BuildAndSetFrameColor(
      &frame_color,
      theme_incognito_inactive_frame,
      GetDefaultTint(ThemeService::TINT_FRAME_INCOGNITO_INACTIVE),
      ThemeService::COLOR_FRAME_INCOGNITO_INACTIVE,
      ThemeService::TINT_FRAME_INCOGNITO_INACTIVE);

  return frame_color;
}

void GtkThemeService::LoadDefaultValues() {
  focus_ring_color_ = SkColorSetARGB(255, 229, 151, 0);
  thumb_active_color_ = SkColorSetRGB(244, 244, 244);
  thumb_inactive_color_ = SkColorSetRGB(234, 234, 234);
  track_color_ = SkColorSetRGB(211, 211, 211);

  active_selection_bg_color_ = SkColorSetRGB(30, 144, 255);
  active_selection_fg_color_ = SK_ColorWHITE;
  inactive_selection_bg_color_ = SkColorSetRGB(200, 200, 200);
  inactive_selection_fg_color_ = SkColorSetRGB(50, 50, 50);
}

void GtkThemeService::RebuildMenuIconSets() {
  FreeIconSets();

  GtkStyle* style = gtk_rc_get_style(fake_menu_item_.get());

  fullscreen_icon_set_ = gtk_icon_set_new();
  BuildIconFromIDRWithColor(IDR_FULLSCREEN_MENU_BUTTON,
                            style,
                            GTK_STATE_PRELIGHT,
                            fullscreen_icon_set_);
  BuildIconFromIDRWithColor(IDR_FULLSCREEN_MENU_BUTTON,
                            style,
                            GTK_STATE_NORMAL,
                            fullscreen_icon_set_);
}

void GtkThemeService::SetThemeColorFromGtk(int id, const GdkColor* color) {
  colors_[id] = GdkToSkColor(color);
}

void GtkThemeService::SetThemeTintFromGtk(int id, const GdkColor* color) {
  color_utils::HSL default_tint = GetDefaultTint(id);
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(GdkToSkColor(color), &hsl);

  if (default_tint.s != -1)
    hsl.s = default_tint.s;

  if (default_tint.l != -1)
    hsl.l = default_tint.l;

  tints_[id] = hsl;
}

GdkColor GtkThemeService::BuildAndSetFrameColor(const GdkColor* base,
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

void GtkThemeService::FreePerDisplaySurfaces(
    PerDisplaySurfaceMap* per_display_map) {
  for (PerDisplaySurfaceMap::iterator it = per_display_map->begin();
       it != per_display_map->end(); ++it) {
    for (CairoCachedSurfaceMap::iterator jt = it->second.begin();
         jt != it->second.end(); ++jt) {
      delete jt->second;
    }
  }
  per_display_map->clear();
}

void GtkThemeService::FreeIconSets() {
  if (fullscreen_icon_set_) {
    gtk_icon_set_unref(fullscreen_icon_set_);
    fullscreen_icon_set_ = NULL;
  }
}

SkBitmap* GtkThemeService::GenerateGtkThemeBitmap(int id) const {
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
      return GenerateFrameImage(ThemeService::COLOR_FRAME,
                                "frame-gradient-color");
    case IDR_THEME_FRAME_INACTIVE:
      return GenerateFrameImage(ThemeService::COLOR_FRAME_INACTIVE,
                                "inactive-frame-gradient-color");
    case IDR_THEME_FRAME_INCOGNITO:
      return GenerateFrameImage(ThemeService::COLOR_FRAME_INCOGNITO,
                                "incognito-frame-gradient-color");
    case IDR_THEME_FRAME_INCOGNITO_INACTIVE: {
      return GenerateFrameImage(
          ThemeService::COLOR_FRAME_INCOGNITO_INACTIVE,
          "incognito-inactive-frame-gradient-color");
    }
    // Icons that sit inside the omnibox shouldn't receive TINT_BUTTONS and
    // instead should tint based on the foreground text entry color in GTK+
    // mode because some themes that try to be dark *and* light have very
    // different colors between the omnibox and the normal background area.
    case IDR_OMNIBOX_EXTENSION_APP:
    case IDR_OMNIBOX_HISTORY:
    case IDR_OMNIBOX_HTTP:
    case IDR_OMNIBOX_SEARCH:
    case IDR_OMNIBOX_STAR:
    case IDR_GEOLOCATION_ALLOWED_LOCATIONBAR_ICON:
    case IDR_GEOLOCATION_DENIED_LOCATIONBAR_ICON: {
      return GenerateTintedIcon(id, entry_tint_);
    }
    // In GTK mode, the dark versions of the omnibox icons only ever appear in
    // the autocomplete popup and only against the current theme's GtkEntry
    // base[GTK_STATE_SELECTED] color, so tint the icons so they won't collide
    // with the selected color.
    case IDR_OMNIBOX_EXTENSION_APP_DARK:
    case IDR_OMNIBOX_HISTORY_DARK:
    case IDR_OMNIBOX_HTTP_DARK:
    case IDR_OMNIBOX_SEARCH_DARK:
    case IDR_OMNIBOX_STAR_DARK: {
      return GenerateTintedIcon(id, selected_entry_tint_);
    }
    default: {
      return GenerateTintedIcon(id, button_tint_);
    }
  }
}

SkBitmap* GtkThemeService::GenerateFrameImage(
    int color_id,
    const char* gradient_name) const {
  // We use two colors: the main color (passed in) and a lightened version of
  // that color (which is supposed to match the light gradient at the top of
  // several GTK+ themes, such as Ambiance, Clearlooks or Bluebird).
  ColorMap::const_iterator it = colors_.find(color_id);
  DCHECK(it != colors_.end());
  SkColor base = it->second;

  gfx::CanvasSkia canvas(kToolbarImageWidth, kToolbarImageHeight, true);

  int gradient_size;
  const GdkColor* gradient_top_color = NULL;
  gtk_widget_style_get(GTK_WIDGET(fake_frame_),
                       "frame-gradient-size", &gradient_size,
                       gradient_name, &gradient_top_color,
                       NULL);
  if (gradient_size) {
    SkColor lighter = gradient_top_color ? GdkToSkColor(gradient_top_color)
                      : color_utils::HSLShift(base, kGtkFrameShift);
    SkShader* shader = gfx::CreateGradientShader(
        0, gradient_size, lighter, base);
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    paint.setShader(shader);
    shader->unref();

    canvas.DrawRectInt(0, 0, kToolbarImageWidth, gradient_size, paint);
  }

  canvas.FillRect(base, gfx::Rect(0, gradient_size,
                                  kToolbarImageWidth,
                                  kToolbarImageHeight - gradient_size));
  return new SkBitmap(canvas.ExtractBitmap());
}

SkBitmap* GtkThemeService::GenerateTabImage(int base_id) const {
  SkBitmap* base_image = GetBitmapNamed(base_id);
  SkBitmap bg_tint = SkBitmapOperations::CreateHSLShiftedBitmap(
      *base_image, GetTint(ThemeService::TINT_BACKGROUND_TAB));
  return new SkBitmap(SkBitmapOperations::CreateTiledBitmap(
      bg_tint, 0, 0, bg_tint.width(), bg_tint.height()));
}

SkBitmap* GtkThemeService::GenerateTintedIcon(
    int base_id,
    const color_utils::HSL& tint) const {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return new SkBitmap(SkBitmapOperations::CreateHSLShiftedBitmap(
      *rb.GetBitmapNamed(base_id), tint));
}

void GtkThemeService::GetNormalButtonTintHSL(
    color_utils::HSL* tint) const {
  GtkStyle* window_style = gtk_rc_get_style(fake_window_);
  const GdkColor accent_gdk_color = window_style->bg[GTK_STATE_SELECTED];
  const GdkColor base_color = window_style->base[GTK_STATE_NORMAL];

  GtkStyle* label_style = gtk_rc_get_style(fake_label_.get());
  const GdkColor text_color = label_style->fg[GTK_STATE_NORMAL];

  PickButtonTintFromColors(accent_gdk_color, text_color, base_color, tint);
}

void GtkThemeService::GetNormalEntryForegroundHSL(
    color_utils::HSL* tint) const {
  GtkStyle* window_style = gtk_rc_get_style(fake_window_);
  const GdkColor accent_gdk_color = window_style->bg[GTK_STATE_SELECTED];

  GtkStyle* style = gtk_rc_get_style(fake_entry_.get());
  const GdkColor text_color = style->text[GTK_STATE_NORMAL];
  const GdkColor base_color = style->base[GTK_STATE_NORMAL];

  PickButtonTintFromColors(accent_gdk_color, text_color, base_color, tint);
}

void GtkThemeService::GetSelectedEntryForegroundHSL(
    color_utils::HSL* tint) const {
  // The simplest of all the tints. We just use the selected text in the entry
  // since the icons tinted this way will only be displayed against
  // base[GTK_STATE_SELECTED].
  GtkStyle* style = gtk_rc_get_style(fake_entry_.get());
  const GdkColor color = style->text[GTK_STATE_SELECTED];
  color_utils::SkColorToHSL(GdkToSkColor(&color), tint);
}

CairoCachedSurface* GtkThemeService::GetSurfaceNamedImpl(
    int id,
    PerDisplaySurfaceMap* display_surface_map,
    PixbufProvidingMethod provider,
    GtkWidget* widget_on_display) {
  GdkDisplay* display = gtk_widget_get_display(widget_on_display);
  CairoCachedSurfaceMap& surface_map = (*display_surface_map)[display];

  // Check to see if we already have the pixbuf in the cache.
  CairoCachedSurfaceMap::const_iterator found = surface_map.find(id);
  if (found != surface_map.end())
    return found->second;

  CairoCachedSurface* surface = new CairoCachedSurface;
  surface->UsePixbuf((this->*provider)(id));

  surface_map[id] = surface;

  return surface;
}

// PixbufProvidingMethod that undoes the negative sign on |id|.
GdkPixbuf* GtkThemeService::GetRTLEnabledPixbufNamedWrapper(int id) const {
  return GetRTLEnabledPixbufNamed(-id);
}

// PixbufProvidingMethod that just calls ResourceBundle. We want to minimize
// the calls to this method because it aquires an AutoLock and we don't want
// this to happen all the time.
GdkPixbuf* GtkThemeService::GetUnthemedNativePixbuf(int id) const {
  return ResourceBundle::GetSharedInstance().GetNativeImageNamed(id);
}

// PixbufProvidingMethod that maps a GtkThemeService::CairoDefaultIcon to a
// GdkPixbuf.
GdkPixbuf* GtkThemeService::GetPixbufForIconId(int id) const {
  switch (id) {
    case GtkThemeService::NATIVE_FAVICON:
      return GtkThemeService::GetDefaultFavicon(true);
    case GtkThemeService::CHROME_FAVICON:
      return GtkThemeService::GetDefaultFavicon(false);
    case GtkThemeService::NATIVE_FOLDER:
      return GtkThemeService::GetFolderIcon(true);
    case GtkThemeService::CHROME_FOLDER:
      return GtkThemeService::GetFolderIcon(false);
    default:
      NOTREACHED();
      return NULL;
  }
}

void GtkThemeService::OnDestroyChromeButton(GtkWidget* button) {
  std::vector<GtkWidget*>::iterator it =
      find(chrome_buttons_.begin(), chrome_buttons_.end(), button);
  if (it != chrome_buttons_.end())
    chrome_buttons_.erase(it);
}

void GtkThemeService::OnDestroyChromeLinkButton(GtkWidget* button) {
  std::vector<GtkWidget*>::iterator it =
      find(link_buttons_.begin(), link_buttons_.end(), button);
  if (it != link_buttons_.end())
    link_buttons_.erase(it);
}

void GtkThemeService::OnDestroyLabel(GtkWidget* button) {
  std::map<GtkWidget*, GdkColor>::iterator it = labels_.find(button);
  if (it != labels_.end())
    labels_.erase(it);
}

gboolean GtkThemeService::OnSeparatorExpose(GtkWidget* widget,
                                            GdkEventExpose* event) {
  if (UsingNativeTheme())
    return FALSE;

  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  gdk_cairo_rectangle(cr, &event->area);
  cairo_clip(cr);

  GdkColor bottom_color = GetGdkColor(ThemeService::COLOR_TOOLBAR);
  double bottom_color_rgb[] = {
      static_cast<double>(bottom_color.red / 257) / 255.0,
      static_cast<double>(bottom_color.green / 257) / 255.0,
      static_cast<double>(bottom_color.blue / 257) / 255.0, };

  cairo_pattern_t* pattern =
      cairo_pattern_create_linear(widget->allocation.x, widget->allocation.y,
                                  widget->allocation.x,
                                  widget->allocation.y +
                                  widget->allocation.height);
  cairo_pattern_add_color_stop_rgb(
      pattern, 0.0,
      kTopSeparatorColor[0], kTopSeparatorColor[1], kTopSeparatorColor[2]);
  cairo_pattern_add_color_stop_rgb(
      pattern, 0.5,
      kMidSeparatorColor[0], kMidSeparatorColor[1], kMidSeparatorColor[2]);
  cairo_pattern_add_color_stop_rgb(
      pattern, 1.0,
      bottom_color_rgb[0], bottom_color_rgb[1], bottom_color_rgb[2]);
  cairo_set_source(cr, pattern);

  double start_x = 0.5 + widget->allocation.x;
  cairo_new_path(cr);
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr, start_x, widget->allocation.y);
  cairo_line_to(cr, start_x,
                widget->allocation.y + widget->allocation.height);
  cairo_stroke(cr);
  cairo_destroy(cr);
  cairo_pattern_destroy(pattern);

  return TRUE;
}
