// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/gconf_titlebar_listener.h"

#include <gtk/gtk.h>

#include "base/environment.h"
#include "base/memory/scoped_ptr.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_tokenizer.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_ui.h"
#include "ui/base/x/x11_util.h"
#include "ui/views/window/frame_buttons.h"

namespace {

// The GConf key we read for the button placement string. Even through the key
// has "metacity" in it, it's shared between metacity and compiz.
const char* kButtonLayoutKey = "/apps/metacity/general/button_layout";

// GConf requires us to subscribe to a parent directory before we can subscribe
// to changes in an individual key in that directory.
const char* kMetacityGeneral = "/apps/metacity/general";

const char kDefaultButtonString[] = ":minimize,maximize,close";

}  // namespace

namespace libgtk2ui {

// Public interface:

GConfTitlebarListener::GConfTitlebarListener(Gtk2UI* delegate)
    : delegate_(delegate),
      client_(NULL) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment de =
      base::nix::GetDesktopEnvironment(env.get());
  if (de == base::nix::DESKTOP_ENVIRONMENT_GNOME ||
      de == base::nix::DESKTOP_ENVIRONMENT_UNITY ||
      ui::GuessWindowManager() == ui::WM_METACITY) {
    client_ = gconf_client_get_default();
    // If we fail to get a context, that's OK, since we'll just fallback on
    // not receiving gconf keys.
    if (client_) {
      // Get the initial value of the key.
      GError* error = NULL;
      GConfValue* gconf_value = gconf_client_get(client_, kButtonLayoutKey,
                                                 &error);
      if (HandleGError(error, kButtonLayoutKey))
        return;
      ParseAndStoreValue(gconf_value);
      if (gconf_value)
        gconf_value_free(gconf_value);

      // Register that we're interested in the values of this directory.
      gconf_client_add_dir(client_, kMetacityGeneral,
                           GCONF_CLIENT_PRELOAD_ONELEVEL, &error);
      if (HandleGError(error, kMetacityGeneral))
        return;

      // Register to get notifies about changes to this key.
      gconf_client_notify_add(
          client_, kButtonLayoutKey,
          reinterpret_cast<void (*)(GConfClient*, guint, GConfEntry*, void*)>(
              OnChangeNotificationThunk),
          this, NULL, &error);
      if (HandleGError(error, kButtonLayoutKey))
        return;
    }
  }
}

GConfTitlebarListener::~GConfTitlebarListener() {
}

// Private:

void GConfTitlebarListener::OnChangeNotification(GConfClient* client,
                                                 guint cnxn_id,
                                                 GConfEntry* entry) {
  if (strcmp(gconf_entry_get_key(entry), kButtonLayoutKey) == 0) {
    GConfValue* gconf_value = gconf_entry_get_value(entry);
    ParseAndStoreValue(gconf_value);
  }
}

bool GConfTitlebarListener::HandleGError(GError* error, const char* key) {
  if (error != NULL) {
    LOG(ERROR) << "Error with gconf key '" << key << "': " << error->message;
    g_error_free(error);
    g_object_unref(client_);
    client_ = NULL;
    return true;
  }
  return false;
}

void GConfTitlebarListener::ParseAndStoreValue(GConfValue* gconf_value) {
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

}  // namespace libgtk2ui
