// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/gtk_theme_provider.h"

#include <gtk/gtk.h>

#include "base/gfx/gtk_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

// The size of the rendered toolbar image.
const int kToolbarImageWidth = 64;
const int kToolbarImageHeight = 128;

const skia::HSL kExactColor = { -1, -1, -1 };

const skia::HSL kDefaultFrameShift = { -1, -1, 0.4 };

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

// Converts a GdkColor to a SkColor.
SkColor GdkToSkColor(GdkColor* color) {
  return SkColorSetRGB(color->red >> 8,
                       color->green >> 8,
                       color->blue >> 8);
}

}  // namespace

// static
GtkThemeProvider* GtkThemeProvider::GetFrom(Profile* profile) {
  return static_cast<GtkThemeProvider*>(profile->GetThemeProvider());
}

GtkThemeProvider::GtkThemeProvider()
    : BrowserThemeProvider(),
      fake_window_(gtk_window_new(GTK_WINDOW_TOPLEVEL)) {
  fake_label_.Own(gtk_label_new(""));

  // Only realized widgets receive style-set notifications, which we need to
  // broadcast new theme images and colors.
  gtk_widget_realize(fake_window_);
  g_signal_connect(fake_window_, "style-set", G_CALLBACK(&OnStyleSet), this);
}

GtkThemeProvider::~GtkThemeProvider() {
  profile()->GetPrefs()->RemovePrefObserver(prefs::kUsesSystemTheme, this);
  gtk_widget_destroy(fake_window_);
  fake_label_.Destroy();

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

void GtkThemeProvider::InitThemesFor(NotificationObserver* observer) {
  observer->Observe(NotificationType::BROWSER_THEME_CHANGED,
                    Source<ThemeProvider>(this),
                    NotificationService::NoDetails());
}

void GtkThemeProvider::SetTheme(Extension* extension) {
  profile()->GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, false);
  BrowserThemeProvider::SetTheme(extension);
}

void GtkThemeProvider::UseDefaultTheme() {
  profile()->GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, false);
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
  if (type == NotificationType::PREF_CHANGED) {
    std::wstring key = *Details<std::wstring>(details).ptr();
    if (key == prefs::kUsesSystemTheme) {
      use_gtk_ = profile()->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme);
    }
  }
}

GtkWidget* GtkThemeProvider::BuildChromeButton() {
  GtkWidget* button = gtk_chrome_button_new();
  gtk_chrome_button_set_use_gtk_rendering(GTK_CHROME_BUTTON(button), use_gtk_);
  chrome_buttons_.push_back(button);

  g_signal_connect(button, "destroy", G_CALLBACK(OnDestroyChromeButton),
                   this);
  return button;
}

bool GtkThemeProvider::UseGtkTheme() {
  return use_gtk_;
}

GdkColor GtkThemeProvider::GetGdkColor(int id) {
  SkColor color = GetColor(id);
  GdkColor gdkcolor =
      GDK_COLOR_RGB(SkColorGetR(color), SkColorGetG(color),
                    SkColorGetB(color));
  return gdkcolor;
}

GdkColor GtkThemeProvider::GetBorderColor() {
  GtkStyle* style = gtk_rc_get_style(fake_window_);

  // Creates a weighted average between the text and base color where
  // the base color counts more than once.
  GdkColor color;
  color.pixel = 0;
  color.red = (style->text[GTK_STATE_NORMAL].red +
               (style->bg[GTK_STATE_NORMAL].red * kBgWeight)) /
              (1 + kBgWeight);
  color.green = (style->text[GTK_STATE_NORMAL].green +
                 (style->bg[GTK_STATE_NORMAL].green * kBgWeight)) /
                (1 + kBgWeight);
  color.blue = (style->text[GTK_STATE_NORMAL].blue +
                (style->bg[GTK_STATE_NORMAL].blue * kBgWeight)) /
               (1 + kBgWeight);

  return color;
}

void GtkThemeProvider::LoadThemePrefs() {
  if (use_gtk_) {
    LoadGtkValues();
  } else {
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

SkBitmap* GtkThemeProvider::LoadThemeBitmap(int id) {
  if (id == IDR_THEME_TOOLBAR && use_gtk_) {
    GtkStyle* style = gtk_rc_get_style(fake_window_);
    GdkColor* color = &style->bg[GTK_STATE_NORMAL];
    SkBitmap* bitmap = new SkBitmap;
    bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                      kToolbarImageWidth, kToolbarImageHeight);
    bitmap->allocPixels();
    bitmap->eraseRGB(color->red >> 8, color->green >> 8, color->blue >> 8);
    return bitmap;
  } else {
    return BrowserThemeProvider::LoadThemeBitmap(id);
  }
}

void GtkThemeProvider::SaveThemeBitmap(const std::string resource_name,
                                       int id) {
  if (!use_gtk_) {
    // Prevent us from writing out our mostly unused resources in gtk theme
    // mode. Simply preventing us from writing this data out in gtk mode isn't
    // the best design, but this would probably be a very invasive change on
    // all three platforms otherwise.
    BrowserThemeProvider::SaveThemeBitmap(resource_name, id);
  }
}

// static
void GtkThemeProvider::OnStyleSet(GtkWidget* widget,
                                  GtkStyle* previous_style,
                                  GtkThemeProvider* provider) {
  if (provider->profile()->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme)) {
    provider->ClearAllThemeData();
    provider->LoadGtkValues();
    provider->NotifyThemeChanged();
  }
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
    SkColor shifted = skia::HSLShift(GdkToSkColor(&frame_color),
                                     kDefaultFrameShift);
    frame_color.pixel = 0;
    frame_color.red = SkColorGetR(shifted) * kSkiaToGDKMultiplier;
    frame_color.green = SkColorGetG(shifted) * kSkiaToGDKMultiplier;
    frame_color.blue = SkColorGetB(shifted) * kSkiaToGDKMultiplier;
  }

  // By default, the button tint color is the background selection color. But
  // this can be unreadable in some dark themes, so we set a minimum contrast
  // between the button color and the toolbar color.
  skia::HSL button_hsl;
  skia::SkColorToHSL(GdkToSkColor(&button_color), button_hsl);
  skia::HSL toolbar_hsl;
  skia::SkColorToHSL(GdkToSkColor(&toolbar_color), toolbar_hsl);
  double hsl_difference = fabs(button_hsl.l - toolbar_hsl.l);
  if (hsl_difference <= kMinimumLuminanceDifference) {
    // Not enough contrast. Try the text color instead.
    skia::HSL label_hsl;
    skia::SkColorToHSL(GdkToSkColor(&label_color), label_hsl);
    double label_difference = fabs(label_hsl.l - toolbar_hsl.l);
    if (label_difference >= kMinimumLuminanceDifference) {
      button_color = label_color;
    }
  }

  SetThemeColorFromGtk(kColorFrame, &frame_color);
  // Skip COLOR_FRAME_INACTIVE and the incognito colors, as they will be
  // autogenerated from tints.
  SetThemeColorFromGtk(kColorToolbar, &toolbar_color);
  SetThemeColorFromGtk(kColorTabText, &label_color);
  SetThemeColorFromGtk(kColorBookmarkText, &label_color);
  SetThemeColorFromGtk(kColorControlBackground,
                       &window_style->bg[GTK_STATE_NORMAL]);
  SetThemeColorFromGtk(kColorButtonBackground,
                       &window_style->bg[GTK_STATE_NORMAL]);

  SetThemeTintFromGtk(kTintButtons, &button_color,
                      kDefaultTintButtons);
  SetThemeTintFromGtk(kTintFrame, &frame_color,
                      kDefaultTintFrame);
  SetThemeTintFromGtk(kTintFrameIncognito,
                      &frame_color,
                      kDefaultTintFrameIncognito);
  SetThemeTintFromGtk(kTintBackgroundTab,
                      &frame_color,
                      kDefaultTintBackgroundTab);

  // The inactive frame color never occurs naturally in the theme, as it is a
  // tinted version of |frame_color|. We generate another color based on the
  // background tab color, with the lightness and saturation moved in the
  // opposite direction. (We don't touch the hue, since there should be subtle
  // hints of the color in the text.)
  skia::HSL inactive_tab_text_hsl = GetTint(TINT_BACKGROUND_TAB);
  if (inactive_tab_text_hsl.l < 0.5)
    inactive_tab_text_hsl.l = kDarkInactiveLuminance;
  else
    inactive_tab_text_hsl.l = kLightInactiveLuminance;

  if (inactive_tab_text_hsl.s < 0.5)
    inactive_tab_text_hsl.s = kHeavyInactiveSaturation;
  else
    inactive_tab_text_hsl.s = kLightInactiveSaturation;

  SetColor(kColorBackgroundTabText,
           skia::HSLToSkColor(255, inactive_tab_text_hsl));

  // The inactive color/tint is special: We *must* use the exact insensitive
  // color for all inactive windows, otherwise we end up neon pink half the
  // time.
  SetThemeColorFromGtk(kColorFrameInactive, &inactive_frame_color);
  SetThemeTintFromGtk(kTintFrameInactive, &inactive_frame_color,
                      kExactColor);
  SetThemeTintFromGtk(kTintFrameIncognitoInactive, &inactive_frame_color,
                      kExactColor);

  force_process_images();
  GenerateFrameColors();
  GenerateFrameImages();
  GenerateTabImages();
}

void GtkThemeProvider::SetThemeColorFromGtk(const char* id, GdkColor* color) {
  SetColor(id, GdkToSkColor(color));
}

void GtkThemeProvider::SetThemeTintFromGtk(const char* id, GdkColor* color,
                                           const skia::HSL& default_tint) {
  skia::HSL hsl;
  skia::SkColorToHSL(GdkToSkColor(color), hsl);

  if (default_tint.s != -1)
    hsl.s = default_tint.s;

  if (default_tint.l != -1)
    hsl.l = default_tint.l;
  SetTint(id, hsl);
}

void GtkThemeProvider::OnDestroyChromeButton(GtkWidget* button,
                                             GtkThemeProvider* provider) {
  std::vector<GtkWidget*>::iterator it =
      find(provider->chrome_buttons_.begin(), provider->chrome_buttons_.end(),
           button);
  if (it != provider->chrome_buttons_.end())
    provider->chrome_buttons_.erase(it);
}
