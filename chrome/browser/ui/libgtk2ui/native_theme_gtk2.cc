// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/native_theme_gtk2.h"

#include <gtk/gtk.h>

#include "chrome/browser/ui/libgtk2ui/chrome_gtk_frame.h"
#include "chrome/browser/ui/libgtk2ui/chrome_gtk_menu_subclasses.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_ui.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_util.h"
#include "chrome/browser/ui/libgtk2ui/skia_utils_gtk2.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"

namespace libgtk2ui {

namespace {

// Theme colors returned by GetSystemColor().
const SkColor kInvalidColorIdColor = SkColorSetRGB(255, 0, 128);
const SkColor kURLTextColor = SkColorSetRGB(0x0b, 0x80, 0x43);

// Generates the normal URL color, a green color used in unhighlighted URL
// text. It is a mix of |kURLTextColor| and the current text color.  Unlike the
// selected text color, it is more important to match the qualities of the
// foreground typeface color instead of taking the background into account.
SkColor NormalURLColor(SkColor foreground) {
  color_utils::HSL fg_hsl, hue_hsl;
  color_utils::SkColorToHSL(foreground, &fg_hsl);
  color_utils::SkColorToHSL(kURLTextColor, &hue_hsl);

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
  return color_utils::HSLToSkColor(output, 255);
}

// Generates the selected URL color, a green color used on URL text in the
// currently highlighted entry in the autocomplete popup. It's a mix of
// |kURLTextColor|, the current text color, and the background color (the
// select highlight). It is more important to contrast with the background
// saturation than to look exactly like the foreground color.
SkColor SelectedURLColor(SkColor foreground, SkColor background) {
  color_utils::HSL fg_hsl, bg_hsl, hue_hsl;
  color_utils::SkColorToHSL(foreground, &fg_hsl);
  color_utils::SkColorToHSL(background, &bg_hsl);
  color_utils::SkColorToHSL(kURLTextColor, &hue_hsl);

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
  return color_utils::HSLToSkColor(output, 255);
}

enum WidgetState {
  NORMAL = 0,
  ACTIVE = 1,
  PRELIGHT = 2,
  SELECTED = 3,
  INSENSITIVE = 4,
};

#if GTK_MAJOR_VERSION == 2
const WidgetState kTextboxInactiveState = ACTIVE;
#else
const WidgetState kTextboxInactiveState = SELECTED;
#endif

#if GTK_MAJOR_VERSION == 2
// Same order as enum WidgetState above
const GtkStateType stateMap[] = {
  GTK_STATE_NORMAL,
  GTK_STATE_ACTIVE,
  GTK_STATE_PRELIGHT,
  GTK_STATE_SELECTED,
  GTK_STATE_INSENSITIVE,
};

SkColor GetFGColor(GtkWidget* widget, WidgetState state) {
  return GdkColorToSkColor(gtk_rc_get_style(widget)->fg[stateMap[state]]);
}
SkColor GetBGColor(GtkWidget* widget, WidgetState state) {
  return GdkColorToSkColor(gtk_rc_get_style(widget)->bg[stateMap[state]]);
}

SkColor GetTextColor(GtkWidget* widget, WidgetState state) {
  return GdkColorToSkColor(gtk_rc_get_style(widget)->text[stateMap[state]]);
}
SkColor GetTextAAColor(GtkWidget* widget, WidgetState state) {
  return GdkColorToSkColor(gtk_rc_get_style(widget)->text_aa[stateMap[state]]);
}
SkColor GetBaseColor(GtkWidget* widget, WidgetState state) {
  return GdkColorToSkColor(gtk_rc_get_style(widget)->base[stateMap[state]]);
}

#else
// Same order as enum WidgetState above
const GtkStateFlags stateMap[] = {
  GTK_STATE_FLAG_NORMAL,
  GTK_STATE_FLAG_ACTIVE,
  GTK_STATE_FLAG_PRELIGHT,
  GTK_STATE_FLAG_SELECTED,
  GTK_STATE_FLAG_INSENSITIVE,
};

SkColor GetFGColor(GtkWidget* widget, WidgetState state) {
  GdkRGBA color;
  gtk_style_context_get_color(
      gtk_widget_get_style_context(widget), stateMap[state], &color);
  return SkColorSetRGB(color.red * 255, color.green * 255, color.blue * 255);
}
SkColor GetBGColor(GtkWidget* widget, WidgetState state) {
  GdkRGBA color;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_style_context_get_background_color(
      gtk_widget_get_style_context(widget), stateMap[state], &color);
  G_GNUC_END_IGNORE_DEPRECATIONS

  // Hack for default color
  if (color.alpha == 0.0)
    color = {1, 1, 1, 1};

  return SkColorSetRGB(color.red * 255, color.green * 255, color.blue * 255);
}

SkColor GetTextColor(GtkWidget* widget, WidgetState state) {
  return GetFGColor(widget, state);
}
SkColor GetTextAAColor(GtkWidget* widget, WidgetState state) {
  return GetFGColor(widget, state);
}
SkColor GetBaseColor(GtkWidget* widget, WidgetState state) {
  return GetBGColor(widget, state);
}

#endif

}  // namespace

// static
NativeThemeGtk2* NativeThemeGtk2::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeGtk2, s_native_theme, ());
  return &s_native_theme;
}

// Constructors automatically called
NativeThemeGtk2::NativeThemeGtk2() {}
// This doesn't actually get called
NativeThemeGtk2::~NativeThemeGtk2() {}

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

SkColor NativeThemeGtk2::GetSystemColor(ColorId color_id) const {
  const SkColor kPositiveTextColor = SkColorSetRGB(0x0b, 0x80, 0x43);
  const SkColor kNegativeTextColor = SkColorSetRGB(0xc5, 0x39, 0x29);

  switch (color_id) {
    // Windows
    case kColorId_WindowBackground:
      return GetBGColor(GetWindow(), SELECTED);

    // Dialogs
    case kColorId_DialogBackground:
    case kColorId_BubbleBackground:
      return GetBGColor(GetWindow(), NORMAL);

    // FocusableBorder
    case kColorId_FocusedBorderColor:
      return GetBGColor(GetEntry(), SELECTED);
    case kColorId_UnfocusedBorderColor:
      return GetTextAAColor(GetEntry(), NORMAL);

    // MenuItem
    case kColorId_EnabledMenuItemForegroundColor:
    case kColorId_DisabledEmphasizedMenuItemForegroundColor:
      return GetTextColor(GetMenuItem(), NORMAL);
    case kColorId_DisabledMenuItemForegroundColor:
      return GetTextColor(GetMenuItem(), INSENSITIVE);
    case kColorId_SelectedMenuItemForegroundColor:
      return GetTextColor(GetMenuItem(), SELECTED);
    case kColorId_FocusedMenuItemBackgroundColor:
      return GetBGColor(GetMenuItem(), SELECTED);
    case kColorId_HoverMenuItemBackgroundColor:
      return GetBGColor(GetMenuItem(), PRELIGHT);
    case kColorId_FocusedMenuButtonBorderColor:
      return GetBGColor(GetEntry(), NORMAL);
    case kColorId_HoverMenuButtonBorderColor:
      return GetTextAAColor(GetEntry(), PRELIGHT);
    case kColorId_MenuBorderColor:
    case kColorId_EnabledMenuButtonBorderColor:
    case kColorId_MenuSeparatorColor: {
      return GetTextColor(GetMenuItem(), INSENSITIVE);
    }
    case kColorId_MenuBackgroundColor:
      return GetBGColor(GetMenu(), NORMAL);

    // Label
    case kColorId_LabelEnabledColor:
      return GetTextColor(GetEntry(), NORMAL);
    case kColorId_LabelDisabledColor:
      return GetTextColor(GetLabel(), INSENSITIVE);
    case kColorId_LabelBackgroundColor:
      return GetBGColor(GetWindow(), NORMAL);

    // Link
    // TODO(estade): get these from the gtk theme instead of hardcoding.
    case kColorId_LinkDisabled:
      return SK_ColorBLACK;
    case kColorId_LinkEnabled:
      return SkColorSetRGB(0x33, 0x67, 0xD6);
    case kColorId_LinkPressed:
      return SK_ColorRED;

    // Button
    case kColorId_ButtonBackgroundColor:
      return GetBGColor(GetButton(), NORMAL);
    case kColorId_ButtonEnabledColor:
      return GetTextColor(GetButton(), NORMAL);
    case kColorId_BlueButtonEnabledColor:
      return GetTextColor(GetBlueButton(), NORMAL);
    case kColorId_ButtonDisabledColor:
      return GetTextColor(GetButton(), INSENSITIVE);
    case kColorId_BlueButtonDisabledColor:
      return GetTextColor(GetBlueButton(), INSENSITIVE);
    case kColorId_ButtonHighlightColor:
      return GetBaseColor(GetButton(), SELECTED);
    case kColorId_ButtonHoverColor:
      return GetTextColor(GetButton(), PRELIGHT);
    case kColorId_BlueButtonHoverColor:
      return GetTextColor(GetBlueButton(), PRELIGHT);
    case kColorId_ButtonHoverBackgroundColor:
      return GetBGColor(GetButton(), PRELIGHT);
    case kColorId_BlueButtonPressedColor:
      return GetTextColor(GetBlueButton(), ACTIVE);
    case kColorId_BlueButtonShadowColor:
      return SK_ColorTRANSPARENT;
      // return GetTextColor(GetButton(), NORMAL);

    // Textfield
    case kColorId_TextfieldDefaultColor:
      return GetTextColor(GetEntry(), NORMAL);
    case kColorId_TextfieldDefaultBackground:
      return GetBaseColor(GetEntry(), NORMAL);
    case kColorId_TextfieldReadOnlyColor:
      return GetTextColor(GetEntry(), kTextboxInactiveState);
    case kColorId_TextfieldReadOnlyBackground:
      return GetBaseColor(GetEntry(), kTextboxInactiveState);
    case kColorId_TextfieldSelectionColor:
      return GetTextColor(GetEntry(), SELECTED);
    case kColorId_TextfieldSelectionBackgroundFocused:
      return GetBaseColor(GetEntry(), SELECTED);

    // Tooltips
    case kColorId_TooltipBackground:
      return GetBGColor(GetTooltip(), NORMAL);
    case kColorId_TooltipText:
      return GetFGColor(GetTooltip(), NORMAL);

    // Trees and Tables (implemented on GTK using the same class)
    case kColorId_TableBackground:
    case kColorId_TreeBackground:
      return GetBGColor(GetTree(), NORMAL);
    case kColorId_TableText:
    case kColorId_TreeText:
      return GetTextColor(GetTree(), NORMAL);
    case kColorId_TableSelectedText:
    case kColorId_TableSelectedTextUnfocused:
    case kColorId_TreeSelectedText:
    case kColorId_TreeSelectedTextUnfocused:
      return GetTextColor(GetTree(), SELECTED);
    case kColorId_TableSelectionBackgroundFocused:
    case kColorId_TableSelectionBackgroundUnfocused:
    case kColorId_TreeSelectionBackgroundFocused:
    case kColorId_TreeSelectionBackgroundUnfocused:
      return GetBGColor(GetTree(), SELECTED);
    case kColorId_TreeArrow:
      return GetFGColor(GetTree(), NORMAL);
    case kColorId_TableGroupingIndicatorColor:
      return GetTextAAColor(GetTree(), NORMAL);

      // Results Table
    case kColorId_ResultsTableNormalBackground:
      return GetBaseColor(GetEntry(), NORMAL);
    case kColorId_ResultsTableHoveredBackground:
      return color_utils::AlphaBlend(GetBaseColor(GetEntry(), NORMAL),
                                     GetBaseColor(GetEntry(), SELECTED),
                                     0x80);
    case kColorId_ResultsTableSelectedBackground:
      return GetBaseColor(GetEntry(), SELECTED);
    case kColorId_ResultsTableNormalText:
    case kColorId_ResultsTableHoveredText:
      return GetTextColor(GetEntry(), NORMAL);
    case kColorId_ResultsTableSelectedText:
      return GetTextColor(GetEntry(), SELECTED);
    case kColorId_ResultsTableNormalDimmedText:
    case kColorId_ResultsTableHoveredDimmedText:
      return color_utils::AlphaBlend(GetTextColor(GetEntry(), NORMAL),
                                     GetBaseColor(GetEntry(), NORMAL),
                                     0x80);
    case kColorId_ResultsTableSelectedDimmedText:
      return color_utils::AlphaBlend(GetTextColor(GetEntry(), SELECTED),
                                     GetBaseColor(GetEntry(), NORMAL),
                                     0x80);
    case kColorId_ResultsTableNormalUrl:
    case kColorId_ResultsTableHoveredUrl:
      return NormalURLColor(GetTextColor(GetEntry(), NORMAL));

    case kColorId_ResultsTableSelectedUrl:
      return SelectedURLColor(GetTextColor(GetEntry(), SELECTED),
                              GetBaseColor(GetEntry(), SELECTED));
    case kColorId_ResultsTableNormalDivider:
      return color_utils::AlphaBlend(GetTextColor(GetWindow(), NORMAL),
                                     GetBGColor(GetWindow(), NORMAL),
                                     0x34);
    case kColorId_ResultsTableHoveredDivider:
      return color_utils::AlphaBlend(GetTextColor(GetWindow(), PRELIGHT),
                                     GetBGColor(GetWindow(), PRELIGHT),
                                     0x34);
    case kColorId_ResultsTableSelectedDivider:
      return color_utils::AlphaBlend(GetTextColor(GetWindow(), SELECTED),
                                     GetBGColor(GetWindow(), SELECTED),
                                     0x34);

    case kColorId_ResultsTablePositiveText: {
      return color_utils::GetReadableColor(kPositiveTextColor,
                                           GetBaseColor(GetEntry(), NORMAL));
    }
    case kColorId_ResultsTablePositiveHoveredText: {
      return color_utils::GetReadableColor(kPositiveTextColor,
                                           GetBaseColor(GetEntry(), PRELIGHT));
    }
    case kColorId_ResultsTablePositiveSelectedText: {
      return color_utils::GetReadableColor(kPositiveTextColor,
                                           GetBaseColor(GetEntry(), SELECTED));
    }
    case kColorId_ResultsTableNegativeText: {
      return color_utils::GetReadableColor(kNegativeTextColor,
                                           GetBaseColor(GetEntry(), NORMAL));
    }
    case kColorId_ResultsTableNegativeHoveredText: {
      return color_utils::GetReadableColor(kNegativeTextColor,
                                           GetBaseColor(GetEntry(), PRELIGHT));
    }
    case kColorId_ResultsTableNegativeSelectedText: {
      return color_utils::GetReadableColor(kNegativeTextColor,
                                           GetBaseColor(GetEntry(), SELECTED));
    }

    // Throbber
    case kColorId_ThrobberSpinningColor:
    case kColorId_ThrobberLightColor: {
      return GetBGColor(GetEntry(), SELECTED);
    }

    case kColorId_ThrobberWaitingColor: {
      return color_utils::AlphaBlend(GetBGColor(GetEntry(), SELECTED),
                                     GetBGColor(GetWindow(), NORMAL),
                                     0x80);
    }

    case kColorId_NumColors:
      NOTREACHED();
      break;
  }

  return kInvalidColorIdColor;
}

GtkWidget* NativeThemeGtk2::GetWindow() const {
  static GtkWidget* fake_window = NULL;

  if (!fake_window) {
    fake_window = chrome_gtk_frame_new();
    gtk_widget_realize(fake_window);
  }

  return fake_window;
}

GtkWidget* NativeThemeGtk2::GetEntry() const {
  static GtkWidget* fake_entry = NULL;

  if (!fake_entry) {
    fake_entry = gtk_entry_new();

    // The fake entry needs to be in the window so it can be realized so we can
    // use the computed parts of the style.
    gtk_container_add(GTK_CONTAINER(GetWindow()), fake_entry);
    gtk_widget_realize(fake_entry);
  }

  return fake_entry;
}

GtkWidget* NativeThemeGtk2::GetLabel() const {
  static GtkWidget* fake_label = NULL;

  if (!fake_label)
    fake_label = gtk_label_new("");

  return fake_label;
}

GtkWidget* NativeThemeGtk2::GetButton() const {
  static GtkWidget* fake_button = NULL;

  if (!fake_button)
    fake_button = gtk_button_new();

  return fake_button;
}

GtkWidget* NativeThemeGtk2::GetBlueButton() const {
  static GtkWidget* fake_bluebutton = NULL;

  if (!fake_bluebutton) {
    fake_bluebutton = gtk_button_new();
    TurnButtonBlue(fake_bluebutton);
  }

  return fake_bluebutton;
}

GtkWidget* NativeThemeGtk2::GetTree() const {
  static GtkWidget* fake_tree = NULL;

  if (!fake_tree)
    fake_tree = gtk_tree_view_new();

  return fake_tree;
}

GtkWidget* NativeThemeGtk2::GetTooltip() const {
  static GtkWidget* fake_tooltip = NULL;

  if (!fake_tooltip) {
    fake_tooltip = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(fake_tooltip, "gtk-tooltip");
    gtk_widget_realize(fake_tooltip);
  }

  return fake_tooltip;
}

GtkWidget* NativeThemeGtk2::GetMenu() const {
  static GtkWidget* fake_menu = NULL;

  if (!fake_menu)
    fake_menu = gtk_custom_menu_new();

  return fake_menu;
}

GtkWidget* NativeThemeGtk2::GetMenuItem() const {
  static GtkWidget* fake_menu_item = NULL;

  if (!fake_menu_item) {
    fake_menu_item = gtk_custom_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(GetMenu()), fake_menu_item);
  }

  return fake_menu_item;
}

}  // namespace libgtk2ui
