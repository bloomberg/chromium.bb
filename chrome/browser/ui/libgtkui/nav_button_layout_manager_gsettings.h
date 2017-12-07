// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_NAV_BUTTON_LAYOUT_MANAGER_GSETTINGS_H_
#define CHROME_BROWSER_UI_LIBGTKUI_NAV_BUTTON_LAYOUT_MANAGER_GSETTINGS_H_

#include <gio/gio.h>

#include "base/macros.h"
#include "chrome/browser/ui/libgtkui/nav_button_layout_manager.h"
#include "ui/base/glib/glib_signal.h"

namespace libgtkui {
class GtkUi;

// On GNOME desktops, subscribes to the gsettings key which controlls button
// order and the middle click action. Everywhere else, SetTiltebarButtons()
// just calls back into BrowserTitlebar with the default ordering.
class NavButtonLayoutManagerGSettings : public NavButtonLayoutManager {
 public:
  // Sends data to the GtkUi when available.
  explicit NavButtonLayoutManagerGSettings(GtkUi* delegate);
  ~NavButtonLayoutManagerGSettings() override;

 private:
  CHROMEG_CALLBACK_1(NavButtonLayoutManagerGSettings,
                     void,
                     OnDecorationButtonLayoutChanged,
                     GSettings*,
                     const gchar*);

  CHROMEG_CALLBACK_1(NavButtonLayoutManagerGSettings,
                     void,
                     OnMiddleClickActionChanged,
                     GSettings*,
                     const gchar*);

  void ParseAndStoreButtonValue(const std::string&);

  void ParseAndStoreMiddleClickValue(const std::string&);

  GtkUi* delegate_;

  GSettings* settings_;
  gulong signal_button_id_;
  gulong signal_middle_click_id_;

  DISALLOW_COPY_AND_ASSIGN(NavButtonLayoutManagerGSettings);
};

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_NAV_BUTTON_LAYOUT_MANAGER_GSETTINGS_H_
