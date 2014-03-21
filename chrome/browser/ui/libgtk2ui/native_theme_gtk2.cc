// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/native_theme_gtk2.h"

#include <gtk/gtk.h>

#include "chrome/browser/ui/libgtk2ui/chrome_gtk_menu_subclasses.h"
#include "chrome/browser/ui/libgtk2ui/skia_utils_gtk2.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"

namespace {

// Theme colors returned by GetSystemColor().
const SkColor kInvalidColorIdColor = SkColorSetRGB(255, 0, 128);

}  // namespace


namespace libgtk2ui {

// static
NativeThemeGtk2* NativeThemeGtk2::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeGtk2, s_native_theme, ());
  return &s_native_theme;
}

NativeThemeGtk2::NativeThemeGtk2()
    : fake_window_(NULL),
      fake_menu_item_(NULL) {
}

NativeThemeGtk2::~NativeThemeGtk2() {
  if (fake_window_)
    gtk_widget_destroy(fake_window_);
  fake_entry_.Destroy();
  fake_label_.Destroy();
  fake_button_.Destroy();
  fake_tree_.Destroy();
  fake_menu_.Destroy();
}

SkColor NativeThemeGtk2::GetSystemColor(ColorId color_id) const {
  return GdkColorToSkColor(GetSystemGdkColor(color_id));
}

void NativeThemeGtk2::PaintMenuPopupBackground(
    SkCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background) const {
  if (menu_background.corner_radius > 0) {
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    paint.setColor(GetSystemColor(kColorId_MenuBackgroundColor));

    gfx::Path path;
    SkRect rect = SkRect::MakeWH(SkIntToScalar(size.width()),
                                 SkIntToScalar(size.height()));
    SkScalar radius = SkIntToScalar(menu_background.corner_radius);
    SkScalar radii[8] = {radius, radius, radius, radius,
                         radius, radius, radius, radius};
    path.addRoundRect(rect, radii);

    canvas->drawPath(path, paint);
  } else {
    canvas->drawColor(GetSystemColor(kColorId_MenuBackgroundColor),
                      SkXfermode::kSrc_Mode);
  }
}

void NativeThemeGtk2::PaintMenuItemBackground(
    SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuListExtraParams& menu_list) const {
  SkColor color;
  SkPaint paint;
  switch (state) {
    case NativeTheme::kNormal:
    case NativeTheme::kDisabled:
      color = GetSystemColor(NativeTheme::kColorId_MenuBackgroundColor);
      paint.setColor(color);
      break;
    case NativeTheme::kHovered:
      color = GetSystemColor(
          NativeTheme::kColorId_FocusedMenuItemBackgroundColor);
      paint.setColor(color);
      break;
    default:
      NOTREACHED() << "Invalid state " << state;
      break;
  }
  canvas->drawRect(gfx::RectToSkRect(rect), paint);
}

GdkColor NativeThemeGtk2::GetSystemGdkColor(ColorId color_id) const {
  switch (color_id) {
    // Windows
    case kColorId_WindowBackground:
      return GetWindowStyle()->bg[GTK_STATE_NORMAL];

    // Dialogs
    case kColorId_DialogBackground:
      return GetWindowStyle()->bg[GTK_STATE_NORMAL];

    // FocusableBorder
    case kColorId_FocusedBorderColor:
      return GetEntryStyle()->bg[GTK_STATE_SELECTED];
    case kColorId_UnfocusedBorderColor:
      return GetEntryStyle()->text_aa[GTK_STATE_NORMAL];

    // MenuItem
    case kColorId_EnabledMenuItemForegroundColor:
    case kColorId_DisabledEmphasizedMenuItemForegroundColor:
      return GetMenuItemStyle()->text[GTK_STATE_NORMAL];
    case kColorId_DisabledMenuItemForegroundColor:
      return GetMenuItemStyle()->text[GTK_STATE_INSENSITIVE];
    case kColorId_SelectedMenuItemForegroundColor:
      return GetMenuItemStyle()->text[GTK_STATE_SELECTED];
    case kColorId_FocusedMenuItemBackgroundColor:
      return GetMenuItemStyle()->bg[GTK_STATE_SELECTED];
    case kColorId_HoverMenuItemBackgroundColor:
      return GetMenuItemStyle()->bg[GTK_STATE_PRELIGHT];
    case kColorId_FocusedMenuButtonBorderColor:
      return GetEntryStyle()->bg[GTK_STATE_NORMAL];
    case kColorId_HoverMenuButtonBorderColor:
      return GetEntryStyle()->text_aa[GTK_STATE_PRELIGHT];
    case kColorId_MenuBorderColor:
    case kColorId_EnabledMenuButtonBorderColor:
    case kColorId_MenuSeparatorColor: {
      return GetMenuItemStyle()->text[GTK_STATE_INSENSITIVE];
    }
    case kColorId_MenuBackgroundColor:
      return GetMenuStyle()->bg[GTK_STATE_NORMAL];

    // Label
    case kColorId_LabelEnabledColor:
      return GetLabelStyle()->text[GTK_STATE_NORMAL];
    case kColorId_LabelDisabledColor:
      return GetLabelStyle()->text[GTK_STATE_INSENSITIVE];
    case kColorId_LabelBackgroundColor:
      return GetWindowStyle()->bg[GTK_STATE_NORMAL];

    // Button
    case kColorId_ButtonBackgroundColor:
      return GetButtonStyle()->bg[GTK_STATE_NORMAL];
    case kColorId_ButtonEnabledColor:
      return GetButtonStyle()->text[GTK_STATE_NORMAL];
    case kColorId_ButtonDisabledColor:
      return GetButtonStyle()->text[GTK_STATE_INSENSITIVE];
    case kColorId_ButtonHighlightColor:
      return GetButtonStyle()->base[GTK_STATE_SELECTED];
    case kColorId_ButtonHoverColor:
      return GetButtonStyle()->text[GTK_STATE_PRELIGHT];

    // Textfield
    case kColorId_TextfieldDefaultColor:
      return GetEntryStyle()->text[GTK_STATE_NORMAL];
    case kColorId_TextfieldDefaultBackground:
      return GetEntryStyle()->base[GTK_STATE_NORMAL];
    case kColorId_TextfieldReadOnlyColor:
      return GetEntryStyle()->text[GTK_STATE_INSENSITIVE];
    case kColorId_TextfieldReadOnlyBackground:
      return GetEntryStyle()->base[GTK_STATE_INSENSITIVE];
    case kColorId_TextfieldSelectionColor:
      return GetEntryStyle()->text[GTK_STATE_SELECTED];
    case kColorId_TextfieldSelectionBackgroundFocused:
      return GetEntryStyle()->base[GTK_STATE_SELECTED];

    // Trees and Tables (implemented on GTK using the same class)
    case kColorId_TableBackground:
    case kColorId_TreeBackground:
      return GetTreeStyle()->bg[GTK_STATE_NORMAL];
    case kColorId_TableText:
    case kColorId_TreeText:
      return GetTreeStyle()->text[GTK_STATE_NORMAL];
    case kColorId_TableSelectedText:
    case kColorId_TableSelectedTextUnfocused:
    case kColorId_TreeSelectedText:
    case kColorId_TreeSelectedTextUnfocused:
      return GetTreeStyle()->text[GTK_STATE_SELECTED];
    case kColorId_TableSelectionBackgroundFocused:
    case kColorId_TableSelectionBackgroundUnfocused:
    case kColorId_TreeSelectionBackgroundFocused:
    case kColorId_TreeSelectionBackgroundUnfocused:
      return GetTreeStyle()->bg[GTK_STATE_SELECTED];
    case kColorId_TreeArrow:
      return GetTreeStyle()->fg[GTK_STATE_NORMAL];
    case kColorId_TableGroupingIndicatorColor:
      return GetTreeStyle()->text_aa[GTK_STATE_NORMAL];

    default:
      // Fall through
      break;
  }

  return SkColorToGdkColor(kInvalidColorIdColor);
}

GtkWidget* NativeThemeGtk2::GetRealizedWindow() const {
  if (!fake_window_) {
    fake_window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_realize(fake_window_);
  }

  return fake_window_;
}

GtkStyle* NativeThemeGtk2::GetWindowStyle() const {
  return gtk_rc_get_style(GetRealizedWindow());
}

GtkStyle* NativeThemeGtk2::GetEntryStyle() const {
  if (!fake_entry_.get()) {
    fake_entry_.Own(gtk_entry_new());

    // The fake entry needs to be in the window so it can be realized sow e can
    // use the computed parts of the style.
    gtk_container_add(GTK_CONTAINER(GetRealizedWindow()), fake_entry_.get());
    gtk_widget_realize(fake_entry_.get());
  }
  return gtk_rc_get_style(fake_entry_.get());
}

GtkStyle* NativeThemeGtk2::GetLabelStyle() const {
  if (!fake_label_.get())
    fake_label_.Own(gtk_label_new(""));

  return gtk_rc_get_style(fake_label_.get());
}

GtkStyle* NativeThemeGtk2::GetButtonStyle() const {
  if (!fake_button_.get())
    fake_button_.Own(gtk_button_new());

  return gtk_rc_get_style(fake_button_.get());
}

GtkStyle* NativeThemeGtk2::GetTreeStyle() const {
  if (!fake_tree_.get())
    fake_tree_.Own(gtk_tree_view_new());

  return gtk_rc_get_style(fake_tree_.get());
}

GtkStyle* NativeThemeGtk2::GetMenuStyle() const {
  if (!fake_menu_.get())
    fake_menu_.Own(gtk_menu_new());
  return gtk_rc_get_style(fake_menu_.get());
}

GtkStyle* NativeThemeGtk2::GetMenuItemStyle() const {
  if (!fake_menu_item_) {
    if (!fake_menu_.get())
      fake_menu_.Own(gtk_custom_menu_new());

    fake_menu_item_ = gtk_custom_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(fake_menu_.get()), fake_menu_item_);
  }

  return gtk_rc_get_style(fake_menu_item_);
}

}  // namespace libgtk2ui
