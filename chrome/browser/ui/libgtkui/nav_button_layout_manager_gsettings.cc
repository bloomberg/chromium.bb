// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/nav_button_layout_manager_gsettings.h"

#include <gio/gio.h>

#include "chrome/browser/ui/libgtkui/gtk_ui.h"
#include "chrome/browser/ui/libgtkui/gtk_util.h"

namespace {

const char kGeneralPreferencesSchema[] = "org.gnome.desktop.wm.preferences";

const char kButtonLayoutKey[] = "button-layout";
const char kButtonLayoutChangedSignal[] = "changed::button-layout";

const char kMiddleClickActionKey[] = "action-middle-click-titlebar";
const char kMiddleClickActionChangedSignal[] =
    "changed::action-middle-click-titlebar";

const char kDefaultButtonString[] = ":minimize,maximize,close";

}  // namespace

namespace libgtkui {

// Public interface:

NavButtonLayoutManagerGSettings::NavButtonLayoutManagerGSettings(
    GtkUi* delegate)
    : delegate_(delegate), settings_(nullptr) {
  DCHECK(delegate_);

  if (!g_settings_schema_source_lookup(g_settings_schema_source_get_default(),
                                       kGeneralPreferencesSchema, FALSE) ||
      !(settings_ = g_settings_new(kGeneralPreferencesSchema))) {
    LOG(ERROR) << "Unable to create a gsettings client - setting button layout "
                  "to default";
    ParseAndStoreButtonValue(kDefaultButtonString);
    return;
  }

  // Get the inital value of the keys we're interested in.
  OnDecorationButtonLayoutChanged(settings_, kButtonLayoutKey);
  signal_button_id_ =
      g_signal_connect(settings_, kButtonLayoutChangedSignal,
                       G_CALLBACK(OnDecorationButtonLayoutChangedThunk), this);

  OnMiddleClickActionChanged(settings_, kMiddleClickActionKey);
  signal_middle_click_id_ =
      g_signal_connect(settings_, kMiddleClickActionChangedSignal,
                       G_CALLBACK(OnMiddleClickActionChangedThunk), this);
}

NavButtonLayoutManagerGSettings::~NavButtonLayoutManagerGSettings() {
  if (settings_) {
    if (signal_button_id_)
      g_signal_handler_disconnect(settings_, signal_button_id_);
    if (signal_middle_click_id_)
      g_signal_handler_disconnect(settings_, signal_middle_click_id_);
  }
  g_free(settings_);
}

// Private:

void NavButtonLayoutManagerGSettings::OnDecorationButtonLayoutChanged(
    GSettings* settings,
    const gchar* key) {
  gchar* button_layout = g_settings_get_string(settings, kButtonLayoutKey);
  if (!button_layout)
    return;
  ParseAndStoreButtonValue(button_layout);
  g_free(button_layout);
}

void NavButtonLayoutManagerGSettings::ParseAndStoreButtonValue(
    const std::string& button_string) {
  std::vector<views::FrameButton> leading_buttons;
  std::vector<views::FrameButton> trailing_buttons;
  ParseButtonLayout(button_string, &leading_buttons, &trailing_buttons);
  delegate_->SetWindowButtonOrdering(leading_buttons, trailing_buttons);
}

void NavButtonLayoutManagerGSettings::OnMiddleClickActionChanged(
    GSettings* settings,
    const gchar* key) {
  gchar* click_action = g_settings_get_string(settings, kMiddleClickActionKey);
  if (!click_action)
    return;
  ParseAndStoreMiddleClickValue(click_action);
  g_free(click_action);
}

void NavButtonLayoutManagerGSettings::ParseAndStoreMiddleClickValue(
    const std::string& click_action) {
  GtkUi::NonClientMiddleClickAction action;

  if (click_action == "none") {
    action = views::LinuxUI::MIDDLE_CLICK_ACTION_NONE;
  } else if (click_action == "lower") {
    action = views::LinuxUI::MIDDLE_CLICK_ACTION_LOWER;
  } else if (click_action == "minimize") {
    action = views::LinuxUI::MIDDLE_CLICK_ACTION_MINIMIZE;
  } else if (click_action == "toggle-maximize") {
    action = views::LinuxUI::MIDDLE_CLICK_ACTION_TOGGLE_MAXIMIZE;
  } else {
    // While we want to have the default state be lower if there isn't a
    // value, we want to default to no action if the user has explicitly
    // chose an action that we don't implement.
    action = views::LinuxUI::MIDDLE_CLICK_ACTION_NONE;
  }

  delegate_->SetNonClientMiddleClickAction(action);
}

}  // namespace libgtkui
