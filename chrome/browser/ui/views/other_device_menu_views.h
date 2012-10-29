// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OTHER_DEVICE_MENU_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OTHER_DEVICE_MENU_VIEWS_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/search/other_device_menu_controller.h"

namespace gfx{
class Point;
}

namespace ui {
class SimpleMenuModel;
}

namespace views {
class MenuRunner;
}

class OtherDeviceMenuViews : public OtherDeviceMenu {
 public:
  explicit OtherDeviceMenuViews(ui::SimpleMenuModel* menu_model);
  virtual ~OtherDeviceMenuViews();

  virtual void ShowMenu(
      gfx::NativeWindow window, const gfx::Point& location) OVERRIDE;

 private:
  ui::SimpleMenuModel* menu_model_;  // Owned by the controller.
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(OtherDeviceMenuViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OTHER_DEVICE_MENU_VIEWS_H_
