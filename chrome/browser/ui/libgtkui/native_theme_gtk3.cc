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
#include "ui/gfx/skbitmap_operations.h"
#include "ui/native_theme/native_theme_dark_aura.h"

namespace libgtkui {

namespace {

enum BackgroundRenderMode {
  BG_RENDER_NORMAL,
  BG_RENDER_NONE,
  BG_RENDER_RECURSIVE,
};

SkBitmap GetWidgetBitmap(const gfx::Size& size,
                         GtkStyleContext* context,
                         BackgroundRenderMode bg_mode,
                         bool render_frame) {
  DCHECK(bg_mode != BG_RENDER_NONE || render_frame);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width(), size.height());
  bitmap.eraseColor(0);

  CairoSurface surface(bitmap);
  cairo_t* cr = surface.cairo();

  switch (bg_mode) {
    case BG_RENDER_NORMAL:
      gtk_render_background(context, cr, 0, 0, size.width(), size.height());
      break;
    case BG_RENDER_RECURSIVE:
      RenderBackground(size, cr, context);
      break;
    case BG_RENDER_NONE:
      break;
  }
  if (render_frame)
    gtk_render_frame(context, cr, 0, 0, size.width(), size.height());
  return bitmap;
}

void PaintWidget(cc::PaintCanvas* canvas,
                 const gfx::Rect& rect,
                 GtkStyleContext* context,
                 BackgroundRenderMode bg_mode,
                 bool render_frame) {
  canvas->drawBitmap(
      GetWidgetBitmap(rect.size(), context, bg_mode, render_frame), rect.x(),
      rect.y());
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
      // GetBorderColor("GtkEntry#entry:focus") is correct here.  The focus ring
      // around widgets is usually a lighter version of the "canonical theme
      // color" - orange on Ambiance, blue on Adwaita, etc.  However, Chrome
      // lightens the color we give it, so it would look wrong if we give it an
      // already-lightened color.  This workaround returns the theme color
      // directly, taken from a selected table row.  This has matched the theme
      // color on every theme that I've tested.
      return GetBgColor(
          "GtkTreeView#treeview.view "
          "GtkTreeView#treeview.view.cell:selected:focus");
    case ui::NativeTheme::kColorId_UnfocusedBorderColor:
      return GetBorderColor("GtkEntry#entry");

    // Menu
    case ui::NativeTheme::kColorId_MenuBackgroundColor:
      return GetBgColor("GtkMenu#menu");
    case ui::NativeTheme::kColorId_MenuBorderColor:
      return GetBorderColor("GtkMenu#menu");
    case ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor:
      return GetBgColor("GtkMenu#menu GtkMenuItem#menuitem:hover");
    case ui::NativeTheme::kColorId_EnabledMenuItemForegroundColor:
      return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem GtkLabel");
    case ui::NativeTheme::kColorId_SelectedMenuItemForegroundColor:
      return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem:hover GtkLabel");
    case ui::NativeTheme::kColorId_DisabledMenuItemForegroundColor:
      return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem:disabled GtkLabel");
    case ui::NativeTheme::kColorId_MenuItemSubtitleColor:
      if (GtkVersionCheck(3, 20)) {
        return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem #accelerator");
      }
      return GetFgColor(
          "GtkMenu#menu GtkMenuItem#menuitem GtkLabel.accelerator");
    case ui::NativeTheme::kColorId_MenuSeparatorColor:
    // MenuButton borders are used as vertical menu separators in Chrome.
    case ui::NativeTheme::kColorId_EnabledMenuButtonBorderColor:
    case ui::NativeTheme::kColorId_FocusedMenuButtonBorderColor:
    case ui::NativeTheme::kColorId_HoverMenuButtonBorderColor:
      if (GtkVersionCheck(3, 20)) {
        return GetSeparatorColor(
            "GtkMenu#menu GtkSeparator#separator.horizontal");
      }
      return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem.separator");

    // Label
    case ui::NativeTheme::kColorId_LabelEnabledColor:
      return GetFgColor("GtkLabel");
    case ui::NativeTheme::kColorId_LabelDisabledColor:
      return GetFgColor("GtkLabel:disabled");
    case ui::NativeTheme::kColorId_LabelTextSelectionColor:
      return GetFgColor(GtkVersionCheck(3, 20) ? "GtkLabel #selection"
                                               : "GtkLabel:selected");
    case ui::NativeTheme::kColorId_LabelTextSelectionBackgroundFocused:
      return GetSelectionBgColor(GtkVersionCheck(3, 20) ? "GtkLabel #selection"
                                                        : "GtkLabel:selected");

    // Link
    case ui::NativeTheme::kColorId_LinkDisabled:
      return SkColorSetA(
          SkColorFromColorId(ui::NativeTheme::kColorId_LinkEnabled), 0xBB);
    case ui::NativeTheme::kColorId_LinkPressed:
      if (GtkVersionCheck(3, 12))
        return GetFgColor("GtkLabel.link:link:hover:active");
    // fallthrough
    case ui::NativeTheme::kColorId_LinkEnabled: {
      if (GtkVersionCheck(3, 12)) {
        return GetFgColor("GtkLabel.link:link");
      }
      auto link_context = GetStyleContextFromCss("GtkLabel.view");
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
      return GetFgColor("GtkButton#button.text-button GtkLabel");
    case ui::NativeTheme::kColorId_ButtonDisabledColor:
      return GetFgColor("GtkButton#button.text-button:disabled GtkLabel");
    case ui::NativeTheme::kColorId_ButtonHoverColor:
      return GetFgColor("GtkButton#button.text-button:hover GtkLabel");
    case ui::NativeTheme::kColorId_ButtonPressedShade:
      return SK_ColorTRANSPARENT;

    // BlueButton
    case ui::NativeTheme::kColorId_BlueButtonEnabledColor:
      return GetFgColor(
          "GtkButton#button.text-button.default.suggested-action GtkLabel");
    case ui::NativeTheme::kColorId_BlueButtonDisabledColor:
      return GetFgColor(
          "GtkButton#button.text-button.default.suggested-action:disabled "
          "GtkLabel");
    case ui::NativeTheme::kColorId_BlueButtonHoverColor:
      return GetFgColor(
          "GtkButton#button.text-button.default.suggested-action:hover "
          "GtkLabel");
    case ui::NativeTheme::kColorId_BlueButtonPressedColor:
      return GetFgColor(
          "GtkButton#button.text-button.default.suggested-action:hover:active "
          "GtkLabel");
    case ui::NativeTheme::kColorId_BlueButtonShadowColor:
      return SK_ColorTRANSPARENT;

    // ProminentButton
    case ui::NativeTheme::kColorId_ProminentButtonColor:
      return GetBgColor(
          "GtkTreeView#treeview.view "
          "GtkTreeView#treeview.view.cell:selected:focus");
    case ui::NativeTheme::kColorId_TextOnProminentButtonColor:
      return GetFgColor(
          "GtkTreeView#treeview.view "
          "GtkTreeview#treeview.view.cell:selected:focus GtkLabel");

    // Textfield
    case ui::NativeTheme::kColorId_TextfieldDefaultColor:
      return GetFgColor(GtkVersionCheck(3, 20)
                            ? "GtkTextView#textview.view #text"
                            : "GtkTextView");
    case ui::NativeTheme::kColorId_TextfieldDefaultBackground:
      return GetBgColor(GtkVersionCheck(3, 20) ? "GtkTextView#textview.view"
                                               : "GtkTextView");
    case ui::NativeTheme::kColorId_TextfieldReadOnlyColor:
      return GetFgColor(GtkVersionCheck(3, 20)
                            ? "GtkTextView#textview.view:disabled #text"
                            : "GtkTextView:disabled");
    case ui::NativeTheme::kColorId_TextfieldReadOnlyBackground:
      return GetBgColor(GtkVersionCheck(3, 20)
                            ? "GtkTextView#textview.view:disabled"
                            : "GtkTextView:disabled");
    case ui::NativeTheme::kColorId_TextfieldSelectionColor:
      return GetFgColor(GtkVersionCheck(3, 20)
                            ? "GtkTextView#textview.view #text #selection"
                            : "GtkTextView:selected");
    case ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused:
      return GetSelectionBgColor(
          GtkVersionCheck(3, 20) ? "GtkTextView#textview.view #text #selection"
                                 : "GtkTextView:selected");

    // Tooltips
    case ui::NativeTheme::kColorId_TooltipBackground:
      return GetBgColor("GtkTooltip#tooltip");
    case ui::NativeTheme::kColorId_TooltipText:
      return color_utils::GetReadableColor(GetFgColor("GtkTooltip#tooltip"),
                                           GetBgColor("GtkTooltip#tooltip"));

    // Trees and Tables (implemented on GTK using the same class)
    case ui::NativeTheme::kColorId_TableBackground:
    case ui::NativeTheme::kColorId_TreeBackground:
      return GetBgColor(
          "GtkTreeView#treeview.view GtkTreeView#treeview.view.cell");
    case ui::NativeTheme::kColorId_TableText:
    case ui::NativeTheme::kColorId_TreeText:
    case ui::NativeTheme::kColorId_TableGroupingIndicatorColor:
      return GetFgColor(
          "GtkTreeView#treeview.view GtkTreeView#treeview.view.cell GtkLabel");
    case ui::NativeTheme::kColorId_TableSelectedText:
    case ui::NativeTheme::kColorId_TableSelectedTextUnfocused:
    case ui::NativeTheme::kColorId_TreeSelectedText:
    case ui::NativeTheme::kColorId_TreeSelectedTextUnfocused:
      return GetFgColor(
          "GtkTreeView#treeview.view "
          "GtkTreeView#treeview.view.cell:selected:focus GtkLabel");
    case ui::NativeTheme::kColorId_TableSelectionBackgroundFocused:
    case ui::NativeTheme::kColorId_TableSelectionBackgroundUnfocused:
    case ui::NativeTheme::kColorId_TreeSelectionBackgroundFocused:
    case ui::NativeTheme::kColorId_TreeSelectionBackgroundUnfocused:
      return GetBgColor(
          "GtkTreeView#treeview.view "
          "GtkTreeView#treeview.view.cell:selected:focus");

    // Table Header
    case ui::NativeTheme::kColorId_TableHeaderText:
      return GetFgColor("GtkTreeView#treeview.view GtkButton#button GtkLabel");
    case ui::NativeTheme::kColorId_TableHeaderBackground:
      return GetBgColor("GtkTreeView#treeview.view GtkButton#button");
    case ui::NativeTheme::kColorId_TableHeaderSeparator:
      return GetBorderColor("GtkTreeView#treeview.view GtkButton#button");

    // Results Table
    case ui::NativeTheme::kColorId_ResultsTableNormalBackground:
      return SkColorFromColorId(
          ui::NativeTheme::kColorId_TextfieldDefaultBackground);
    case ui::NativeTheme::kColorId_ResultsTableHoveredBackground:
      return color_utils::AlphaBlend(
          SkColorFromColorId(
              ui::NativeTheme::kColorId_TextfieldDefaultBackground),
          SkColorFromColorId(
              ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused),
          0x80);
    case ui::NativeTheme::kColorId_ResultsTableSelectedBackground:
      return SkColorFromColorId(
          ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused);
    case ui::NativeTheme::kColorId_ResultsTableNormalText:
    case ui::NativeTheme::kColorId_ResultsTableHoveredText:
      return SkColorFromColorId(
          ui::NativeTheme::kColorId_TextfieldDefaultColor);
    case ui::NativeTheme::kColorId_ResultsTableSelectedText:
      return SkColorFromColorId(
          ui::NativeTheme::kColorId_TextfieldSelectionColor);
    case ui::NativeTheme::kColorId_ResultsTableNormalDimmedText:
    case ui::NativeTheme::kColorId_ResultsTableHoveredDimmedText:
      return color_utils::AlphaBlend(
          SkColorFromColorId(ui::NativeTheme::kColorId_TextfieldDefaultColor),
          SkColorFromColorId(
              ui::NativeTheme::kColorId_TextfieldDefaultBackground),
          0x80);
    case ui::NativeTheme::kColorId_ResultsTableSelectedDimmedText:
      return color_utils::AlphaBlend(
          SkColorFromColorId(ui::NativeTheme::kColorId_TextfieldSelectionColor),
          SkColorFromColorId(
              ui::NativeTheme::kColorId_TextfieldDefaultBackground),
          0x80);
    case ui::NativeTheme::kColorId_ResultsTableNormalUrl:
    case ui::NativeTheme::kColorId_ResultsTableHoveredUrl:
      return NormalURLColor(
          SkColorFromColorId(ui::NativeTheme::kColorId_TextfieldDefaultColor));
    case ui::NativeTheme::kColorId_ResultsTableSelectedUrl:
      return SelectedURLColor(
          SkColorFromColorId(ui::NativeTheme::kColorId_TextfieldSelectionColor),
          SkColorFromColorId(
              ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused));
    case ui::NativeTheme::kColorId_ResultsTablePositiveText:
      return color_utils::GetReadableColor(
          kPositiveTextColor,
          SkColorFromColorId(
              ui::NativeTheme::kColorId_TextfieldDefaultBackground));
    case ui::NativeTheme::kColorId_ResultsTablePositiveHoveredText:
      return color_utils::GetReadableColor(
          kPositiveTextColor,
          SkColorFromColorId(
              ui::NativeTheme::kColorId_TextfieldDefaultBackground));
    case ui::NativeTheme::kColorId_ResultsTablePositiveSelectedText:
      return color_utils::GetReadableColor(
          kPositiveTextColor,
          SkColorFromColorId(
              ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused));
    case ui::NativeTheme::kColorId_ResultsTableNegativeText:
      return color_utils::GetReadableColor(
          kNegativeTextColor,
          SkColorFromColorId(
              ui::NativeTheme::kColorId_TextfieldDefaultBackground));
    case ui::NativeTheme::kColorId_ResultsTableNegativeHoveredText:
      return color_utils::GetReadableColor(
          kNegativeTextColor,
          SkColorFromColorId(
              ui::NativeTheme::kColorId_TextfieldDefaultBackground));
    case ui::NativeTheme::kColorId_ResultsTableNegativeSelectedText:
      return color_utils::GetReadableColor(
          kNegativeTextColor,
          SkColorFromColorId(
              ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused));

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
  g_type_class_unref(g_type_class_ref(gtk_entry_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_info_bar_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_label_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_menu_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_menu_bar_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_menu_item_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_range_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_scrollbar_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_scrolled_window_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_separator_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_spinner_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_text_view_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_toolbar_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_tooltip_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_tree_view_get_type()));
  g_type_class_unref(g_type_class_ref(gtk_window_get_type()));

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

void NativeThemeGtk3::PaintArrowButton(cc::PaintCanvas* canvas,
                                       const gfx::Rect& rect,
                                       Part direction,
                                       State state) const {
  auto context = GetStyleContextFromCss(
      GtkVersionCheck(3, 20)
          ? "GtkScrollbar#scrollbar #contents GtkButton#button"
          : "GtkRange.scrollbar.button");
  GtkStateFlags state_flags = StateToStateFlags(state);
  gtk_style_context_set_state(context, state_flags);

  switch (direction) {
    case kScrollbarUpArrow:
      gtk_style_context_add_class(context, GTK_STYLE_CLASS_TOP);
      break;
    case kScrollbarRightArrow:
      gtk_style_context_add_class(context, GTK_STYLE_CLASS_RIGHT);
      break;
    case kScrollbarDownArrow:
      gtk_style_context_add_class(context, GTK_STYLE_CLASS_BOTTOM);
      break;
    case kScrollbarLeftArrow:
      gtk_style_context_add_class(context, GTK_STYLE_CLASS_LEFT);
      break;
    default:
      NOTREACHED();
  }

  PaintWidget(canvas, rect, context, BG_RENDER_NORMAL, true);
  PaintArrow(canvas, rect, direction, GetFgColorFromStyleContext(context));
}

void NativeThemeGtk3::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect) const {
  PaintWidget(canvas, rect, GetStyleContextFromCss(
                                GtkVersionCheck(3, 20)
                                    ? "GtkScrollbar#scrollbar #contents #trough"
                                    : "GtkScrollbar.scrollbar.trough"),
              BG_RENDER_NORMAL, true);
}

void NativeThemeGtk3::PaintScrollbarThumb(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const gfx::Rect& rect,
    NativeTheme::ScrollbarOverlayColorTheme theme) const {
  auto context = GetStyleContextFromCss(
      GtkVersionCheck(3, 20)
          ? "GtkScrollbar#scrollbar #contents #trough #slider"
          : "GtkScrollbar.scrollbar.slider");
  gtk_style_context_set_state(context, StateToStateFlags(state));
  PaintWidget(canvas, rect, context, BG_RENDER_NORMAL, true);
}

void NativeThemeGtk3::PaintScrollbarCorner(cc::PaintCanvas* canvas,
                                           State state,
                                           const gfx::Rect& rect) const {
  auto context = GetStyleContextFromCss(
      GtkVersionCheck(3, 19, 2)
          ? "GtkScrolledWindow#scrolledwindow #junction"
          : "GtkScrolledWindow.scrolledwindow.scrollbars-junction");
  PaintWidget(canvas, rect, context, BG_RENDER_NORMAL, true);
}

void NativeThemeGtk3::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background) const {
  PaintWidget(canvas, gfx::Rect(size), GetStyleContextFromCss("GtkMenu#menu"),
              BG_RENDER_RECURSIVE, false);
}

void NativeThemeGtk3::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item) const {
  auto context = GetStyleContextFromCss("GtkMenu#menu GtkMenuItem#menuitem");
  gtk_style_context_set_state(context, StateToStateFlags(state));
  PaintWidget(canvas, rect, context, BG_RENDER_NORMAL, true);
}

void NativeThemeGtk3::PaintMenuSeparator(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuSeparatorExtraParams& menu_separator) const {
  auto separator_offset = [&](int separator_thickness) {
    switch (menu_separator.type) {
      case ui::LOWER_SEPARATOR:
        return rect.height() - separator_thickness;
      case ui::UPPER_SEPARATOR:
        return 0;
      default:
        return (rect.height() - separator_thickness) / 2;
    }
  };
  if (GtkVersionCheck(3, 20)) {
    auto context = GetStyleContextFromCss(
        "GtkMenu#menu GtkSeparator#separator.horizontal");
    GtkBorder margin, border, padding;
    GtkStateFlags state = gtk_style_context_get_state(context);
    gtk_style_context_get_margin(context, state, &margin);
    gtk_style_context_get_border(context, state, &border);
    gtk_style_context_get_padding(context, state, &padding);
    int min_height = 1;
    gtk_style_context_get(context, state, "min-height", &min_height, nullptr);
    int w = rect.width() - margin.left - margin.right;
    int h = std::max(
        min_height + padding.top + padding.bottom + border.top + border.bottom,
        1);
    int x = margin.left;
    int y = separator_offset(h);
    PaintWidget(canvas, gfx::Rect(x, y, w, h), context, BG_RENDER_NORMAL, true);
  } else {
    auto context = GetStyleContextFromCss(
        "GtkMenu#menu GtkMenuItem#menuitem.separator.horizontal");
    gboolean wide_separators = false;
    gint separator_height = 0;
    gtk_style_context_get_style(context, "wide-separators", &wide_separators,
                                "separator-height", &separator_height, nullptr);
    // This code was adapted from gtk/gtkmenuitem.c.  For some reason,
    // padding is used as the margin.
    GtkBorder padding;
    gtk_style_context_get_padding(context, gtk_style_context_get_state(context),
                                  &padding);
    int w = rect.width() - padding.left - padding.right;
    int x = rect.x() + padding.left;
    int h = wide_separators ? separator_height : 1;
    int y = rect.y() + separator_offset(h);
    if (wide_separators) {
      PaintWidget(canvas, gfx::Rect(x, y, w, h), context, BG_RENDER_NONE, true);
    } else {
      cc::PaintFlags flags;
      flags.setColor(GetFgColorFromStyleContext(context));
      canvas->drawLine(x, y, x + w, y, flags);
    }
  }
}

void NativeThemeGtk3::PaintFrameTopArea(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const FrameTopAreaExtraParams& frame_top_area) const {
  auto context = GetStyleContextFromCss(frame_top_area.use_custom_frame
                                            ? "#headerbar.header-bar.titlebar"
                                            : "GtkMenuBar#menubar");
  ApplyCssToContext(context, "* { border-radius: 0px; border-style: none; }");
  gtk_style_context_set_state(context, frame_top_area.is_active
                                           ? GTK_STATE_FLAG_NORMAL
                                           : GTK_STATE_FLAG_BACKDROP);

  SkBitmap bitmap =
      GetWidgetBitmap(rect.size(), context, BG_RENDER_RECURSIVE, false);

  if (frame_top_area.incognito) {
    bitmap = SkBitmapOperations::CreateHSLShiftedBitmap(
        bitmap, kDefaultTintFrameIncognito);
  }

  canvas->drawBitmap(bitmap, rect.x(), rect.y());
}

}  // namespace libgtkui
