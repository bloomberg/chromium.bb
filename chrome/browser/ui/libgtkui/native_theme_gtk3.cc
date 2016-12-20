// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/native_theme_gtk3.h"

#include <gtk/gtk.h>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/libgtkui/chrome_gtk_frame.h"
#include "chrome/browser/ui/libgtkui/chrome_gtk_menu_subclasses.h"
#include "chrome/browser/ui/libgtkui/gtk_util.h"
#include "chrome/browser/ui/libgtkui/skia_utils_gtk.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
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

// The caller must g_object_unref the returned context.
GtkStyleContext* GetStyleContextFromCss(const char* css_selector) {
  GtkWidgetPath* path = gtk_widget_path_new();
  for (const auto& widget_type :
       base::SplitString(css_selector, base::kWhitespaceASCII,
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    gtk_widget_path_append_type(path, G_TYPE_NONE);
    for (const auto& widget_class :
         base::SplitString(widget_type, ".", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY)) {
      gtk_widget_path_iter_add_class(path, -1, widget_class.c_str());
    }
  }

  GtkStyleContext* context = gtk_style_context_new();
  gtk_style_context_set_path(context, path);
  gtk_widget_path_unref(path);
  return context;
}

SkColor GdkRgbaToSkColor(const GdkRGBA& color) {
  return SkColorSetARGB(color.alpha * 255, color.red * 255, color.green * 255,
                        color.blue * 255);
}

SkColor ColorFromContext(GtkStyleContext* context,
                         GtkStateFlags state,
                         const char* color_name) {
  GdkRGBA* color = nullptr;
  gtk_style_context_get(context, state, color_name, &color, nullptr);
  DCHECK(color);
  SkColor sk_color = GdkRgbaToSkColor(*color);
  gdk_rgba_free(color);
  return sk_color;
}

SkColor GetFGColor(GtkWidget* widget, WidgetState state) {
  return ColorFromContext(gtk_widget_get_style_context(widget), stateMap[state],
                          "color");
}

SkColor GetBGColor(GtkWidget* widget, WidgetState state) {
  return ColorFromContext(gtk_widget_get_style_context(widget), stateMap[state],
                          "background-color");
}

SkColor GetFGColor(const char* css_selector, GtkStateFlags state) {
  GtkStyleContext* context = GetStyleContextFromCss(css_selector);
  SkColor color = ColorFromContext(context, state, "color");
  g_object_unref(context);
  return color;
}

SkColor GetBGColor(const char* css_selector, GtkStateFlags state) {
  GtkStyleContext* context = GetStyleContextFromCss(css_selector);
  SkColor color = ColorFromContext(context, state, "background-color");
  g_object_unref(context);
  return color;
}

SkColor GetBorderColor(const char* css_selector, GtkStateFlags state) {
  GtkStyleContext* context = GetStyleContextFromCss(css_selector);
  GtkBorder border;
  gtk_style_context_get_border(context, state, &border);
  bool has_border = border.left || border.right || border.top || border.bottom;
  SkColor color = ColorFromContext(
      context, state, has_border ? "border-color" : "background-color");
  g_object_unref(context);
  return color;
}

void PaintWidget(SkCanvas* canvas,
                 const gfx::Rect& rect,
                 const char* css_selector,
                 GtkStateFlags state) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(rect.width(), rect.height());
  bitmap.eraseColor(0);

  cairo_surface_t* surface = cairo_image_surface_create_for_data(
      static_cast<unsigned char*>(bitmap.getAddr(0, 0)), CAIRO_FORMAT_ARGB32,
      rect.width(), rect.height(),
      cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, rect.width()));
  cairo_t* cr = cairo_create(surface);

  GtkStyleContext* context = GetStyleContextFromCss(css_selector);
  gtk_style_context_set_state(context, state);

  gtk_render_background(context, cr, 0, 0, rect.width(), rect.height());
  gtk_render_frame(context, cr, 0, 0, rect.width(), rect.height());
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  canvas->drawBitmap(bitmap, rect.x(), rect.y());

  g_object_unref(context);
}

GtkStateFlags StateToStateFlags(NativeThemeGtk3::State state) {
  switch (state) {
    case NativeThemeGtk3::kDisabled:
      return GTK_STATE_FLAG_INSENSITIVE;
    case NativeThemeGtk3::kHovered:
      return GTK_STATE_FLAG_PRELIGHT;
    case NativeThemeGtk3::kNormal:
      return GTK_STATE_FLAG_NORMAL;
    case NativeThemeGtk3::kPressed:
      return static_cast<GtkStateFlags>(GTK_STATE_FLAG_PRELIGHT |
                                        GTK_STATE_FLAG_ACTIVE);
    default:
      NOTREACHED();
      return GTK_STATE_FLAG_NORMAL;
  }
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

SkColor NativeThemeGtk3::LookupGtkThemeColor(ColorId color_id) const {
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
      return GetFGColor(GetEntry(), NORMAL);

    // Menu
    case kColorId_MenuBackgroundColor:
      return GetBGColor("menu", GTK_STATE_FLAG_NORMAL);
    case kColorId_MenuBorderColor:
      return GetBorderColor("menu", GTK_STATE_FLAG_NORMAL);
    case kColorId_FocusedMenuItemBackgroundColor:
      return GetBGColor("menu menuitem", GTK_STATE_FLAG_FOCUSED);
    case kColorId_EnabledMenuItemForegroundColor:
      return GetFGColor("menu menuitem label", GTK_STATE_FLAG_NORMAL);
    case kColorId_SelectedMenuItemForegroundColor:
      return GetFGColor("menu menuitem label", GTK_STATE_FLAG_SELECTED);
    case kColorId_DisabledMenuItemForegroundColor:
      return GetFGColor("menu menuitem label", GTK_STATE_FLAG_INSENSITIVE);
    case kColorId_MenuItemSubtitleColor:
      return GetFGColor("menu menuitem accelerator", GTK_STATE_FLAG_NORMAL);
    case kColorId_MenuSeparatorColor:
    // MenuButton borders are used the same way as menu separtors in Chrome.
    case kColorId_EnabledMenuButtonBorderColor:
    case kColorId_FocusedMenuButtonBorderColor:
    case kColorId_HoverMenuButtonBorderColor:
      return GetFGColor("menu menuitem.separator", GTK_STATE_FLAG_INSENSITIVE);

    // Label
    case kColorId_LabelEnabledColor:
      return GetFGColor(GetEntry(), NORMAL);
    case kColorId_LabelDisabledColor:
      return GetFGColor(GetLabel(), INSENSITIVE);
    case kColorId_LabelTextSelectionColor:
      return GetFGColor(GetLabel(), SELECTED);
    case kColorId_LabelTextSelectionBackgroundFocused:
      return GetBGColor(GetLabel(), SELECTED);

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
      return GetFGColor(GetButton(), NORMAL);
    case kColorId_BlueButtonEnabledColor:
      return GetFGColor(GetBlueButton(), NORMAL);
    case kColorId_ButtonDisabledColor:
      return GetFGColor(GetButton(), INSENSITIVE);
    case kColorId_BlueButtonDisabledColor:
      return GetFGColor(GetBlueButton(), INSENSITIVE);
    case kColorId_ButtonHoverColor:
      return GetFGColor(GetButton(), PRELIGHT);
    case kColorId_BlueButtonHoverColor:
      return GetFGColor(GetBlueButton(), PRELIGHT);
    case kColorId_BlueButtonPressedColor:
      return GetFGColor(GetBlueButton(), ACTIVE);
    case kColorId_BlueButtonShadowColor:
      return SK_ColorTRANSPARENT;
    case kColorId_ProminentButtonColor:
      return GetSystemColor(kColorId_LinkEnabled);
    case kColorId_TextOnProminentButtonColor:
      return GetFGColor(GetLabel(), SELECTED);
    case kColorId_ButtonPressedShade:
      return SK_ColorTRANSPARENT;

    // Textfield
    case kColorId_TextfieldDefaultColor:
      return GetFGColor(GetEntry(), NORMAL);
    case kColorId_TextfieldDefaultBackground:
      return GetBGColor(GetEntry(), NORMAL);

    case kColorId_TextfieldReadOnlyColor:
      return GetFGColor(GetEntry(), SELECTED);
    case kColorId_TextfieldReadOnlyBackground:
      return GetBGColor(GetEntry(), SELECTED);
    case kColorId_TextfieldSelectionColor:
      return GetFGColor(GetLabel(), SELECTED);
    case kColorId_TextfieldSelectionBackgroundFocused:
      return GetBGColor(GetLabel(), SELECTED);

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
      return GetFGColor(GetTree(), NORMAL);
    case kColorId_TableSelectedText:
    case kColorId_TableSelectedTextUnfocused:
    case kColorId_TreeSelectedText:
    case kColorId_TreeSelectedTextUnfocused:
      return GetFGColor(GetTree(), SELECTED);
    case kColorId_TableSelectionBackgroundFocused:
    case kColorId_TableSelectionBackgroundUnfocused:
    case kColorId_TreeSelectionBackgroundFocused:
    case kColorId_TreeSelectionBackgroundUnfocused:
      return GetBGColor(GetTree(), SELECTED);
    case kColorId_TreeArrow:
      return GetFGColor(GetTree(), NORMAL);
    case kColorId_TableGroupingIndicatorColor:
      return GetFGColor(GetTree(), NORMAL);

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

    case kColorId_ResultsTablePositiveText:
      return color_utils::GetReadableColor(kPositiveTextColor,
                                           GetBGColor(GetEntry(), NORMAL));
    case kColorId_ResultsTablePositiveHoveredText:
      return color_utils::GetReadableColor(kPositiveTextColor,
                                           GetBGColor(GetEntry(), PRELIGHT));
    case kColorId_ResultsTablePositiveSelectedText:
      return color_utils::GetReadableColor(kPositiveTextColor,
                                           GetBGColor(GetEntry(), SELECTED));
    case kColorId_ResultsTableNegativeText:
      return color_utils::GetReadableColor(kNegativeTextColor,
                                           GetBGColor(GetEntry(), NORMAL));
    case kColorId_ResultsTableNegativeHoveredText:
      return color_utils::GetReadableColor(kNegativeTextColor,
                                           GetBGColor(GetEntry(), PRELIGHT));
    case kColorId_ResultsTableNegativeSelectedText:
      return color_utils::GetReadableColor(kNegativeTextColor,
                                           GetBGColor(GetEntry(), SELECTED));

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
    case kColorId_AlertSeverityHigh:
      return SK_ColorTRANSPARENT;

    case kColorId_NumColors:
      NOTREACHED();
      break;
  }

  return kInvalidColorIdColor;
}

SkColor NativeThemeGtk3::GetSystemColor(ColorId color_id) const {
  SkColor color = LookupGtkThemeColor(color_id);
  if (SkColorGetA(color))
    return color;
  gboolean prefer_dark_theme = FALSE;
  g_object_get(gtk_settings_get_default(), "gtk-application-prefer-dark-theme",
               &prefer_dark_theme, nullptr);
  ui::NativeTheme* fallback_theme =
      prefer_dark_theme ? ui::NativeThemeDarkAura::instance()
                        : ui::NativeTheme::GetInstanceForNativeUi();
  return fallback_theme->GetSystemColor(color_id);
}

void NativeThemeGtk3::PaintMenuPopupBackground(
    SkCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background) const {
  PaintWidget(canvas, gfx::Rect(size), "menu", GTK_STATE_FLAG_NORMAL);
}

void NativeThemeGtk3::PaintMenuItemBackground(
    SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item) const {
  PaintWidget(canvas, rect, "menu menuitem", StateToStateFlags(state));
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
