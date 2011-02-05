// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GTK_THEME_PROVIDER_H_
#define CHROME_BROWSER_UI_GTK_GTK_THEME_PROVIDER_H_
#pragma once

#include <map>
#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/browser/ui/gtk/owned_widget_gtk.h"
#include "chrome/common/notification_observer.h"
#include "ui/base/gtk/gtk_integers.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/color_utils.h"

class CairoCachedSurface;
class Profile;

namespace ui {
class GtkSignalRegistrar;
}

typedef struct _GdkDisplay GdkDisplay;
typedef struct _GdkEventExpose GdkEventExpose;
typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GtkIconSet GtkIconSet;
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
  virtual void SetTheme(const Extension* extension);
  virtual void UseDefaultTheme();
  virtual void SetNativeTheme();
  virtual bool UsingDefaultTheme();

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Creates a GtkChromeButton instance, registered with this theme provider,
  // with a "destroy" signal to remove it from our internal list when it goes
  // away.
  GtkWidget* BuildChromeButton();

  // Creates a theme-aware vertical separator widget.
  GtkWidget* CreateToolbarSeparator();

  // Whether we should use the GTK system theme.
  bool UseGtkTheme() const;

  // A wrapper around ui::ThemeProvider::GetColor, transforming the result to a
  // GdkColor.
  GdkColor GetGdkColor(int id) const;

  // A weighted average between the text color and the background color of a
  // label. Used for borders between GTK stuff and the webcontent.
  GdkColor GetBorderColor() const;

  // Returns a set of icons tinted for different GtkStateTypes based on the
  // label colors for the IDR resource |id|.
  GtkIconSet* GetIconSetForId(int id) const;

  // This method returns the colors webkit will use for the scrollbars. When no
  // colors are specified by the GTK+ theme, this function averages of the
  // thumb part and of the track colors.
  void GetScrollbarColors(GdkColor* thumb_active_color,
                          GdkColor* thumb_inactive_color,
                          GdkColor* track_color);

  // Expose the inner label. Only used for testing.
  GtkWidget* fake_label() { return fake_label_.get(); }

  // Returns a CairoCachedSurface for a particular Display. CairoCachedSurfaces
  // (hopefully) live on the X server, instead of the client so we don't have
  // to send the image to the server on each expose.
  CairoCachedSurface* GetSurfaceNamed(int id, GtkWidget* widget_on_display);

  // Same as above, but auto-mirrors the underlying pixbuf in RTL mode.
  CairoCachedSurface* GetRTLEnabledSurfaceNamed(int id,
                                                GtkWidget* widget_on_display);

  // Same as above, but gets the resource from the ResourceBundle instead of the
  // BrowserThemeProvider.
  // NOTE: Never call this with resource IDs that are ever passed to the above
  // two functions!  Depending on which call comes first, all callers will
  // either get the themed or the unthemed version.
  CairoCachedSurface* GetUnthemedSurfaceNamed(int id,
                                              GtkWidget* widget_on_display);

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

  // Whether we use the GTK theme by default in the current desktop
  // environment. Returns true when we GTK defaults to on.
  static bool DefaultUsesSystemTheme();

 private:
  typedef std::map<int, SkColor> ColorMap;
  typedef std::map<int, color_utils::HSL> TintMap;
  typedef std::map<int, SkBitmap*> ImageCache;
  typedef std::map<int, CairoCachedSurface*> CairoCachedSurfaceMap;
  typedef std::map<GdkDisplay*, CairoCachedSurfaceMap> PerDisplaySurfaceMap;

  // Clears all the GTK color overrides.
  virtual void ClearAllThemeData();

  // Load theme data from preferences, possibly picking colors from GTK.
  virtual void LoadThemePrefs();

  // Let all the browser views know that themes have changed.
  virtual void NotifyThemeChanged(const Extension* extension);

  // Additionally frees the CairoCachedSurfaces.
  virtual void FreePlatformCaches();

  // Extracts colors and tints from the GTK theme, both for the
  // BrowserThemeProvider interface and the colors we send to webkit.
  void LoadGtkValues();

  // Reads in explicit theme frame colors from the ChromeGtkFrame style class
  // or generates them per our fallback algorithm.
  GdkColor BuildFrameColors(GtkStyle* frame_style);

  // Sets the values that we send to webkit to safe defaults.
  void LoadDefaultValues();

  // Builds all of the tinted menus images needed for custom buttons. This is
  // always called on style-set even if we aren't using the gtk-theme because
  // the menus are always rendered with gtk colors.
  void RebuildMenuIconSets();

  // Sets the underlying theme colors/tints from a GTK color.
  void SetThemeColorFromGtk(int id, const GdkColor* color);
  void SetThemeTintFromGtk(int id, const GdkColor* color);

  // Creates and returns a frame color, either using |gtk_base| verbatim if
  // non-NULL, or tinting |base| with |tint|. Also sets |color_id| and
  // |tint_id| to the returned color.
  GdkColor BuildAndSetFrameColor(const GdkColor* base,
                                 const GdkColor* gtk_base,
                                 const color_utils::HSL& tint,
                                 int color_id,
                                 int tint_id);

  // Split out from FreePlatformCaches so it can be called in our destructor;
  // FreePlatformCaches() is called from the BrowserThemeProvider's destructor,
  // but by the time ~BrowserThemeProvider() is run, the vtable no longer
  // points to GtkThemeProvider's version.
  void FreePerDisplaySurfaces(PerDisplaySurfaceMap* per_display_map);

  // Frees all the created GtkIconSets we use for the chrome menu.
  void FreeIconSets();

  // Lazily generates each bitmap used in the gtk theme.
  SkBitmap* GenerateGtkThemeBitmap(int id) const;

  // Creates a GTK+ version of IDR_THEME_FRAME. Instead of tinting, this
  // creates a theme configurable gradient ending with |color_id| at the
  // bottom, and |gradient_name| at the top if that color is specified in the
  // theme.
  SkBitmap* GenerateFrameImage(int color_id,
                               const char* gradient_name) const;

  // Takes the base frame image |base_id| and tints it with |tint_id|.
  SkBitmap* GenerateTabImage(int base_id) const;

  // Tints an icon based on tint.
  SkBitmap* GenerateTintedIcon(int base_id, color_utils::HSL tint) const;

  // Returns the tint for buttons that contrasts with the normal window
  // background color.
  void GetNormalButtonTintHSL(color_utils::HSL* tint) const;

  // Returns a tint that's the color of the current normal text in an entry.
  void GetNormalEntryForegroundHSL(color_utils::HSL* tint) const;

  // Returns a tint that's the color of the current highlighted text in an
  // entry.
  void GetSelectedEntryForegroundHSL(color_utils::HSL* tint) const;

  // Implements GetXXXSurfaceNamed(), given the appropriate pixbuf to use.
  CairoCachedSurface* GetSurfaceNamedImpl(int id,
                                          PerDisplaySurfaceMap* surface_map,
                                          GdkPixbuf* pixbuf,
                                          GtkWidget* widget_on_display);

  // Handles signal from GTK that our theme has been changed.
  CHROMEGTK_CALLBACK_1(GtkThemeProvider, void, OnStyleSet, GtkStyle*);

  // A notification from the GtkChromeButton GObject destructor that we should
  // remove it from our internal list.
  CHROMEGTK_CALLBACK_0(GtkThemeProvider, void, OnDestroyChromeButton);

  CHROMEGTK_CALLBACK_1(GtkThemeProvider, gboolean, OnSeparatorExpose,
                       GdkEventExpose*);

  // Whether we should be using gtk rendering.
  bool use_gtk_;

  // GtkWidgets that exist only so we can look at their properties (and take
  // their colors).
  GtkWidget* fake_window_;
  GtkWidget* fake_frame_;
  OwnedWidgetGtk fake_label_;
  OwnedWidgetGtk fake_entry_;
  OwnedWidgetGtk fake_menu_item_;

  // A list of all GtkChromeButton instances. We hold on to these to notify
  // them of theme changes.
  std::vector<GtkWidget*> chrome_buttons_;

  // Tracks all the signals we have connected to on various widgets.
  scoped_ptr<ui::GtkSignalRegistrar> signals_;

  // Tints and colors calculated by LoadGtkValues() that are given to the
  // caller while |use_gtk_| is true.
  ColorMap colors_;
  TintMap tints_;

  // Colors used to tint certain icons.
  color_utils::HSL button_tint_;
  color_utils::HSL entry_tint_;
  color_utils::HSL selected_entry_tint_;

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

  // A GtkIconSet that has the tinted icons for the NORMAL and PRELIGHT states
  // of the IDR_FULLSCREEN_MENU_BUTTON tinted to the respective menu item label
  // colors.
  GtkIconSet* fullscreen_icon_set_;

  // Image cache of lazily created images, created when requested by
  // GetBitmapNamed().
  mutable ImageCache gtk_images_;

  // Cairo surfaces for each GdkDisplay.
  PerDisplaySurfaceMap per_display_surfaces_;
  PerDisplaySurfaceMap per_display_unthemed_surfaces_;

  PrefChangeRegistrar registrar_;

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

#endif  // CHROME_BROWSER_UI_GTK_GTK_THEME_PROVIDER_H_
