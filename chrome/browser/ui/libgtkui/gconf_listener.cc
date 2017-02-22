// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/gconf_listener.h"

#include <gtk/gtk.h>

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_tokenizer.h"
#include "chrome/browser/ui/libgtkui/gtk_ui.h"
#include "ui/base/x/x11_util.h"
#include "ui/views/window/frame_buttons.h"

namespace {

// The GConf key we read for the button placement string. Even through the key
// has "metacity" in it, it's shared between metacity and compiz.
const char kButtonLayoutKey[] = "/apps/metacity/general/button_layout";

// The GConf key we read for what to do in case of middle clicks on non client
// area. Even through the key has "metacity" in it, it's shared between
// metacity and compiz.
const char kMiddleClickActionKey[] =
    "/apps/metacity/general/action_middle_click_titlebar";

// GConf requires us to subscribe to a parent directory before we can subscribe
// to changes in an individual key in that directory.
const char kMetacityGeneral[] = "/apps/metacity/general";

const char kDefaultButtonString[] = ":minimize,maximize,close";

}  // namespace

namespace libgtkui {

// Public interface:

GConfListener::GConfListener(GtkUi* delegate)
    : delegate_(delegate), client_(nullptr) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment de =
      base::nix::GetDesktopEnvironment(env.get());
  if (de == base::nix::DESKTOP_ENVIRONMENT_GNOME ||
      de == base::nix::DESKTOP_ENVIRONMENT_UNITY ||
      ui::GuessWindowManager() == ui::WM_METACITY) {
    client_ = gconf_client_get_default();
    // If we fail to get a context, that's OK, since we'll just fallback on
    // not receiving gconf keys.
    if (client_) {
      // Register that we're interested in the values of this directory.
      GError* error = nullptr;
      gconf_client_add_dir(client_, kMetacityGeneral,
                           GCONF_CLIENT_PRELOAD_ONELEVEL, &error);
      if (HandleGError(error, kMetacityGeneral))
        return;

      // Get the initial value of the keys we're interested in.
      GetAndRegister(kButtonLayoutKey,
                     base::Bind(&GConfListener::ParseAndStoreButtonValue,
                                base::Unretained(this)));
      GetAndRegister(kMiddleClickActionKey,
                     base::Bind(&GConfListener::ParseAndStoreMiddleClickValue,
                                base::Unretained(this)));
    }
  }
}

GConfListener::~GConfListener() {
}

// Private:

void GConfListener::GetAndRegister(
    const char* key_to_subscribe,
    const base::Callback<void(GConfValue*)>& initial_setter) {
  GError* error = nullptr;
  GConfValue* gconf_value = gconf_client_get(client_, key_to_subscribe,
                                             &error);
  if (HandleGError(error, key_to_subscribe))
    return;
  initial_setter.Run(gconf_value);
  if (gconf_value)
    gconf_value_free(gconf_value);

  // Register to get notifies about changes to this key.
  gconf_client_notify_add(
      client_, key_to_subscribe,
      reinterpret_cast<void (*)(GConfClient*, guint, GConfEntry*, void*)>(
          OnChangeNotificationThunk),
      this, nullptr, &error);
  if (HandleGError(error, key_to_subscribe))
    return;
}

void GConfListener::OnChangeNotification(GConfClient* client,
                                         guint cnxn_id,
                                         GConfEntry* entry) {
  if (strcmp(gconf_entry_get_key(entry), kButtonLayoutKey) == 0) {
    GConfValue* gconf_value = gconf_entry_get_value(entry);
    ParseAndStoreButtonValue(gconf_value);
  } else if (strcmp(gconf_entry_get_key(entry), kMiddleClickActionKey) == 0) {
    GConfValue* gconf_value = gconf_entry_get_value(entry);
    ParseAndStoreMiddleClickValue(gconf_value);
  }
}

bool GConfListener::HandleGError(GError* error, const char* key) {
  if (error != nullptr) {
    LOG(ERROR) << "Error with gconf key '" << key << "': " << error->message;
    g_error_free(error);
    g_object_unref(client_);
    client_ = nullptr;
    return true;
  }
  return false;
}

void GConfListener::ParseAndStoreButtonValue(GConfValue* gconf_value) {
  std::string button_string;
  if (gconf_value) {
    const char* value = gconf_value_get_string(gconf_value);
    button_string = value ? value : kDefaultButtonString;
  } else {
    button_string = kDefaultButtonString;
  }

  // Parse the button_layout string.
  std::vector<views::FrameButton> leading_buttons;
  std::vector<views::FrameButton> trailing_buttons;
  bool left_side = true;
  base::StringTokenizer tokenizer(button_string, ":,");
  tokenizer.set_options(base::StringTokenizer::RETURN_DELIMS);
  while (tokenizer.GetNext()) {
    if (tokenizer.token_is_delim()) {
      if (*tokenizer.token_begin() == ':')
        left_side = false;
    } else {
      base::StringPiece token = tokenizer.token_piece();
      if (token == "minimize") {
        (left_side ? leading_buttons : trailing_buttons).push_back(
            views::FRAME_BUTTON_MINIMIZE);
      } else if (token == "maximize") {
        (left_side ? leading_buttons : trailing_buttons).push_back(
            views::FRAME_BUTTON_MAXIMIZE);
      } else if (token == "close") {
        (left_side ? leading_buttons : trailing_buttons).push_back(
            views::FRAME_BUTTON_CLOSE);
      }
    }
  }

  delegate_->SetWindowButtonOrdering(leading_buttons, trailing_buttons);
}

void GConfListener::ParseAndStoreMiddleClickValue(GConfValue* gconf_value) {
  GtkUi::NonClientMiddleClickAction action =
      views::LinuxUI::MIDDLE_CLICK_ACTION_LOWER;
  if (gconf_value) {
    const char* value = gconf_value_get_string(gconf_value);

    if (strcmp(value, "none") == 0) {
      action = views::LinuxUI::MIDDLE_CLICK_ACTION_NONE;
    } else if (strcmp(value, "lower") == 0) {
      action = views::LinuxUI::MIDDLE_CLICK_ACTION_LOWER;
    } else if (strcmp(value, "minimize") == 0) {
      action = views::LinuxUI::MIDDLE_CLICK_ACTION_MINIMIZE;
    } else if (strcmp(value, "toggle-maximize") == 0) {
      action = views::LinuxUI::MIDDLE_CLICK_ACTION_TOGGLE_MAXIMIZE;
    } else {
      // While we want to have the default state be lower if there isn't a
      // value, we want to default to no action if the user has explicitly
      // chose an action that we don't implement.
      action = views::LinuxUI::MIDDLE_CLICK_ACTION_NONE;
    }
  }

  delegate_->SetNonClientMiddleClickAction(action);
}

}  // namespace libgtkui
