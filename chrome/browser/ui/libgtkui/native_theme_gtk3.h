// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_NATIVE_THEME_GTK3_H_
#define CHROME_BROWSER_UI_LIBGTKUI_NATIVE_THEME_GTK3_H_

#include "base/macros.h"
#include "ui/native_theme/native_theme_base.h"

typedef struct _GtkWidget GtkWidget;

namespace libgtkui {

// A version of NativeTheme that uses GTK3-rendered widgets.
class NativeThemeGtk3 : public ui::NativeThemeBase {
 public:
  static NativeThemeGtk3* instance();

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
      const MenuItemExtraParams& menu_item) const override;

 private:
  NativeThemeGtk3();
  ~NativeThemeGtk3() override;

  // Returns various widgets for theming use.
  // TODO(thomasanderson): Remove all of these.
  GtkWidget* GetWindow() const;
  GtkWidget* GetEntry() const;
  GtkWidget* GetLabel() const;
  GtkWidget* GetButton() const;
  GtkWidget* GetBlueButton() const;
  GtkWidget* GetTree() const;
  GtkWidget* GetTooltip() const;
  GtkWidget* GetMenu() const;
  GtkWidget* GetMenuItem() const;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeGtk3);
};

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_NATIVE_THEME_GTK3_H_
