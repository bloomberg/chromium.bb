// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/native_theme_gtk2.h"

#include <gtk/gtk.h>

#include "chrome/browser/ui/libgtk2ui/chrome_gtk_menu_subclasses.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_ui.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_util.h"
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

const GdkColor kURLTextColor = GDK_COLOR_RGB(0x00, 0x88, 0x00);

GdkColor GdkAlphaBlend(GdkColor foreground,
                       GdkColor background,
                       SkAlpha alpha) {
  return libgtk2ui::SkColorToGdkColor(
      color_utils::AlphaBlend(libgtk2ui::GdkColorToSkColor(foreground),
                              libgtk2ui::GdkColorToSkColor(background), alpha));
}

// Generates the normal URL color, a green color used in unhighlighted URL
// text. It is a mix of |kURLTextColor| and the current text color.  Unlike the
// selected text color, it is more important to match the qualities of the
// foreground typeface color instead of taking the background into account.
GdkColor NormalURLColor(GdkColor foreground) {
  color_utils::HSL fg_hsl;
  color_utils::SkColorToHSL(libgtk2ui::GdkColorToSkColor(foreground), &fg_hsl);

  color_utils::HSL hue_hsl;
  color_utils::SkColorToHSL(libgtk2ui::GdkColorToSkColor(kURLTextColor),
                            &hue_hsl);

  // Only allow colors that have a fair amount of saturation in them (color vs
  // white). This means that our output color will always be fairly green.
  double s = std::max(0.5, fg_hsl.s);

  // Make sure the luminance is at least as bright as the |kURLTextColor| green
  // would be if we were to use that.
  double l;
  if (fg_hsl.l < hue_hsl.l)
    l = hue_hsl.l;
  else
    l = (fg_hsl.l + hue_hsl.l) / 2;

  color_utils::HSL output = { hue_hsl.h, s, l };
  return libgtk2ui::SkColorToGdkColor(color_utils::HSLToSkColor(output, 255));
}

// Generates the selected URL color, a green color used on URL text in the
// currently highlighted entry in the autocomplete popup. It's a mix of
// |kURLTextColor|, the current text color, and the background color (the
// select highlight). It is more important to contrast with the background
// saturation than to look exactly like the foreground color.
GdkColor SelectedURLColor(GdkColor foreground, GdkColor background) {
  color_utils::HSL fg_hsl;
  color_utils::SkColorToHSL(libgtk2ui::GdkColorToSkColor(foreground),
                            &fg_hsl);

  color_utils::HSL bg_hsl;
  color_utils::SkColorToHSL(libgtk2ui::GdkColorToSkColor(background),
                            &bg_hsl);

  color_utils::HSL hue_hsl;
  color_utils::SkColorToHSL(libgtk2ui::GdkColorToSkColor(kURLTextColor),
                            &hue_hsl);

  // The saturation of the text should be opposite of the background, clamped
  // to 0.2-0.8. We make sure it's greater than 0.2 so there's some color, but
  // less than 0.8 so it's not the oversaturated neon-color.
  double opposite_s = 1 - bg_hsl.s;
  double s = std::max(0.2, std::min(0.8, opposite_s));

  // The luminance should match the luminance of the foreground text.  Again,
  // we clamp so as to have at some amount of color (green) in the text.
  double opposite_l = fg_hsl.l;
  double l = std::max(0.1, std::min(0.9, opposite_l));

  color_utils::HSL output = { hue_hsl.h, s, l };
  return libgtk2ui::SkColorToGdkColor(color_utils::HSLToSkColor(output, 255));
}

}  // namespace


namespace libgtk2ui {

// static
NativeThemeGtk2* NativeThemeGtk2::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeGtk2, s_native_theme, ());
  return &s_native_theme;
}

NativeThemeGtk2::NativeThemeGtk2()
    : fake_window_(NULL),
      fake_tooltip_(NULL),
      fake_menu_item_(NULL) {
}

NativeThemeGtk2::~NativeThemeGtk2() {
  if (fake_window_)
    gtk_widget_destroy(fake_window_);
  if (fake_tooltip_)
    gtk_widget_destroy(fake_tooltip_);

  fake_entry_.Destroy();
  fake_label_.Destroy();
  fake_button_.Destroy();
  fake_tree_.Destroy();
  fake_menu_.Destroy();
}

gfx::Size NativeThemeGtk2::GetPartSize(Part part,
                                       State state,
                                       const ExtraParams& extra) const {
  if (part == kComboboxArrow)
    return gfx::Size(12, 12);

  return ui::NativeThemeBase::GetPartSize(part, state, extra);
}

void NativeThemeGtk2::Paint(SkCanvas* canvas,
                            Part part,
                            State state,
                            const gfx::Rect& rect,
                            const ExtraParams& extra) const {
  if (rect.IsEmpty())
    return;

  switch (part) {
    case kComboboxArrow:
      PaintComboboxArrow(canvas, GetGtkState(state), rect);
      return;

    default:
      NativeThemeBase::Paint(canvas, part, state, rect, extra);
  }
}

SkColor NativeThemeGtk2::GetSystemColor(ColorId color_id) const {
  if (color_id == kColorId_BlueButtonShadowColor)
    return SK_ColorTRANSPARENT;

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
    case kColorId_BlueButtonEnabledColor:
      return GetButtonStyle()->text[GTK_STATE_NORMAL];
    case kColorId_ButtonDisabledColor:
    case kColorId_BlueButtonDisabledColor:
      return GetButtonStyle()->text[GTK_STATE_INSENSITIVE];
    case kColorId_ButtonHighlightColor:
      return GetButtonStyle()->base[GTK_STATE_SELECTED];
    case kColorId_ButtonHoverColor:
    case kColorId_BlueButtonHoverColor:
      return GetButtonStyle()->text[GTK_STATE_PRELIGHT];
    case kColorId_ButtonHoverBackgroundColor:
      return GetButtonStyle()->bg[GTK_STATE_PRELIGHT];
    case kColorId_BlueButtonPressedColor:
      return GetButtonStyle()->text[GTK_STATE_ACTIVE];
    case kColorId_BlueButtonShadowColor:
      // Should be handled in GetSystemColor().
      NOTREACHED();
      return GetButtonStyle()->text[GTK_STATE_NORMAL];

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

    // Tooltips
    case kColorId_TooltipBackground:
      return GetTooltipStyle()->bg[GTK_STATE_NORMAL];
    case kColorId_TooltipText:
      return GetTooltipStyle()->fg[GTK_STATE_NORMAL];

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

      // Results Table
    case kColorId_ResultsTableNormalBackground:
      return GetEntryStyle()->base[GTK_STATE_NORMAL];
    case kColorId_ResultsTableHoveredBackground: {
      GtkStyle* entry_style = GetEntryStyle();
      return GdkAlphaBlend(
          entry_style->base[GTK_STATE_NORMAL],
          entry_style->base[GTK_STATE_SELECTED], 0x80);
    }
    case kColorId_ResultsTableSelectedBackground:
      return GetEntryStyle()->base[GTK_STATE_SELECTED];
    case kColorId_ResultsTableNormalText:
    case kColorId_ResultsTableHoveredText:
      return GetEntryStyle()->text[GTK_STATE_NORMAL];
    case kColorId_ResultsTableSelectedText:
      return GetEntryStyle()->text[GTK_STATE_SELECTED];
    case kColorId_ResultsTableNormalDimmedText:
    case kColorId_ResultsTableHoveredDimmedText: {
      GtkStyle* entry_style = GetEntryStyle();
      return GdkAlphaBlend(
          entry_style->text[GTK_STATE_NORMAL],
          entry_style->base[GTK_STATE_NORMAL], 0x80);
    }
    case kColorId_ResultsTableSelectedDimmedText: {
      GtkStyle* entry_style = GetEntryStyle();
      return GdkAlphaBlend(
          entry_style->text[GTK_STATE_SELECTED],
          entry_style->base[GTK_STATE_NORMAL], 0x80);
    }
    case kColorId_ResultsTableNormalUrl:
    case kColorId_ResultsTableHoveredUrl: {
      return NormalURLColor(GetEntryStyle()->text[GTK_STATE_NORMAL]);
    }
    case kColorId_ResultsTableSelectedUrl: {
      GtkStyle* entry_style = GetEntryStyle();
      return SelectedURLColor(entry_style->text[GTK_STATE_SELECTED],
                              entry_style->base[GTK_STATE_SELECTED]);
    }
    case kColorId_ResultsTableNormalDivider: {
      GtkStyle* win_style = GetWindowStyle();
      return GdkAlphaBlend(win_style->text[GTK_STATE_NORMAL],
                           win_style->bg[GTK_STATE_NORMAL], 0x34);
    }
    case kColorId_ResultsTableHoveredDivider: {
      GtkStyle* win_style = GetWindowStyle();
      return GdkAlphaBlend(win_style->text[GTK_STATE_PRELIGHT],
                           win_style->bg[GTK_STATE_PRELIGHT], 0x34);
    }
    case kColorId_ResultsTableSelectedDivider: {
      GtkStyle* win_style = GetWindowStyle();
      return GdkAlphaBlend(win_style->text[GTK_STATE_SELECTED],
                           win_style->bg[GTK_STATE_SELECTED], 0x34);
    }
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

    // The fake entry needs to be in the window so it can be realized so we can
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

GtkStyle* NativeThemeGtk2::GetTooltipStyle() const {
  if (!fake_tooltip_) {
    fake_tooltip_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(fake_tooltip_, "gtk-tooltip");
    gtk_widget_realize(fake_tooltip_);
  }
  return gtk_rc_get_style(fake_tooltip_);
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

void NativeThemeGtk2::PaintComboboxArrow(SkCanvas* canvas,
                                         GtkStateType state,
                                         const gfx::Rect& rect) const {
  GdkPixmap* pm = gdk_pixmap_new(gtk_widget_get_window(GetRealizedWindow()),
                                 rect.width(),
                                 rect.height(),
                                 -1);
  // Paint the background.
  gtk_paint_flat_box(GetWindowStyle(),
                     pm,
                     state,
                     GTK_SHADOW_NONE,
                     NULL,
                     GetRealizedWindow(),
                     NULL, 0, 0, rect.width(), rect.height());
  gtk_paint_arrow(GetWindowStyle(),
                  pm,
                  state,
                  GTK_SHADOW_NONE,
                  NULL,
                  GetRealizedWindow(),
                  NULL,
                  GTK_ARROW_DOWN,
                  true,
                  0, 0, rect.width(), rect.height());
  GdkPixbuf* pb = gdk_pixbuf_get_from_drawable(NULL,
                                               pm,
                                               gdk_drawable_get_colormap(pm),
                                               0, 0,
                                               0, 0,
                                               rect.width(), rect.height());
  SkBitmap arrow = GdkPixbufToImageSkia(pb);
  canvas->drawBitmap(arrow, rect.x(), rect.y());

  g_object_unref(pb);
  g_object_unref(pm);
}

}  // namespace libgtk2ui
