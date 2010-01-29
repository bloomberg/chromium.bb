// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_AREA_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_AREA_VIEW_H_

#include "app/gfx/native_widget_types.h"
#include "app/menus/simple_menu_model.h"
#include "base/basictypes.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/controls/menu/menu_2.h"
#include "views/view.h"

class AppMenuModel;
class BrowserView;

namespace chromeos {

class ClockMenuButton;
class LanguageMenuButton;
class NetworkMenuButton;
class PowerMenuButton;
class StatusAreaButton;

// This class is used to wrap the small informative widgets in the upper-right
// of the window title bar. It is used on ChromeOS only.
class StatusAreaView : public views::View,
                       public menus::SimpleMenuModel::Delegate,
                       public views::ViewMenuDelegate {
 public:
  enum OpenTabsMode {
    OPEN_TABS_ON_LEFT = 1,
    OPEN_TABS_CLOBBER,
    OPEN_TABS_ON_RIGHT
  };

  explicit StatusAreaView(BrowserView* browser_view);
  virtual ~StatusAreaView() {}

  void Init();

  // Called when the compact navigation bar mode has changed to
  // toggle the app menu visibility.
  void Update();

  // Creates an AppMenuModel for chromeos.
  AppMenuModel* CreateAppMenuModel(menus::SimpleMenuModel::Delegate* delegate);

  // views::View* overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  static OpenTabsMode GetOpenTabsMode();
  static void SetOpenTabsMode(OpenTabsMode mode);

 private:

  // menus::SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // The browser window that owns us.
  BrowserView* browser_view_;

  ClockMenuButton* clock_view_;
  LanguageMenuButton* language_view_;
  NetworkMenuButton* network_view_;
  PowerMenuButton* battery_view_;
  StatusAreaButton* menu_view_;

  scoped_ptr<menus::SimpleMenuModel> app_menu_contents_;
  scoped_ptr<menus::SimpleMenuModel> options_menu_contents_;
  scoped_ptr<views::Menu2> app_menu_menu_;

  static OpenTabsMode open_tabs_mode_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_AREA_VIEW_H_
