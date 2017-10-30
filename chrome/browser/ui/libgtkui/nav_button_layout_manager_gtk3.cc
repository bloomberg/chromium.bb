// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/nav_button_layout_manager_gtk3.h"

#include "base/strings/string_split.h"
#include "chrome/browser/ui/libgtkui/gtk_ui.h"
#include "chrome/browser/ui/libgtkui/gtk_util.h"

namespace libgtkui {

namespace {

const char kDefaultGtkLayout[] = "menu:minimize,maximize,close";

std::string GetGtkSettingsStringProperty(GtkSettings* settings,
                                         const gchar* prop_name) {
  GValue layout = G_VALUE_INIT;
  g_value_init(&layout, G_TYPE_STRING);
  g_object_get_property(G_OBJECT(settings), prop_name, &layout);
  DCHECK(G_VALUE_HOLDS_STRING(&layout));
  std::string prop_value(g_value_get_string(&layout));
  g_value_unset(&layout);
  return prop_value;
}

std::string GetDecorationLayoutFromGtkWindow() {
  static ScopedStyleContext context;
  if (!context) {
    context = GetStyleContextFromCss("");
    gtk_style_context_add_class(context, "csd");
  }

  gchar* layout_c = nullptr;
  gtk_style_context_get_style(context, "decoration-button-layout", &layout_c,
                              nullptr);
  DCHECK(layout_c);
  std::string layout(layout_c);
  g_free(layout_c);
  return layout;
}

}  // namespace

NavButtonLayoutManagerGtk3::NavButtonLayoutManagerGtk3(GtkUi* delegate)
    : delegate_(delegate), signal_id_(0), signal_id_middle_click_(0) {
  DCHECK(delegate_);
  GtkSettings* settings = gtk_settings_get_default();
  if (GtkVersionCheck(3, 14)) {
    signal_id_ = g_signal_connect(
        settings, "notify::gtk-decoration-layout",
        G_CALLBACK(OnDecorationButtonLayoutChangedThunk), this);
    DCHECK(signal_id_);
    OnDecorationButtonLayoutChanged(settings, nullptr);

    signal_id_middle_click_ =
        g_signal_connect(settings, "notify::gtk-titlebar-middle-click",
                         G_CALLBACK(OnMiddleClickActionChangedThunk), this);
    DCHECK(signal_id_middle_click_);
    OnMiddleClickActionChanged(settings, nullptr);
  } else if (GtkVersionCheck(3, 10, 3)) {
    signal_id_ = g_signal_connect_after(settings, "notify::gtk-theme-name",
                                        G_CALLBACK(OnThemeChangedThunk), this);
    DCHECK(signal_id_);
    OnThemeChanged(settings, nullptr);
  } else {
    // On versions older than 3.10.3, the layout was hardcoded.
    SetWindowButtonOrderingFromGtkLayout(kDefaultGtkLayout);
  }
}

NavButtonLayoutManagerGtk3::~NavButtonLayoutManagerGtk3() {
  if (signal_id_)
    g_signal_handler_disconnect(gtk_settings_get_default(), signal_id_);
  if (signal_id_middle_click_)
    g_signal_handler_disconnect(gtk_settings_get_default(),
                                signal_id_middle_click_);
}

void NavButtonLayoutManagerGtk3::SetWindowButtonOrderingFromGtkLayout(
    const std::string& gtk_layout) {
  std::vector<views::FrameButton> leading_buttons;
  std::vector<views::FrameButton> trailing_buttons;
  ParseButtonLayout(gtk_layout, &leading_buttons, &trailing_buttons);
  delegate_->SetWindowButtonOrdering(leading_buttons, trailing_buttons);
}

void NavButtonLayoutManagerGtk3::OnDecorationButtonLayoutChanged(
    GtkSettings* settings,
    GParamSpec* param) {
  SetWindowButtonOrderingFromGtkLayout(
      GetGtkSettingsStringProperty(settings, "gtk-decoration-layout"));
}

void NavButtonLayoutManagerGtk3::OnMiddleClickActionChanged(
    GtkSettings* settings,
    GParamSpec* param) {
  std::string value =
      GetGtkSettingsStringProperty(settings, "gtk-titlebar-middle-click");
  GtkUi::NonClientMiddleClickAction action;
  if (value == "none") {
    action = views::LinuxUI::MIDDLE_CLICK_ACTION_NONE;
  } else if (value == "lower") {
    action = views::LinuxUI::MIDDLE_CLICK_ACTION_LOWER;
  } else if (value == "minimize") {
    action = views::LinuxUI::MIDDLE_CLICK_ACTION_MINIMIZE;
  } else if (value == "toggle-maximize") {
    action = views::LinuxUI::MIDDLE_CLICK_ACTION_TOGGLE_MAXIMIZE;
  } else {
    // Default to no action if the user has explicitly
    // chosen an action that we don't implement.
    action = views::LinuxUI::MIDDLE_CLICK_ACTION_NONE;
  }
  delegate_->SetNonClientMiddleClickAction(action);
}

void NavButtonLayoutManagerGtk3::OnThemeChanged(GtkSettings* settings,
                                                GParamSpec* param) {
  std::string layout = GetDecorationLayoutFromGtkWindow();
  SetWindowButtonOrderingFromGtkLayout(layout);
}

}  // namespace libgtkui
