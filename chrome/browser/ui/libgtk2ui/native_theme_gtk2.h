// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_NATIVE_THEME_GTK2_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_NATIVE_THEME_GTK2_H_

#include <gtk/gtk.h>

#include "chrome/browser/ui/libgtk2ui/owned_widget_gtk2.h"
#include "ui/native_theme/native_theme_base.h"


namespace libgtk2ui {

// A version of NativeTheme that uses GTK2 supplied colours instead of the
// default aura colours. Analogue to NativeThemeWin, except that can't be
// compiled into the main chrome binary like the Windows code can.
class NativeThemeGtk2 : public ui::NativeThemeBase {
 public:
  static NativeThemeGtk2* instance();

  // Overridden from ui::NativeThemeBase:
  SkColor GetSystemColor(ColorId color_id) const override;
  void PaintMenuPopupBackground(
      SkCanvas* canvas,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& menu_background) const override;
  void PaintMenuItemBackground(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuListExtraParams& menu_list) const override;

  // Returns various widgets for theming use.
  GtkWidget* GetWindow() const;
  GtkWidget* GetEntry() const;
  GtkWidget* GetLabel() const;
  GtkWidget* GetButton() const;
  GtkWidget* GetBlueButton() const;
  GtkWidget* GetTree() const;
  GtkWidget* GetTooltip() const;
  GtkWidget* GetMenu() const;
  GtkWidget* GetMenuItem() const;

 private:
  NativeThemeGtk2();
  ~NativeThemeGtk2() override;

  mutable OwnedWidgetGtk fake_window_;
  mutable OwnedWidgetGtk fake_entry_;
  mutable OwnedWidgetGtk fake_label_;
  mutable OwnedWidgetGtk fake_button_;
  mutable OwnedWidgetGtk fake_bluebutton_;
  mutable OwnedWidgetGtk fake_tree_;
  mutable OwnedWidgetGtk fake_tooltip_;
  mutable OwnedWidgetGtk fake_menu_;
  mutable OwnedWidgetGtk fake_menu_item_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeGtk2);
};

}  // namespace libgtk2ui

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_NATIVE_THEME_GTK2_H_
