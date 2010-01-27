// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_GTK_THEME_PROVIDER_H_
#define CHROME_BROWSER_GTK_GTK_THEME_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "app/gfx/color_utils.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/owned_widget_gtk.h"

class CairoCachedSurface;
class Profile;

typedef struct _GdkDisplay GdkDisplay;
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
  virtual SkBitmap* GetBitmapNamed(int id) const;
  virtual SkColor GetColor(int id) const;
  virtual bool HasCustomImage(int id) const;
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
  bool UseGtkTheme() const;

  // A wrapper around ThemeProvider::GetColor, transforming the result to a
  // GdkColor.
  GdkColor GetGdkColor(int id) const;

  // A weighted average between the text color and the background color of a
  // label. Used for borders between GTK stuff and the webcontent.
  GdkColor GetBorderColor() const;

  // This method returns averages of the thumb part and of the track colors.
  // Used when rendering scrollbars.
  static void GetScrollbarColors(GdkColor* thumb_active_color,
                                 GdkColor* thumb_inactive_color,
                                 GdkColor* track_color);

  // Expose the inner label. Only used for testing.
  GtkWidget* fake_label() { return fake_label_.get(); }

  // Returns a CairoCachedSurface for a particular Display. CairoCachedSurfaces
  // (hopefully) live on the X server, instead of the client so we don't have
  // to send the image to the server on each expose.
  CairoCachedSurface* GetSurfaceNamed(int id, GtkWidget* widget_on_display);

  // Returns colors that we pass to webkit to match the system theme.
  const SkColor& get_focus_ring_color() const { return focus_ring_color_; }
  const SkColor& get_thumb_active_color() const { return thumb_active_color_; }
  const SkColor& get_thumb_inactive_color() const {
    return thumb_inactive_color_;
  }
  const SkColor& get_track_color() const { return track_color_; }
  const SkColor& get_active_selection_bg_color() const {
    return active_selection_bg_color_;
  }
  const SkColor& get_active_selection_fg_color() const {
    return active_selection_fg_color_;
  }
  const SkColor& get_inactive_selection_bg_color() const {
    return inactive_selection_bg_color_;
  }
  const SkColor& get_inactive_selection_fg_color() const {
    return inactive_selection_fg_color_;
  }

  // These functions do not add a ref to the returned pixbuf, and it should not
  // be unreffed.  If |native| is true, get the GTK_STOCK version of the icon.
  static GdkPixbuf* GetFolderIcon(bool native);
  static GdkPixbuf* GetDefaultFavicon(bool native);

 private:
  typedef std::map<int, SkColor> ColorMap;
  typedef std::map<int, color_utils::HSL> TintMap;
  typedef std::map<int, SkBitmap*> ImageCache;

  // Clears all the GTK color overrides.
  virtual void ClearAllThemeData();

  // Load theme data from preferences, possibly picking colors from GTK.
  virtual void LoadThemePrefs();

  // Let all the browser views know that themes have changed.
  virtual void NotifyThemeChanged();

  // Additionally frees the CairoCachedSurfaces.
  virtual void FreePlatformCaches();

  // Handles signal from GTK that our theme has been changed.
  static void OnStyleSet(GtkWidget* widget,
                         GtkStyle* previous_style,
                         GtkThemeProvider* provider);

  // Extracts colors and tints from the GTK theme, both for the
  // BrowserThemeProvider interface and the colors we send to webkit.
  void LoadGtkValues();

  // Returns a GtkStyle* from which we get the colors for our frame. Checks for
  // the optional "MetaFrames" widget class before returning the default
  // GtkWindow one.
  GtkStyle* GetFrameStyle();

  // Sets the values that we send to webkit to safe defaults.
  void LoadDefaultValues();

  // Sets the underlying theme colors/tints from a GTK color.
  void SetThemeColorFromGtk(int id, const GdkColor* color);
  void SetThemeTintFromGtk(int id, const GdkColor* color);
  void BuildTintedFrameColor(int color_id, int tint_id);
  void SetTintToExactColor(int id, const GdkColor* color);

  // Split out from FreePlatformCaches so it can be called in our destructor;
  // FreePlatformCaches() is called from the BrowserThemeProvider's destructor,
  // but by the time ~BrowserThemeProvider() is run, the vtable no longer
  // points to GtkThemeProvider's version.
  void FreePerDisplaySurfaces();

  // Lazily generates each bitmap used in the gtk theme.
  SkBitmap* GenerateGtkThemeBitmap(int id) const;

  // Tints IDR_THEME_FRAME based based on |tint_id|. Used during lazy
  // generation of the gtk theme bitmaps.
  SkBitmap* GenerateFrameImage(int tint_id) const;

  // Takes the base frame image |base_id| and tints it with |tint_id|.
  SkBitmap* GenerateTabImage(int base_id) const;

  // A notification from the GtkChromeButton GObject destructor that we should
  // remove it from our internal list.
  static void OnDestroyChromeButton(GtkWidget* button,
                                    GtkThemeProvider* provider);

  // Whether we should be using gtk rendering.
  bool use_gtk_;

  // GtkWidgets that exist only so we can look at their properties (and take
  // their colors).
  GtkWidget* fake_window_;
  OwnedWidgetGtk fake_label_;
  OwnedWidgetGtk fake_entry_;

  // A list of all GtkChromeButton instances. We hold on to these to notify
  // them of theme changes.
  std::vector<GtkWidget*> chrome_buttons_;

  // Tints and colors calculated by LoadGtkValues() that are given to the
  // caller while |use_gtk_| is true.
  ColorMap colors_;
  TintMap tints_;

  // Colors that we pass to WebKit. These are generated each time the theme
  // changes.
  SkColor focus_ring_color_;
  SkColor thumb_active_color_;
  SkColor thumb_inactive_color_;
  SkColor track_color_;
  SkColor active_selection_bg_color_;
  SkColor active_selection_fg_color_;
  SkColor inactive_selection_bg_color_;
  SkColor inactive_selection_fg_color_;

  // Image cache of lazily created images, created when requested by
  // GetBitmapNamed().
  mutable ImageCache gtk_images_;

  // Cairo surfaces for each GdkDisplay.
  typedef std::map<int, CairoCachedSurface*> CairoCachedSurfaceMap;
  typedef std::map<GdkDisplay*, CairoCachedSurfaceMap> PerDisplaySurfaceMap;
  PerDisplaySurfaceMap per_display_surfaces_;

  // This is a dummy widget that only exists so we have something to pass to
  // gtk_widget_render_icon().
  static GtkWidget* icon_widget_;

  // The default folder icon and default bookmark icon for the GTK theme.
  // These are static because the system can only have one theme at a time.
  // They are cached when they are requested the first time, and cleared when
  // the system theme changes.
  static GdkPixbuf* default_folder_icon_;
  static GdkPixbuf* default_bookmark_icon_;
};

#endif  // CHROME_BROWSER_GTK_GTK_THEME_PROVIDER_H_
