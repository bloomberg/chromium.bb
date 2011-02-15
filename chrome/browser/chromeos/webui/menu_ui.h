// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_MENU_UI_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_MENU_UI_H_
#pragma once

#include <string>

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/web_ui.h"

class DictionaryValue;

namespace ui{
class MenuModel;
}  // namespace ui

namespace chromeos {

class WebUIMenuControl;

// MenuSourceDelegate class allows subclass to injects specific values
// to menu javascript code.
class MenuSourceDelegate {
 public:
  virtual ~MenuSourceDelegate() {}
  // Subclass can add extra parameters or replaces default configuration.
  virtual void AddCustomConfigValues(DictionaryValue* config) const {}

  // Subclass can add their values to |localized_strings| and those values
  // are used by JS template builder and could be accessed via JS class
  // LocalStrings.
  virtual void AddLocalizedStrings(DictionaryValue* localized_strings) const {}
};

class MenuUI : public WebUI {
 public:
  explicit MenuUI(TabContents* contents);

  // A callback method that is invoked when a menu model associated
  // with the WebUI Menu gets updated.
  virtual void ModelUpdated(const ui::MenuModel* new_model);

  // Creates a menu item for the menu item at index in the model.
  virtual DictionaryValue* CreateMenuItem(const ui::MenuModel* model,
                                          int index,
                                          const char* type,
                                          int* max_icon_width,
                                          bool* has_accel) const;

  // A utility function which creates a concrete html file from
  // template file |menu_resource_id| and |menu_css_id| for given |menu_class|.
  // The resource_name is the host part of WebUI's url.
  static ChromeURLDataManager::DataSource* CreateMenuUIHTMLSource(
      const MenuSourceDelegate* delegate,
      const std::string& source_name,
      const std::string& menu_class,
      int menu_source_res_id,
      int menu_css_res_id);

  // Returns true if DMOUI menu is enabled.
  static bool IsEnabled();

 protected:
  // A constructor for subclass to initialize the MenuUI with
  // different data source.
  MenuUI(TabContents* contents, ChromeURLDataManager::DataSource* source);

 private:
  // Create HTML Data source for the menu.
  ChromeURLDataManager::DataSource* CreateDataSource();

  DISALLOW_COPY_AND_ASSIGN(MenuUI);
};

// Base class for MenuUI's WebUIMessageHandler.
class MenuHandlerBase : public WebUIMessageHandler {
 public:
  MenuHandlerBase() : WebUIMessageHandler() {}

  // Returns the menu control that is associated with the
  // MenuUI. This may return null when menu is being deleted.
  WebUIMenuControl* GetMenuControl();

  // Returns the menu model for this menu ui.
  // This may return null when menu is being deleted.
  ui::MenuModel* GetMenuModel();

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuHandlerBase);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_MENU_UI_H_
