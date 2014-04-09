// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_GCONF_LISTENER_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_GCONF_LISTENER_H_

#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_signal.h"

namespace libgtk2ui {
class Gtk2UI;

// On GNOME desktops, subscribes to the gconf key which controlls button order.
// Everywhere else, SetTiltebarButtons() just calls back into BrowserTitlebar
// with the default ordering.
class GConfListener {
 public:
  // Sends data to the Gtk2UI when available.
  explicit GConfListener(Gtk2UI* delegate);
  ~GConfListener();

 private:
  // Called whenever the metacity key changes.
  CHROMEG_CALLBACK_2(GConfListener, void, OnChangeNotification,
                     GConfClient*, guint, GConfEntry*);

  void GetAndRegister(const char* key_to_subscribe,
                      const base::Callback<void(GConfValue*)>& initial_setter);

  // Checks |error|. On error, prints out a message and closes the connection
  // to GConf and reverts to default mode.
  bool HandleGError(GError* error, const char* key);

  // Parses the return data structure from GConf, falling back to the default
  // value on any error.
  void ParseAndStoreButtonValue(GConfValue* gconf_value);
  void ParseAndStoreMiddleClickValue(GConfValue* gconf_value);

  Gtk2UI* delegate_;

  // Pointer to our gconf context. NULL if we aren't on a desktop that uses
  // gconf.
  GConfClient* client_;

  DISALLOW_COPY_AND_ASSIGN(GConfListener);
};

}  // namespace libgtk2ui

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_GCONF_LISTENER_H_
