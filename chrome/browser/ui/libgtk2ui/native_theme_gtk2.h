// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_NATIVE_THEME_GTK2_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_NATIVE_THEME_GTK2_H_

#include "chrome/browser/ui/libgtk2ui/owned_widget_gtk2.h"
#include "ui/native_theme/native_theme_base.h"

typedef struct _GdkColor GdkColor;
typedef struct _GtkStyle GtkStyle;
typedef struct _GtkWidget GtkWidget;

namespace libgtk2ui {

// A version of NativeTheme that uses GTK2 supplied colours instead of the
// default aura colours. Analogue to NativeThemeWin, except that can't be
// compiled into the main chrome binary like the Windows code can.
class NativeThemeGtk2 : public ui::NativeThemeBase {
 public:
  static NativeThemeGtk2* instance();

  // Overridden from ui::NativeThemeBase:
  virtual SkColor GetSystemColor(ColorId color_id) const OVERRIDE;
  virtual void PaintMenuPopupBackground(
      SkCanvas* canvas,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& menu_background) const OVERRIDE;

 private:
  NativeThemeGtk2();
  virtual ~NativeThemeGtk2();

  // Implementation of GetSystemColor.
  GdkColor GetSystemGdkColor(ColorId color_id) const;

  // Returns styles associated with various widget types.
  GtkWidget* GetRealizedWindow() const;
  GtkStyle* GetWindowStyle() const;
  GtkStyle* GetEntryStyle() const;
  GtkStyle* GetLabelStyle() const;
  GtkStyle* GetButtonStyle() const;
  GtkStyle* GetMenuStyle() const;
  GtkStyle* GetMenuItemStyle() const;

  mutable GtkWidget* fake_window_;
  mutable OwnedWidgetGtk fake_entry_;
  mutable OwnedWidgetGtk fake_label_;
  mutable OwnedWidgetGtk fake_button_;

  mutable OwnedWidgetGtk fake_menu_;
  mutable GtkWidget* fake_menu_item_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeGtk2);
};

}  // namespace libgtk2ui

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_NATIVE_THEME_GTK2_H_
