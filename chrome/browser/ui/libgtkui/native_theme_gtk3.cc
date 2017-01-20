// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/native_theme_gtk3.h"

#include <gtk/gtk.h>

#include "chrome/browser/ui/libgtkui/chrome_gtk_frame.h"
#include "chrome/browser/ui/libgtkui/chrome_gtk_menu_subclasses.h"
#include "chrome/browser/ui/libgtkui/gtk_util.h"
#include "chrome/browser/ui/libgtkui/skia_utils_gtk.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme_dark_aura.h"

namespace libgtkui {

namespace {

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

  auto context = GetStyleContextFromCss(css_selector);
  gtk_style_context_set_state(context, state);

  gtk_render_background(context, cr, 0, 0, rect.width(), rect.height());
  gtk_render_frame(context, cr, 0, 0, rect.width(), rect.height());
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  canvas->drawBitmap(bitmap, rect.x(), rect.y());
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

SkColor SkColorFromColorId(ui::NativeTheme::ColorId color_id) {
  const SkColor kPositiveTextColor = SkColorSetRGB(0x0b, 0x80, 0x43);
  const SkColor kNegativeTextColor = SkColorSetRGB(0xc5, 0x39, 0x29);

  switch (color_id) {
    // Windows
    case ui::NativeTheme::kColorId_WindowBackground:
    // Dialogs
    case ui::NativeTheme::kColorId_DialogBackground:
    case ui::NativeTheme::kColorId_BubbleBackground:
      return GetBgColor("");

    // FocusableBorder
    case ui::NativeTheme::kColorId_FocusedBorderColor:
      return GetBorderColor("GtkEntry#entry:focus");
    case ui::NativeTheme::kColorId_UnfocusedBorderColor:
      return GetBorderColor("GtkEntry#entry");

    // Menu
    case ui::NativeTheme::kColorId_MenuBackgroundColor:
      return GetBgColor("GtkMenu#menu");
    case ui::NativeTheme::kColorId_MenuBorderColor:
      return GetBorderColor("GtkMenu#menu");
    case ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor:
      return GetBgColor("GtkMenu#menu GtkMenuItem#menuitem:focus");
    case ui::NativeTheme::kColorId_EnabledMenuItemForegroundColor:
      return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem GtkLabel#label");
    case ui::NativeTheme::kColorId_SelectedMenuItemForegroundColor:
      return GetFgColor(
          "GtkMenu#menu GtkMenuItem#menuitem:selected GtkLabel#label");
    case ui::NativeTheme::kColorId_DisabledMenuItemForegroundColor:
      return GetFgColor(
          "GtkMenu#menu GtkMenuItem#menuitem:disabled GtkLabel#label");
    case ui::NativeTheme::kColorId_MenuItemSubtitleColor:
      return GetFgColor(
          "GtkMenu#menu GtkMenuItem#menuitem GtkLabel#label.accelerator");
    case ui::NativeTheme::kColorId_MenuSeparatorColor:
    // MenuButton borders are used the same way as menu separators in Chrome.
    case ui::NativeTheme::kColorId_EnabledMenuButtonBorderColor:
    case ui::NativeTheme::kColorId_FocusedMenuButtonBorderColor:
    case ui::NativeTheme::kColorId_HoverMenuButtonBorderColor:
      if (GtkVersionCheck(3, 20))
        return GetBgColor("GtkMenu#menu GtkSeparator#separator");
      else
        return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem.separator");

    // Label
    case ui::NativeTheme::kColorId_LabelEnabledColor:
      return GetFgColor("GtkLabel#label");
    case ui::NativeTheme::kColorId_LabelDisabledColor:
      return GetFgColor("GtkLabel#label:disabled");
    case ui::NativeTheme::kColorId_LabelTextSelectionColor:
      return GetFgColor("GtkLabel#label #selection:selected");
    case ui::NativeTheme::kColorId_LabelTextSelectionBackgroundFocused:
      return GetBgColor("GtkLabel#label #selection:selected");

    // Link
    case ui::NativeTheme::kColorId_LinkDisabled:
      return SkColorSetA(
          SkColorFromColorId(ui::NativeTheme::kColorId_LinkEnabled), 0xBB);
    case ui::NativeTheme::kColorId_LinkPressed:
      if (GtkVersionCheck(3, 12))
        return GetFgColor("GtkLabel#label.link:link:hover:active");
      // fallthrough
    case ui::NativeTheme::kColorId_LinkEnabled: {
      if (GtkVersionCheck(3, 12)) {
        return GetFgColor("GtkLabel#label.link:link");
      }
      auto link_context = GetStyleContextFromCss("GtkLabel#label.view");
      GdkColor* color;
      gtk_style_context_get_style(link_context, "link-color", &color, nullptr);
      if (color) {
        SkColor ret_color = SkColorSetRGB(color->red / 255, color->green / 255,
                                          color->blue / 255);
        // gdk_color_free() was deprecated in Gtk3.14.  This code path is only
        // taken on versions earlier than Gtk3.12, but the compiler doesn't know
        // that, so silence the deprecation warnings.
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        gdk_color_free(color);
        G_GNUC_END_IGNORE_DEPRECATIONS;
        return ret_color;
      }

      // Default color comes from gtklinkbutton.c.
      return SkColorSetRGB(0x00, 0x00, 0xEE);
    }

    // Separator
    case ui::NativeTheme::kColorId_SeparatorColor:
      return GetSeparatorColor("GtkSeparator#separator.horizontal");

    // Button
    case ui::NativeTheme::kColorId_ButtonEnabledColor:
      return GetFgColor("GtkButton#button.text-button GtkLabel#label");
    case ui::NativeTheme::kColorId_ButtonDisabledColor:
      return GetFgColor("GtkButton#button.text-button:disabled GtkLabel#label");
    case ui::NativeTheme::kColorId_ButtonHoverColor:
      return GetFgColor("GtkButton#button.text-button:hover GtkLabel#label");
    case ui::NativeTheme::kColorId_ButtonPressedShade:
      return SK_ColorTRANSPARENT;

    case ui::NativeTheme::kColorId_BlueButtonEnabledColor:
      return GetFgColor(
          "GtkButton#button.text-button.suggested-action GtkLabel#label");
    case ui::NativeTheme::kColorId_BlueButtonDisabledColor:
      return GetFgColor(
          "GtkButton#button.text-button.suggested-action:disabled "
          "GtkLabel#label");
    case ui::NativeTheme::kColorId_BlueButtonHoverColor:
      return GetFgColor(
          "GtkButton#button.text-button.suggested-action:hover GtkLabel#label");
    case ui::NativeTheme::kColorId_BlueButtonPressedColor:
      return GetFgColor(
          "GtkButton#button.text-button.suggested-action:hover:active "
          "GtkLabel#label");
    case ui::NativeTheme::kColorId_BlueButtonShadowColor:
      return SK_ColorTRANSPARENT;

    case ui::NativeTheme::kColorId_ProminentButtonColor:
      return GetBgColor("GtkButton#button.text-button.destructive-action");
    case ui::NativeTheme::kColorId_TextOnProminentButtonColor:
      return GetFgColor(
          "GtkButton#button.text-button.destructive-action GtkLabel#label");

    // Textfield
    case ui::NativeTheme::kColorId_TextfieldDefaultColor:
      return GetFgColor("GtkEntry#entry");
    case ui::NativeTheme::kColorId_TextfieldDefaultBackground:
      return GetBgColor("GtkEntry#entry");
    case ui::NativeTheme::kColorId_TextfieldReadOnlyColor:
      return GetFgColor("GtkEntry#entry:disabled");
    case ui::NativeTheme::kColorId_TextfieldReadOnlyBackground:
      return GetBgColor("GtkEntry#entry:disabled");
    case ui::NativeTheme::kColorId_TextfieldSelectionColor:
      return GetFgColor("GtkEntry#entry #selection:selected");
    case ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused:
      return GetBgColor("GtkEntry#entry #selection:selected");

    // Tooltips
    case ui::NativeTheme::kColorId_TooltipBackground:
      return GetBgColor("GtkTooltip#tooltip");
    case ui::NativeTheme::kColorId_TooltipText:
      return color_utils::GetReadableColor(GetFgColor("GtkTooltip#tooltip"),
                                           GetBgColor("GtkTooltip#tooltip"));

    // Trees and Tables (implemented on GTK using the same class)
    case ui::NativeTheme::kColorId_TableBackground:
    case ui::NativeTheme::kColorId_TreeBackground:
      return GetBgColor("GtkTreeView#treeview.view .view.cell");
    case ui::NativeTheme::kColorId_TableText:
    case ui::NativeTheme::kColorId_TreeText:
    case ui::NativeTheme::kColorId_TreeArrow:
    case ui::NativeTheme::kColorId_TableGroupingIndicatorColor:
      return GetFgColor("GtkTreeView#treeview.view .view.cell GtkLabel#label");
    case ui::NativeTheme::kColorId_TableSelectedText:
    case ui::NativeTheme::kColorId_TableSelectedTextUnfocused:
    case ui::NativeTheme::kColorId_TreeSelectedText:
    case ui::NativeTheme::kColorId_TreeSelectedTextUnfocused:
      return GetFgColor(
          "GtkTreeView#treeview.view .view.cell:selected:focus GtkLabel#label");
    case ui::NativeTheme::kColorId_TableSelectionBackgroundFocused:
    case ui::NativeTheme::kColorId_TableSelectionBackgroundUnfocused:
    case ui::NativeTheme::kColorId_TreeSelectionBackgroundFocused:
    case ui::NativeTheme::kColorId_TreeSelectionBackgroundUnfocused:
      return GetBgColor("GtkTreeView#treeview.view .view.cell:selected:focus");

    // Results Table
    // TODO(thomasanderson): The GtkEntry selectors was how the gtk2 theme got
    // these colors.  Update this code to use a different widget.
    case ui::NativeTheme::kColorId_ResultsTableNormalBackground:
      return GetBgColor("GtkEntry#entry");
    case ui::NativeTheme::kColorId_ResultsTableHoveredBackground:
      return color_utils::AlphaBlend(
          GetBgColor("GtkEntry#entry"),
          GetBgColor("GtkEntry#entry #selection:selected"), 0x80);
    case ui::NativeTheme::kColorId_ResultsTableSelectedBackground:
      return GetBgColor("GtkEntry#entry #selection:selected");
    case ui::NativeTheme::kColorId_ResultsTableNormalText:
    case ui::NativeTheme::kColorId_ResultsTableHoveredText:
      return GetFgColor("GtkEntry#entry");
    case ui::NativeTheme::kColorId_ResultsTableSelectedText:
      return GetFgColor("GtkEntry#entry #selection:selected");
    case ui::NativeTheme::kColorId_ResultsTableNormalDimmedText:
    case ui::NativeTheme::kColorId_ResultsTableHoveredDimmedText:
      return color_utils::AlphaBlend(GetFgColor("GtkEntry#entry"),
                                     GetBgColor("GtkEntry#entry"), 0x80);
    case ui::NativeTheme::kColorId_ResultsTableSelectedDimmedText:
      return color_utils::AlphaBlend(
          GetFgColor("GtkEntry#entry #selection:selected"),
          GetBgColor("GtkEntry#entry"), 0x80);
    case ui::NativeTheme::kColorId_ResultsTableNormalUrl:
    case ui::NativeTheme::kColorId_ResultsTableHoveredUrl:
      return NormalURLColor(GetFgColor("GtkEntry#entry"));
    case ui::NativeTheme::kColorId_ResultsTableSelectedUrl:
      return SelectedURLColor(GetFgColor("GtkEntry#entry #selection:selected"),
                              GetBgColor("GtkEntry#entry #selection:selected"));

    case ui::NativeTheme::kColorId_ResultsTablePositiveText:
      return color_utils::GetReadableColor(kPositiveTextColor,
                                           GetBgColor("GtkEntry#entry"));
    case ui::NativeTheme::kColorId_ResultsTablePositiveHoveredText:
      return color_utils::GetReadableColor(kPositiveTextColor,
                                           GetBgColor("GtkEntry#entry:hover"));
    case ui::NativeTheme::kColorId_ResultsTablePositiveSelectedText:
      return color_utils::GetReadableColor(
          kPositiveTextColor, GetBgColor("GtkEntry#entry:selected"));
    case ui::NativeTheme::kColorId_ResultsTableNegativeText:
      return color_utils::GetReadableColor(kNegativeTextColor,
                                           GetBgColor("GtkEntry#entry"));
    case ui::NativeTheme::kColorId_ResultsTableNegativeHoveredText:
      return color_utils::GetReadableColor(kNegativeTextColor,
                                           GetBgColor("GtkEntry#entry:hover"));
    case ui::NativeTheme::kColorId_ResultsTableNegativeSelectedText:
      return color_utils::GetReadableColor(
          kNegativeTextColor, GetBgColor("GtkEntry#entry:selected"));

    // Throbber
    // TODO(thomasanderson): Render GtkSpinner directly.
    case ui::NativeTheme::kColorId_ThrobberSpinningColor:
    case ui::NativeTheme::kColorId_ThrobberWaitingColor:
      return GetFgColor("GtkMenu#menu GtkSpinner#spinner");
    case ui::NativeTheme::kColorId_ThrobberLightColor:
      return GetFgColor("GtkMenu#menu GtkSpinner#spinner:disabled");

    // Alert icons
    case ui::NativeTheme::kColorId_AlertSeverityLow:
      return GetBgColor("GtkInfoBar#infobar.info");
    case ui::NativeTheme::kColorId_AlertSeverityMedium:
      return GetBgColor("GtkInfoBar#infobar.warning");
    case ui::NativeTheme::kColorId_AlertSeverityHigh:
      return GetBgColor("GtkInfoBar#infobar.error");

    case ui::NativeTheme::kColorId_NumColors:
      NOTREACHED();
      break;
  }
  return kInvalidColorIdColor;
}

void OnThemeChanged(GObject* obj, GParamSpec* param, NativeThemeGtk3* theme) {
  theme->ResetColorCache();
}

}  // namespace

// static
NativeThemeGtk3* NativeThemeGtk3::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeGtk3, s_native_theme, ());
  return &s_native_theme;
}

// Constructors automatically called
NativeThemeGtk3::NativeThemeGtk3() {
  // These types are needed by g_type_from_name(), but may not be registered at
  // this point.  We need the g_type_class magic to make sure the compiler
  // doesn't optimize away this code.
  g_type_class_unref(g_type_class_ref(gtk_button_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_label_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_window_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_link_button_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_spinner_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_menu_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_menu_item_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_entry_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_info_bar_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_tooltip_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_scrollbar_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_toolbar_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_text_view_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_separator_get_type()));

  g_signal_connect_after(gtk_settings_get_default(), "notify::gtk-theme-name",
                         G_CALLBACK(OnThemeChanged), this);
}

// This doesn't actually get called
NativeThemeGtk3::~NativeThemeGtk3() {}

void NativeThemeGtk3::ResetColorCache() {
  for (auto& color : color_cache_)
    color = base::nullopt;
}

SkColor NativeThemeGtk3::GetSystemColor(ColorId color_id) const {
  if (color_cache_[color_id])
    return color_cache_[color_id].value();

  SkColor color = SkColorFromColorId(color_id);
  color_cache_[color_id] = color;
  return color;
}

void NativeThemeGtk3::PaintMenuPopupBackground(
    SkCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background) const {
  PaintWidget(canvas, gfx::Rect(size), "GtkMenu#menu", GTK_STATE_FLAG_NORMAL);
}

void NativeThemeGtk3::PaintMenuItemBackground(
    SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item) const {
  PaintWidget(canvas, rect, "GtkMenu#menu GtkMenuItem#menuitem",
              StateToStateFlags(state));
}

}  // namespace libgtkui
