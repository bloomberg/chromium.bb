// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/native_theme_gtk2.h"

#include <gtk/gtk.h>

#include "chrome/browser/ui/libgtk2ui/skia_utils_gtk2.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/native_theme/common_theme.h"

namespace {

// Theme colors returned by GetSystemColor().
const SkColor kInvalidColorIdColor = SkColorSetRGB(255, 0, 128);
// Tree
const SkColor kTreeBackground = SK_ColorWHITE;
const SkColor kTreeTextColor = SK_ColorBLACK;
const SkColor kTreeSelectedTextColor = SK_ColorBLACK;
const SkColor kTreeSelectionBackgroundColor = SkColorSetRGB(0xEE, 0xEE, 0xEE);
const SkColor kTreeArrowColor = SkColorSetRGB(0x7A, 0x7A, 0x7A);
// Table
const SkColor kTableBackground = SK_ColorWHITE;
const SkColor kTableTextColor = SK_ColorBLACK;
const SkColor kTableSelectedTextColor = SK_ColorBLACK;
const SkColor kTableSelectionBackgroundColor = SkColorSetRGB(0xEE, 0xEE, 0xEE);
const SkColor kTableGroupingIndicatorColor = SkColorSetRGB(0xCC, 0xCC, 0xCC);

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
  fake_menu_.Destroy();
}

SkColor NativeThemeGtk2::GetSystemColor(ColorId color_id) const {
  switch (color_id) {
    // TODO(erg): Still need to fish the colors out of trees and tables.

    // Tree
    case kColorId_TreeBackground:
      return kTreeBackground;
    case kColorId_TreeText:
      return kTreeTextColor;
    case kColorId_TreeSelectedText:
    case kColorId_TreeSelectedTextUnfocused:
      return kTreeSelectedTextColor;
    case kColorId_TreeSelectionBackgroundFocused:
    case kColorId_TreeSelectionBackgroundUnfocused:
      return kTreeSelectionBackgroundColor;
    case kColorId_TreeArrow:
      return kTreeArrowColor;

    // Table
    case kColorId_TableBackground:
      return kTableBackground;
    case kColorId_TableText:
      return kTableTextColor;
    case kColorId_TableSelectedText:
    case kColorId_TableSelectedTextUnfocused:
      return kTableSelectedTextColor;
    case kColorId_TableSelectionBackgroundFocused:
    case kColorId_TableSelectionBackgroundUnfocused:
      return kTableSelectionBackgroundColor;
    case kColorId_TableGroupingIndicatorColor:
      return kTableGroupingIndicatorColor;

    default:
      // Fall through.
      break;
  }

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
      return GetMenuItemStyle()->text[GTK_STATE_NORMAL];
    case kColorId_DisabledMenuItemForegroundColor:
      return GetMenuItemStyle()->text[GTK_STATE_INSENSITIVE];
    // TODO(erg): We also need a FocusedMenuItemForegroundColor, since the
    // accessibility theme HighContrastInverse is unreadable without it. That
    // will require careful threading through existing menu code though.
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

    // TextButton
    case kColorId_TextButtonBackgroundColor:
      return GetButtonStyle()->bg[GTK_STATE_NORMAL];
    case kColorId_TextButtonEnabledColor:
      return GetButtonStyle()->text[GTK_STATE_NORMAL];
    case kColorId_TextButtonDisabledColor:
      return GetButtonStyle()->text[GTK_STATE_INSENSITIVE];
    case kColorId_TextButtonHighlightColor:
      return GetButtonStyle()->base[GTK_STATE_SELECTED];
    case kColorId_TextButtonHoverColor:
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
    case kColorId_TextfieldSelectionBackgroundUnfocused:
      return GetEntryStyle()->base[GTK_STATE_ACTIVE];

    // Tree
    // Table

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

GtkStyle* NativeThemeGtk2::GetMenuStyle() const {
  if (!fake_menu_.get())
    fake_menu_.Own(gtk_menu_new());
  return gtk_rc_get_style(fake_menu_.get());
}

GtkStyle* NativeThemeGtk2::GetMenuItemStyle() const {
  if (!fake_menu_item_) {
    if (!fake_menu_.get())
      fake_menu_.Own(gtk_menu_new());

    fake_menu_item_ = gtk_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(fake_menu_.get()), fake_menu_item_);
  }

  return gtk_rc_get_style(fake_menu_item_);
}

}  // namespace libgtk2ui
