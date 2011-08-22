// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_PROFILE_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_GTK_PROFILE_MENU_BUTTON_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class Browser;
class ProfileMenuModel;

// Shows the button for the multiprofile menu.
class ProfileMenuButton {
 public:
  explicit ProfileMenuButton(Browser* browser);

  virtual ~ProfileMenuButton();

  virtual void UpdateText();

  GtkWidget* widget() const { return widget_.get(); }

 private:
  CHROMEGTK_CALLBACK_1(ProfileMenuButton, gboolean, OnButtonPressed,
                       GdkEventButton*);

  scoped_ptr<MenuGtk> menu_;
  scoped_ptr<ProfileMenuModel> profile_menu_model_;
  OwnedWidgetGtk widget_;
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuButton);
};

#endif  // CHROME_BROWSER_UI_GTK_PROFILE_MENU_BUTTON_H_
