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

}  // namespace

// static
GtkThemeProvider* GtkThemeProvider::GetFrom(Profile* profile) {
  return static_cast<GtkThemeProvider*>(profile->GetThemeProvider());
}

GtkThemeProvider::GtkThemeProvider()
    : BrowserThemeProvider(),
      fake_window_(gtk_window_new(GTK_WINDOW_TOPLEVEL)) {
  // Only realized widgets receive style-set notifications, which we need to
  // broadcast new theme images and colors.
  gtk_widget_realize(fake_window_);
  g_signal_connect(fake_window_, "style-set", G_CALLBACK(&OnStyleSet), this);
}

GtkThemeProvider::~GtkThemeProvider() {
  profile()->GetPrefs()->RemovePrefObserver(prefs::kUsesSystemTheme, this);
  gtk_widget_destroy(fake_window_);

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

void GtkThemeProvider::LoadThemePrefs() {
  if (use_gtk_) {
    LoadGtkValues();
  } else {
    BrowserThemeProvider::LoadThemePrefs();
  }
}

void GtkThemeProvider::NotifyThemeChanged() {
  BrowserThemeProvider::NotifyThemeChanged();

  // Notify all constructored GtkChromeButtons of their new rendering mode:
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
  GtkStyle* style = gtk_rc_get_style(fake_window_);

  SetThemeColorFromGtk(kColorFrame, &style->bg[GTK_STATE_SELECTED]);
  // Skip COLOR_FRAME_INACTIVE and the incognito colors, as they will be
  // autogenerated from tints.
  SetThemeColorFromGtk(kColorToolbar,
                       &style->bg[GTK_STATE_NORMAL]);
  SetThemeColorFromGtk(kColorTabText,
                       &style->text[GTK_STATE_NORMAL]);
  SetThemeColorFromGtk(kColorBackgroundTabText,
                       &style->text[GTK_STATE_NORMAL]);
  SetThemeColorFromGtk(kColorBookmarkText,
                       &style->text[GTK_STATE_NORMAL]);
  SetThemeColorFromGtk(kColorControlBackground,
                       &style->bg[GTK_STATE_NORMAL]);
  SetThemeColorFromGtk(kColorButtonBackground,
                       &style->bg[GTK_STATE_NORMAL]);

  SetThemeTintFromGtk(kTintButtons, &style->bg[GTK_STATE_SELECTED],
                      kDefaultTintButtons);
  SetThemeTintFromGtk(kTintFrame, &style->bg[GTK_STATE_SELECTED],
                      kDefaultTintFrame);
  SetThemeTintFromGtk(kTintFrameInactive,
                      &style->bg[GTK_STATE_SELECTED],
                      kDefaultTintFrameInactive);
  SetThemeTintFromGtk(kTintFrameIncognito,
                      &style->bg[GTK_STATE_SELECTED],
                      kDefaultTintFrameIncognito);
  SetThemeTintFromGtk(kTintFrameIncognitoInactive,
                      &style->bg[GTK_STATE_SELECTED],
                      kDefaultTintFrameIncognitoInactive);
  SetThemeTintFromGtk(kTintBackgroundTab,
                      &style->bg[GTK_STATE_SELECTED],
                      kDefaultTintBackgroundTab);

  GenerateFrameColors();
  GenerateFrameImages();
}

void GtkThemeProvider::SetThemeColorFromGtk(const char* id, GdkColor* color) {
  SetColor(id, SkColorSetRGB(color->red >> 8,
                             color->green >> 8,
                             color->blue >> 8));
}

void GtkThemeProvider::SetThemeTintFromGtk(const char* id, GdkColor* color,
                                           const skia::HSL& default_tint) {
  skia::HSL hsl;
  skia::SkColorToHSL(SkColorSetRGB((color->red >> 8),
                                   (color->green >> 8),
                                   (color->blue >> 8)), hsl);
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
