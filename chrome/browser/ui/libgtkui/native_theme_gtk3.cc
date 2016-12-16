// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/native_theme_gtk3.h"

#include <gtk/gtk.h>

#include "chrome/browser/ui/libgtkui/chrome_gtk_frame.h"
#include "chrome/browser/ui/libgtkui/chrome_gtk_menu_subclasses.h"
#include "chrome/browser/ui/libgtkui/gtk_util.h"
#include "chrome/browser/ui/libgtkui/skia_utils_gtk.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme_dark_aura.h"

namespace libgtkui {

namespace {

enum WidgetState {
  NORMAL = 0,
  ACTIVE = 1,
  PRELIGHT = 2,
  SELECTED = 3,
  INSENSITIVE = 4,
};

// Same order as enum WidgetState above
const GtkStateFlags stateMap[] = {
    GTK_STATE_FLAG_NORMAL,      GTK_STATE_FLAG_ACTIVE,
    GTK_STATE_FLAG_PRELIGHT,    GTK_STATE_FLAG_SELECTED,
    GTK_STATE_FLAG_INSENSITIVE,
};

SkColor GetFGColor(GtkWidget* widget, WidgetState state) {
  GdkRGBA color;
  gtk_style_context_get_color(gtk_widget_get_style_context(widget),
                              stateMap[state], &color);
  return SkColorSetRGB(color.red * 255, color.green * 255, color.blue * 255);
}
SkColor GetBGColor(GtkWidget* widget, WidgetState state) {
  GdkRGBA color;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_style_context_get_background_color(gtk_widget_get_style_context(widget),
                                         stateMap[state], &color);
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

}  // namespace

// static
NativeThemeGtk3* NativeThemeGtk3::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeGtk3, s_native_theme, ());
  return &s_native_theme;
}

// Constructors automatically called
NativeThemeGtk3::NativeThemeGtk3() {}
// This doesn't actually get called
NativeThemeGtk3::~NativeThemeGtk3() {}

SkColor NativeThemeGtk3::GetSystemColor(ColorId color_id) const {
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
    case kColorId_SelectedMenuItemForegroundColor:
      return GetTextColor(GetMenuItem(), SELECTED);
    case kColorId_FocusedMenuItemBackgroundColor:
      return GetBGColor(GetMenuItem(), SELECTED);

    case kColorId_EnabledMenuItemForegroundColor:
      return GetTextColor(GetMenuItem(), NORMAL);
    case kColorId_DisabledMenuItemForegroundColor:
      return GetTextColor(GetMenuItem(), INSENSITIVE);
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
    case kColorId_LabelTextSelectionColor:
      return GetTextColor(GetLabel(), SELECTED);
    case kColorId_LabelTextSelectionBackgroundFocused:
      return GetBaseColor(GetLabel(), SELECTED);

    // Link
    case kColorId_LinkDisabled:
      return SkColorSetA(GetSystemColor(kColorId_LinkEnabled), 0xBB);
    case kColorId_LinkEnabled: {
      SkColor link_color = SK_ColorTRANSPARENT;
      GdkColor* style_color = nullptr;
      gtk_widget_style_get(GetWindow(), "link-color", &style_color, nullptr);
      if (style_color) {
        link_color = GdkColorToSkColor(*style_color);
        gdk_color_free(style_color);
      }
      if (link_color != SK_ColorTRANSPARENT)
        return link_color;
      // Default color comes from gtklinkbutton.c.
      return SkColorSetRGB(0x00, 0x00, 0xEE);
    }
    case kColorId_LinkPressed:
      return SK_ColorRED;

    // Button
    case kColorId_ButtonEnabledColor:
      return GetTextColor(GetButton(), NORMAL);
    case kColorId_BlueButtonEnabledColor:
      return GetTextColor(GetBlueButton(), NORMAL);
    case kColorId_ButtonDisabledColor:
      return GetTextColor(GetButton(), INSENSITIVE);
    case kColorId_BlueButtonDisabledColor:
      return GetTextColor(GetBlueButton(), INSENSITIVE);
    case kColorId_ButtonHoverColor:
      return GetTextColor(GetButton(), PRELIGHT);
    case kColorId_BlueButtonHoverColor:
      return GetTextColor(GetBlueButton(), PRELIGHT);
    case kColorId_BlueButtonPressedColor:
      return GetTextColor(GetBlueButton(), ACTIVE);
    case kColorId_BlueButtonShadowColor:
      return SK_ColorTRANSPARENT;
    case kColorId_ProminentButtonColor:
      return GetSystemColor(kColorId_LinkEnabled);
    case kColorId_TextOnProminentButtonColor:
      return GetTextColor(GetLabel(), SELECTED);
    case kColorId_ButtonPressedShade:
      return SK_ColorTRANSPARENT;

    // Textfield
    case kColorId_TextfieldDefaultColor:
      return GetTextColor(GetEntry(), NORMAL);
    case kColorId_TextfieldDefaultBackground:
      return GetBaseColor(GetEntry(), NORMAL);

    case kColorId_TextfieldReadOnlyColor:
      return GetTextColor(GetEntry(), SELECTED);
    case kColorId_TextfieldReadOnlyBackground:
      return GetBaseColor(GetEntry(), SELECTED);
    case kColorId_TextfieldSelectionColor:
      return GetTextColor(GetLabel(), SELECTED);
    case kColorId_TextfieldSelectionBackgroundFocused:
      return GetBaseColor(GetLabel(), SELECTED);

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
      return GetSystemColor(kColorId_TextfieldDefaultBackground);
    case kColorId_ResultsTableHoveredBackground:
      return color_utils::AlphaBlend(
          GetSystemColor(kColorId_TextfieldDefaultBackground),
          GetSystemColor(kColorId_TextfieldSelectionBackgroundFocused), 0x80);
    case kColorId_ResultsTableSelectedBackground:
      return GetSystemColor(kColorId_TextfieldSelectionBackgroundFocused);
    case kColorId_ResultsTableNormalText:
    case kColorId_ResultsTableHoveredText:
      return GetSystemColor(kColorId_TextfieldDefaultColor);
    case kColorId_ResultsTableSelectedText:
      return GetSystemColor(kColorId_TextfieldSelectionColor);
    case kColorId_ResultsTableNormalDimmedText:
    case kColorId_ResultsTableHoveredDimmedText:
      return color_utils::AlphaBlend(
          GetSystemColor(kColorId_TextfieldDefaultColor),
          GetSystemColor(kColorId_TextfieldDefaultBackground), 0x80);
    case kColorId_ResultsTableSelectedDimmedText:
      return color_utils::AlphaBlend(
          GetSystemColor(kColorId_TextfieldSelectionColor),
          GetSystemColor(kColorId_TextfieldDefaultBackground), 0x80);
    case kColorId_ResultsTableNormalUrl:
    case kColorId_ResultsTableHoveredUrl:
      return NormalURLColor(GetSystemColor(kColorId_TextfieldDefaultColor));

    case kColorId_ResultsTableSelectedUrl:
      return SelectedURLColor(
          GetSystemColor(kColorId_TextfieldSelectionColor),
          GetSystemColor(kColorId_TextfieldSelectionBackgroundFocused));

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
    case kColorId_ThrobberLightColor:
      return GetSystemColor(kColorId_TextfieldSelectionBackgroundFocused);

    case kColorId_ThrobberWaitingColor:
      return color_utils::AlphaBlend(
          GetSystemColor(kColorId_TextfieldSelectionBackgroundFocused),
          GetBGColor(GetWindow(), NORMAL), 0x80);

    // Alert icons
    // Just fall back to the same colors as Aura.
    case kColorId_AlertSeverityLow:
    case kColorId_AlertSeverityMedium:
    case kColorId_AlertSeverityHigh: {
      ui::NativeTheme* fallback_theme =
          color_utils::IsDark(GetTextColor(GetEntry(), NORMAL))
              ? ui::NativeTheme::GetInstanceForNativeUi()
              : ui::NativeThemeDarkAura::instance();
      return fallback_theme->GetSystemColor(color_id);
    }

    case kColorId_NumColors:
      NOTREACHED();
      break;
  }

  return kInvalidColorIdColor;
}

GtkWidget* NativeThemeGtk3::GetWindow() const {
  static GtkWidget* fake_window = NULL;

  if (!fake_window) {
    fake_window = chrome_gtk_frame_new();
    gtk_widget_realize(fake_window);
  }

  return fake_window;
}

GtkWidget* NativeThemeGtk3::GetEntry() const {
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

GtkWidget* NativeThemeGtk3::GetLabel() const {
  static GtkWidget* fake_label = NULL;

  if (!fake_label)
    fake_label = gtk_label_new("");

  return fake_label;
}

GtkWidget* NativeThemeGtk3::GetButton() const {
  static GtkWidget* fake_button = NULL;

  if (!fake_button)
    fake_button = gtk_button_new();

  return fake_button;
}

GtkWidget* NativeThemeGtk3::GetBlueButton() const {
  static GtkWidget* fake_bluebutton = NULL;

  if (!fake_bluebutton) {
    fake_bluebutton = gtk_button_new();
    TurnButtonBlue(fake_bluebutton);
  }

  return fake_bluebutton;
}

GtkWidget* NativeThemeGtk3::GetTree() const {
  static GtkWidget* fake_tree = NULL;

  if (!fake_tree)
    fake_tree = gtk_tree_view_new();

  return fake_tree;
}

GtkWidget* NativeThemeGtk3::GetTooltip() const {
  static GtkWidget* fake_tooltip = NULL;

  if (!fake_tooltip) {
    fake_tooltip = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(fake_tooltip, "gtk-tooltip");
    gtk_widget_realize(fake_tooltip);
  }

  return fake_tooltip;
}

GtkWidget* NativeThemeGtk3::GetMenu() const {
  static GtkWidget* fake_menu = NULL;

  if (!fake_menu)
    fake_menu = gtk_custom_menu_new();

  return fake_menu;
}

GtkWidget* NativeThemeGtk3::GetMenuItem() const {
  static GtkWidget* fake_menu_item = NULL;

  if (!fake_menu_item) {
    fake_menu_item = gtk_custom_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(GetMenu()), fake_menu_item);
  }

  return fake_menu_item;
}

}  // namespace libgtkui
