// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GCONF_TITLEBAR_LISTENER_H_
#define CHROME_BROWSER_UI_GTK_GCONF_TITLEBAR_LISTENER_H_
#pragma once

#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

#include <set>
#include <string>

#include "base/basictypes.h"
#include "ui/base/gtk/gtk_signal.h"

class BrowserTitlebar;
template <typename T> struct DefaultSingletonTraits;

// On GNOME desktops, subscribes to the gconf key which controlls button order.
// Everywhere else, SetTiltebarButtons() just calls back into BrowserTitlebar
// with the default ordering.
//
// Meant to be used as a Singleton through base/memory/singleton.h's interface.
class GConfTitlebarListener {
 public:
  // Returns the singleton instance.
  static GConfTitlebarListener* GetInstance();

  // Sets the current titlebar button order. On GNOME desktops, also subscribes
  // to further notifications when this changes.
  void SetTitlebarButtons(BrowserTitlebar* titlebar);

  // Removes |titlebar| from the list of objects observing button order change
  // notifications.
  void RemoveObserver(BrowserTitlebar* titlebar);

 protected:
  virtual ~GConfTitlebarListener();

 private:
  // Private constructor to enforce singleton access.
  GConfTitlebarListener();

  // Called whenever the metacity key changes.
  CHROMEG_CALLBACK_2(GConfTitlebarListener, void, OnChangeNotification,
                     GConfClient*, guint, GConfEntry*);

  // Checks |error|. On error, prints out a message and closes the connection
  // to GConf and reverts to default mode.
  bool HandleGError(GError* error, const char* key);

  // Parses the return data structure from GConf, falling back to the default
  // value on any error.
  void ParseAndStoreValue(GConfValue* gconf_value);

  // Pointer to our gconf context. NULL if we aren't on a desktop that uses
  // gconf.
  GConfClient* client_;

  // The current button ordering as heard from gconf.
  std::string current_value_;

  // BrowserTitlebar objects which have subscribed to updates.
  std::set<BrowserTitlebar*> titlebars_;

  friend struct DefaultSingletonTraits<GConfTitlebarListener>;
  DISALLOW_COPY_AND_ASSIGN(GConfTitlebarListener);
};

#endif  // CHROME_BROWSER_UI_GTK_GCONF_TITLEBAR_LISTENER_H_
