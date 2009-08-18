// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_STATUS_AREA_VIEW_H_
#define CHROME_BROWSER_VIEWS_FRAME_STATUS_AREA_VIEW_H_

#include "base/basictypes.h"
#include "views/controls/menu/simple_menu_model.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"

class Browser;

namespace views {
class MenuButton;
class ImageView;
}

// This class is used to wrap the small informative widgets in the upper-right
// of the window title bar. It is used on ChromeOS only.
class StatusAreaView : public views::View,
                       public views::SimpleMenuModel::Delegate,
                       public views::ViewMenuDelegate {
 public:
  enum OpenTabsMode {
    OPEN_TABS_ON_LEFT = 1,
    OPEN_TABS_CLOBBER,
    OPEN_TABS_ON_RIGHT
  };

  StatusAreaView(Browser* browser);
  virtual ~StatusAreaView();

  void Init();

  // views::View* overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);

  static OpenTabsMode GetOpenTabsMode();
  static void SetOpenTabsMode(OpenTabsMode mode);

 private:
  void CreateAppMenu();

  // views::SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          views::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt,
                       gfx::NativeView hwnd);

  // The browser window that owns us.
  Browser* browser_;

  views::ImageView* battery_view_;
  views::MenuButton* menu_view_;

  scoped_ptr<views::SimpleMenuModel> app_menu_contents_;
  scoped_ptr<views::SimpleMenuModel> options_menu_contents_;
  scoped_ptr<views::Menu2> app_menu_menu_;

  static OpenTabsMode open_tabs_mode_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaView);
};

#endif  // CHROME_BROWSER_VIEWS_FRAME_STATUS_AREA_VIEW_H_
