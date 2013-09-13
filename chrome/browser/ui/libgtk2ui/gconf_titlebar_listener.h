// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_GCONF_TITLEBAR_LISTENER_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_GCONF_TITLEBAR_LISTENER_H_

#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

#include <set>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_signal.h"

namespace libgtk2ui {
class Gtk2UI;

// On GNOME desktops, subscribes to the gconf key which controlls button order.
// Everywhere else, SetTiltebarButtons() just calls back into BrowserTitlebar
// with the default ordering.
class GConfTitlebarListener {
 public:
  // Sends data to the Gtk2UI when available.
  explicit GConfTitlebarListener(Gtk2UI* delegate);
  ~GConfTitlebarListener();

 private:
  // Called whenever the metacity key changes.
  CHROMEG_CALLBACK_2(GConfTitlebarListener, void, OnChangeNotification,
                     GConfClient*, guint, GConfEntry*);

  // Checks |error|. On error, prints out a message and closes the connection
  // to GConf and reverts to default mode.
  bool HandleGError(GError* error, const char* key);

  // Parses the return data structure from GConf, falling back to the default
  // value on any error.
  void ParseAndStoreValue(GConfValue* gconf_value);

  Gtk2UI* delegate_;

  // Pointer to our gconf context. NULL if we aren't on a desktop that uses
  // gconf.
  GConfClient* client_;

  DISALLOW_COPY_AND_ASSIGN(GConfTitlebarListener);
};

}  // namespace libgtk2ui

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_GCONF_TITLEBAR_LISTENER_H_
