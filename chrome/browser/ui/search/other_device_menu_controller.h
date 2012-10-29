// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_OTHER_DEVICE_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEARCH_OTHER_DEVICE_MENU_CONTROLLER_H_

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/point.h"

namespace content {
class WebUI;
}

// The view.
class OtherDeviceMenu {
 public:
  virtual ~OtherDeviceMenu();

  // Displays the menu in the specified location and window.
  virtual void ShowMenu(
      gfx::NativeWindow window, const gfx::Point& location) = 0;

  static OtherDeviceMenu* Create(ui::SimpleMenuModel* menu_model);
};

// A controller that builds the menu model and drives the view.  This menu is
// displayed when a device on the ntp_search device page is clicked and contains
// the session for that device.  This is implemented natively rather than in
// JavaScript to allow the menu to extend into the natively-rendered section of
// the NTP.
class OtherDeviceMenuController : public ui::SimpleMenuModel::Delegate {
 public:
  OtherDeviceMenuController(content::WebUI* web_ui,
                            const std::string& session_id,
                            const gfx::Point& location);
  virtual ~OtherDeviceMenuController();

  // Displays the menu by invoking the view.
  void ShowMenu();

 private:
  // ui::SimpleMenuModel::Delegate overrides:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;

  typedef std::vector<linked_ptr<DictionaryValue> > TabData;

  // Adds the tabs of the |session_id_| session to the menu.
  void AddDeviceTabs();

  // Weak - attached to the handler that created this menu.
  content::WebUI* web_ui_;

  // The session ID of the tabs displayed in the menu.
  std::string session_id_;

  // The anchor point for the top-left corner of the menu.
  gfx::Point location_;

  ui::SimpleMenuModel menu_model_;

  // The tab data for each menu item, in the order in which they're displayed.
  // |command_id| i corresponds to index i.
  TabData tab_data_;

  scoped_ptr<OtherDeviceMenu> view_;

  DISALLOW_COPY_AND_ASSIGN(OtherDeviceMenuController);
};

#endif  // CHROME_BROWSER_UI_SEARCH_OTHER_DEVICE_MENU_CONTROLLER_H_
