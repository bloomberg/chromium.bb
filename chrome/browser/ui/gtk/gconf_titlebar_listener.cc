// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/gconf_titlebar_listener.h"

#include <gtk/gtk.h>

#include "base/environment.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/nix/xdg_util.h"
#include "chrome/browser/ui/gtk/browser_titlebar.h"

namespace {

// The GConf key we read for the button placement string. Even through the key
// has "metacity" in it, it's shared between metacity and compiz.
const char* kButtonLayoutKey = "/apps/metacity/general/button_layout";

// GConf requires us to subscribe to a parent directory before we can subscribe
// to changes in an individual key in that directory.
const char* kMetacityGeneral = "/apps/metacity/general";

}  // namespace

// Public interface:

// static
GConfTitlebarListener* GConfTitlebarListener::GetInstance() {
  return Singleton<GConfTitlebarListener>::get();
}

void GConfTitlebarListener::SetTitlebarButtons(BrowserTitlebar* titlebar) {
  if (client_) {
    titlebar->BuildButtons(current_value_);
    titlebars_.insert(titlebar);
  } else {
    titlebar->BuildButtons(BrowserTitlebar::kDefaultButtonString);
  }
}

void GConfTitlebarListener::RemoveObserver(BrowserTitlebar* titlebar) {
  titlebars_.erase(titlebar);
}

// Protected:

GConfTitlebarListener::~GConfTitlebarListener() {}

// Private:

GConfTitlebarListener::GConfTitlebarListener() : client_(NULL) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (base::nix::GetDesktopEnvironment(env.get()) ==
      base::nix::DESKTOP_ENVIRONMENT_GNOME) {
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

void GConfTitlebarListener::OnChangeNotification(GConfClient* client,
                                                 guint cnxn_id,
                                                 GConfEntry* entry) {
  if (strcmp(gconf_entry_get_key(entry), kButtonLayoutKey) == 0) {
    GConfValue* gconf_value = gconf_entry_get_value(entry);
    ParseAndStoreValue(gconf_value);

    // Broadcast the new configuration to all windows:
    for (std::set<BrowserTitlebar*>::const_iterator it = titlebars_.begin();
         it != titlebars_.end(); ++it) {
      (*it)->BuildButtons(current_value_);
    }
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
  if (gconf_value) {
    const char* value = gconf_value_get_string(gconf_value);
    current_value_ = value ? value : BrowserTitlebar::kDefaultButtonString;
  } else {
    current_value_ = BrowserTitlebar::kDefaultButtonString;
  }
}
