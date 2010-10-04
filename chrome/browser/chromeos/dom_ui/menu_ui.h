// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_MENU_UI_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_MENU_UI_H_
#pragma once

#include <string>

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui.h"

namespace menus {
class MenuModel;
}  // namespace menus

namespace chromeos {

class DOMUIMenuControl;

class MenuUI : public DOMUI {
 public:
  explicit MenuUI(TabContents* contents);

  // Create HTML Data source for the menu.  Extended menu
  // implementation may provide its own menu implmentation.
  virtual ChromeURLDataManager::DataSource* CreateDataSource();

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuUI);
};

// Base class for MenuUI's DOMMessageHandler.
class MenuHandlerBase : public DOMMessageHandler {
 public:
  MenuHandlerBase() : DOMMessageHandler() {}

  // Returns the menu control that is associated with the
  // MenuUI. This may return null when menu is being deleted.
  DOMUIMenuControl* GetMenuControl();

  // Returns the menu model for this menu ui.
  // This may return null when menu is being deleted.
  menus::MenuModel* GetMenuModel();

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuHandlerBase);
};

// Returns the menu's html code given by the resource id with the code
// to intialization the menu. The resource string should be pure code
// and should not contain i18n string.
std::string GetMenuUIHTMLSourceFromResource(int res);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_MENU_UI_H_
