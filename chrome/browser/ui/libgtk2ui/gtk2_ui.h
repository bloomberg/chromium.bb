// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UI_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UI_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/libgtk2ui/libgtk2ui_export.h"
#include "chrome/browser/ui/libgtk2ui/owned_widget_gtk2.h"
#include "ui/gfx/color_utils.h"
#include "ui/linux_ui/linux_ui.h"

typedef struct _GdkColor GdkColor;
typedef struct _GtkStyle GtkStyle;
typedef struct _GtkWidget GtkWidget;

class SkBitmap;

namespace gfx {
class Image;
}

namespace libgtk2ui {

// Interface to GTK2 desktop features.
//
class Gtk2UI : public ui::LinuxUI {
 public:
  Gtk2UI();
  virtual ~Gtk2UI();

  // ui::LinuxShellDialog:
  virtual ui::SelectFileDialog* CreateSelectFileDialog(
      ui::SelectFileDialog::Listener* listener,
      ui::SelectFilePolicy* policy) const OVERRIDE;

  // ui::LinuxUI:
  virtual bool UseNativeTheme() const OVERRIDE;
  virtual gfx::Image* GetThemeImageNamed(int id) const OVERRIDE;
  virtual bool GetColor(int id, SkColor* color) const OVERRIDE;
  virtual ui::NativeTheme* GetNativeTheme() const OVERRIDE;

 private:
  typedef std::map<int, SkColor> ColorMap;
  typedef std::map<int, color_utils::HSL> TintMap;
  typedef std::map<int, gfx::Image*> ImageCache;

  // This method returns the colors webkit will use for the scrollbars. When no
  // colors are specified by the GTK+ theme, this function averages of the
  // thumb part and of the track colors.
  void GetScrollbarColors(GdkColor* thumb_active_color,
                          GdkColor* thumb_inactive_color,
                          GdkColor* track_color);

  // Gets the name of the current icon theme and passes it to our low level XDG
  // integration.
  void SetXDGIconTheme();

  // Extracts colors and tints from the GTK theme, both for the
  // ThemeService interface and the colors we send to webkit.
  void LoadGtkValues();

  // Reads in explicit theme frame colors from the ChromeGtkFrame style class
  // or generates them per our fallback algorithm.
  GdkColor BuildFrameColors(GtkStyle* frame_style);

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

  // Renders a GTK icon as a SkBitmap, with prelight/active border if
  // appropriate.
  SkBitmap GenerateGTKIcon(int base_id) const;

  // Renders a GTK button border around a tinted wrench icon.
  SkBitmap GenerateWrenchIcon(int base_id) const;

  // Returns the tint for buttons that contrasts with the normal window
  // background color.
  void GetNormalButtonTintHSL(color_utils::HSL* tint) const;

  // Returns a tint that's the color of the current normal text in an entry.
  void GetNormalEntryForegroundHSL(color_utils::HSL* tint) const;

  // Returns a tint that's the color of the current highlighted text in an
  // entry.
  void GetSelectedEntryForegroundHSL(color_utils::HSL* tint) const;

  // Draws the GTK button border for state |gtk_state| onto a bitmap.
  SkBitmap DrawGtkButtonBorder(int gtk_state, int width, int height) const;

  // Frees all calculated images and color data.
  void ClearAllThemeData();

  GtkWidget* fake_window_;
  GtkWidget* fake_frame_;
  OwnedWidgetGtk fake_label_;
  OwnedWidgetGtk fake_entry_;

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

  // Image cache of lazily created images.
  mutable ImageCache gtk_images_;

  DISALLOW_COPY_AND_ASSIGN(Gtk2UI);
};

}  // namespace libgtk2ui

// Access point to the GTK2 desktop system. This should be the only symbol that
// is exported in the library; everything else should be used through the
// interface, because eventually this .so will be loaded through dlopen at
// runtime so our main binary can conditionally load GTK2 or GTK3 or EFL or
// QT or whatever.
LIBGTK2UI_EXPORT ui::LinuxUI* BuildGtk2UI();

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UI_H_
