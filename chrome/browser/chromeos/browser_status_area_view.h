// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BROWSER_STATUS_AREA_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_BROWSER_STATUS_AREA_VIEW_H_

#include "app/menus/simple_menu_model.h"
#include "base/basictypes.h"
#include "chrome/browser/chromeos/status_area_view.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/controls/menu/menu_2.h"
#include "views/view.h"

class AppMenuModel;

namespace chromeos {

class ChromeosBrowserView;
class StatusAreaButton;

// StatusAreView specialization specific for Chrome browser.
// Adds a menu button visible in compact location bar mode.
class BrowserStatusAreaView : public StatusAreaView,
                              public menus::SimpleMenuModel::Delegate,
                              public views::ViewMenuDelegate {
 public:
  explicit BrowserStatusAreaView(ChromeosBrowserView* browser_view);
  virtual ~BrowserStatusAreaView() {}

  virtual void Init();

  // Creates an AppMenuModel for chromeos.
  AppMenuModel* CreateAppMenuModel(menus::SimpleMenuModel::Delegate* delegate);

  StatusAreaButton* menu_view() const { return menu_view_; }

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
  ChromeosBrowserView* browser_view_;

  StatusAreaButton* menu_view_;

  scoped_ptr<menus::SimpleMenuModel> app_menu_contents_;
  scoped_ptr<menus::SimpleMenuModel> options_menu_contents_;
  scoped_ptr<views::Menu2> app_menu_menu_;

  DISALLOW_COPY_AND_ASSIGN(BrowserStatusAreaView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BROWSER_STATUS_AREA_VIEW_H_
