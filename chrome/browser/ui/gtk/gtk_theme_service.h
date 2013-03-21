// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GTK_THEME_SERVICE_H_
#define CHROME_BROWSER_UI_GTK_GTK_THEME_SERVICE_H_

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/themes/theme_service.h"
#include "ui/base/glib/glib_integers.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"
#include "ui/gfx/color_utils.h"

class Profile;

namespace content {
class NotificationObserver;
}

namespace extensions {
class Extension;
}

namespace gfx {
class CairoCachedSurface;
}

namespace ui {
class GtkSignalRegistrar;
}

typedef struct _GdkDisplay GdkDisplay;
typedef struct _GdkEventExpose GdkEventExpose;
typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GtkIconSet GtkIconSet;
typedef struct _GtkStyle GtkStyle;
typedef struct _GtkWidget GtkWidget;

// Specialization of ThemeService which supplies system colors.
class GtkThemeService : public ThemeService {
 public:
  // A list of integer keys for a separate PerDisplaySurfaceMap that keeps
  // what would otherwise be static icons on the X11 server.
  enum CairoDefaultIcon {
    NATIVE_FAVICON = 1,
    CHROME_FAVICON,
    NATIVE_FOLDER,
    CHROME_FOLDER
  };

  // Returns GtkThemeService, casted from our superclass.
  static GtkThemeService* GetFrom(Profile* profile);

  GtkThemeService();
  virtual ~GtkThemeService();

  // Calls |observer|.Observe() for the browser theme with this provider as the
  // source.
  void InitThemesFor(content::NotificationObserver* observer);

  // Overridden from ThemeService:
  //
  // Sets that we aren't using the system theme, then calls
  // ThemeService's implementation.
  virtual void Init(Profile* profile) OVERRIDE;
  virtual gfx::ImageSkia* GetImageSkiaNamed(int id) const OVERRIDE;
  virtual gfx::Image GetImageNamed(int id) const OVERRIDE;
  virtual SkColor GetColor(int id) const OVERRIDE;
  virtual bool HasCustomImage(int id) const OVERRIDE;
  virtual void SetTheme(const extensions::Extension* extension) OVERRIDE;
  virtual void UseDefaultTheme() OVERRIDE;
  virtual void SetNativeTheme() OVERRIDE;
  virtual bool UsingDefaultTheme() const OVERRIDE;
  virtual bool UsingNativeTheme() const OVERRIDE;

  // Creates a GtkChromeButton instance, registered with this theme provider,
  // with a "destroy" signal to remove it from our internal list when it goes
  // away.
  GtkWidget* BuildChromeButton();

  // Creates a GtkChromeLinkButton instance. We update its state as theme
  // changes, and listen for its destruction.
  GtkWidget* BuildChromeLinkButton(const std::string& text);

  // Builds a GtkLabel that is |color| in chrome theme mode, and the normal
  // text color in gtk-mode. Like the previous two calls, listens for the
  // object's destruction.
  GtkWidget* BuildLabel(const std::string& text, const GdkColor& color);

  // Creates a theme-aware vertical separator widget.
  GtkWidget* CreateToolbarSeparator();

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

  // Returns colors that we pass to webkit to match the system theme.
  SkColor get_focus_ring_color() const { return focus_ring_color_; }
  SkColor get_thumb_active_color() const { return thumb_active_color_; }
  SkColor get_thumb_inactive_color() const { return thumb_inactive_color_; }
  SkColor get_track_color() const { return track_color_; }
  SkColor get_active_selection_bg_color() const {
    return active_selection_bg_color_;
  }
  SkColor get_active_selection_fg_color() const {
    return active_selection_fg_color_;
  }
  SkColor get_inactive_selection_bg_color() const {
    return inactive_selection_bg_color_;
  }
  SkColor get_inactive_selection_fg_color() const {
    return inactive_selection_fg_color_;
  }
  SkColor get_location_bar_text_color() const {
    return location_bar_text_color_;
  }
  SkColor get_location_bar_bg_color() const {
    return location_bar_bg_color_;
  }

  // These functions return an image that is not owned by the caller and should
  // not be deleted. If |native| is true, get the GTK_STOCK version of the
  // icon.
  static gfx::Image GetFolderIcon(bool native);
  static gfx::Image GetDefaultFavicon(bool native);

  // Whether we use the GTK theme by default in the current desktop
  // environment. Returns true when we GTK defaults to on.
  static bool DefaultUsesSystemTheme();

 private:
  typedef std::map<int, SkColor> ColorMap;
  typedef std::map<int, color_utils::HSL> TintMap;
  typedef std::map<int, gfx::Image*> ImageCache;

  // Clears all the GTK color overrides.
  virtual void ClearAllThemeData() OVERRIDE;

  // Load theme data from preferences, possibly picking colors from GTK.
  virtual void LoadThemePrefs() OVERRIDE;

  // Let all the browser views know that themes have changed.
  virtual void NotifyThemeChanged() OVERRIDE;

  // Additionally frees the CairoCachedSurfaces.
  virtual void FreePlatformCaches() OVERRIDE;

  // Gets the name of the current icon theme and passes it to our low level XDG
  // integration.
  void SetXDGIconTheme();

  // Extracts colors and tints from the GTK theme, both for the
  // ThemeService interface and the colors we send to webkit.
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

  // Frees all the created GtkIconSets we use for the chrome menu.
  void FreeIconSets();

  // Lazily generates each bitmap used in the gtk theme.
  SkBitmap GenerateGtkThemeBitmap(int id) const;

  // Creates a GTK+ version of IDR_THEME_FRAME. Instead of tinting, this
  // creates a theme configurable gradient ending with |color_id| at the
  // bottom, and |gradient_name| at the top if that color is specified in the
  // theme.
  SkBitmap GenerateFrameImage(int color_id,
                              const char* gradient_name) const;

  // Takes the base frame image |base_id| and tints it with |tint_id|.
  SkBitmap GenerateTabImage(int base_id) const;

  // Tints an icon based on tint.
  SkBitmap GenerateTintedIcon(int base_id,
                              const color_utils::HSL& tint) const;

  // Returns the tint for buttons that contrasts with the normal window
  // background color.
  void GetNormalButtonTintHSL(color_utils::HSL* tint) const;

  // Returns a tint that's the color of the current normal text in an entry.
  void GetNormalEntryForegroundHSL(color_utils::HSL* tint) const;

  // Returns a tint that's the color of the current highlighted text in an
  // entry.
  void GetSelectedEntryForegroundHSL(color_utils::HSL* tint) const;

  // Handles signal from GTK that our theme has been changed.
  CHROMEGTK_CALLBACK_1(GtkThemeService, void, OnStyleSet, GtkStyle*);

  // A notification from various GObject destructors that we should
  // remove it from our internal list.
  CHROMEGTK_CALLBACK_0(GtkThemeService, void, OnDestroyChromeButton);
  CHROMEGTK_CALLBACK_0(GtkThemeService, void, OnDestroyChromeLinkButton);
  CHROMEGTK_CALLBACK_0(GtkThemeService, void, OnDestroyLabel);

  CHROMEGTK_CALLBACK_1(GtkThemeService, gboolean, OnSeparatorExpose,
                       GdkEventExpose*);

  void OnUsesSystemThemeChanged();

  // Whether we should be using gtk rendering.
  bool use_gtk_;

  // GtkWidgets that exist only so we can look at their properties (and take
  // their colors).
  GtkWidget* fake_window_;
  GtkWidget* fake_frame_;
  ui::OwnedWidgetGtk fake_label_;
  ui::OwnedWidgetGtk fake_entry_;
  ui::OwnedWidgetGtk fake_menu_item_;

  // A list of diferent types of widgets that we hold on to these to notify
  // them of theme changes. We do not own these and listen for their
  // destruction via OnDestory{ChromeButton,ChromeLinkButton,Label}.
  std::vector<GtkWidget*> chrome_buttons_;
  std::vector<GtkWidget*> link_buttons_;
  std::map<GtkWidget*, GdkColor> labels_;

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
  SkColor location_bar_bg_color_;
  SkColor location_bar_text_color_;

  // A GtkIconSet that has the tinted icons for the NORMAL and PRELIGHT states
  // of the IDR_FULLSCREEN_MENU_BUTTON tinted to the respective menu item label
  // colors.
  GtkIconSet* fullscreen_icon_set_;

  // Image cache of lazily created images, created when requested by
  // GetImageNamed().
  mutable ImageCache gtk_images_;

  PrefChangeRegistrar registrar_;

  // This is a dummy widget that only exists so we have something to pass to
  // gtk_widget_render_icon().
  static GtkWidget* icon_widget_;

  // The default folder icon and default bookmark icon for the GTK theme.
  // These are static because the system can only have one theme at a time.
  // They are cached when they are requested the first time, and cleared when
  // the system theme changes.
  static base::LazyInstance<gfx::Image> default_folder_icon_;
  static base::LazyInstance<gfx::Image> default_bookmark_icon_;
};

#endif  // CHROME_BROWSER_UI_GTK_GTK_THEME_SERVICE_H_
