// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_GTK_THEME_PROVIDER_H_
#define CHROME_BROWSER_GTK_GTK_THEME_PROVIDER_H_

#include <vector>

#include "chrome/browser/browser_theme_provider.h"
#include "chrome/common/notification_observer.h"

#include "skia/ext/skia_utils.h"

class Profile;

typedef struct _GtkStyle GtkStyle;
typedef struct _GtkWidget GtkWidget;

// Specialization of BrowserThemeProvider which supplies system colors.
class GtkThemeProvider : public BrowserThemeProvider,
                         public NotificationObserver {
 public:
  // Returns GtkThemeProvider, casted from our superclass.
  static GtkThemeProvider* GetFrom(Profile* profile);

  GtkThemeProvider();
  virtual ~GtkThemeProvider();

  // Calls |observer|.Observe() for the browser theme with this provider as the
  // source.
  void InitThemesFor(NotificationObserver* observer);

  // Overridden from BrowserThemeProvider:
  //
  // Sets that we aren't using the system theme, then calls
  // BrowserThemeProvider's implementation.
  virtual void Init(Profile* profile);
  virtual void SetTheme(Extension* extension);
  virtual void UseDefaultTheme();
  virtual void SetNativeTheme();

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Creates a GtkChromeButton instance, registered with this theme provider,
  // with a "destroy" signal to remove it from our internal list when it goes
  // away.
  GtkWidget* BuildChromeButton();

  // Whether we should use the GTK system theme.
  bool UseGtkTheme();

  // A wrapper around ThemeProvider::GetColor, transforming the result to a
  // GdkColor.
  GdkColor GetGdkColor(int id);

 private:
  // Load theme data from preferences, possibly picking colors from GTK.
  virtual void LoadThemePrefs();

  // Let all the browser views know that themes have changed.
  virtual void NotifyThemeChanged();

  // Possibly creates a theme specific version of theme_toolbar_default.
  // (minimally acceptable version right now, which is just a fill of the bg
  // color; this should instead invoke gtk_draw_box(...) for complex theme
  // engines.)
  virtual SkBitmap* LoadThemeBitmap(int id);

  // Handles signal from GTK that our theme has been changed.
  static void OnStyleSet(GtkWidget* widget,
                         GtkStyle* previous_style,
                         GtkThemeProvider* provider);

  void LoadGtkValues();

  // Sets the underlying theme colors/tints from a GTK color.
  void SetThemeColorFromGtk(const char* id, GdkColor* color);
  void SetThemeTintFromGtk(const char* id, GdkColor* color,
                           const skia::HSL& default_tint);

  // A notification from the GtkChromeButton GObject destructor that we should
  // remove it from our internal list.
  static void OnDestroyChromeButton(GtkWidget* button,
                                    GtkThemeProvider* provider);

  // Whether we should be using gtk rendering.
  bool use_gtk_;

  // A GtkWidget that exists only so we can look at its properties (and take
  // its colors).
  GtkWidget* fake_window_;

  // A list of all GtkChromeButton instances. We hold on to these to notify
  // them of theme changes.
  std::vector<GtkWidget*> chrome_buttons_;
};

#endif  // CHROME_BROWSER_GTK_GTK_THEME_PROVIDER_H_
