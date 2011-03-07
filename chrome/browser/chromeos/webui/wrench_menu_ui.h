// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_WRENCH_MENU_UI_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_WRENCH_MENU_UI_H_
#pragma once

#include "chrome/browser/chromeos/webui/menu_ui.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_type.h"

class NotificationSource;
class NotificationDetails;

namespace ui {
class MenuModel;
}  // namespace ui

namespace views {
class Menu2;
}  // namespace views

namespace chromeos {

class WrenchMenuUI : public MenuUI,
                     public NotificationObserver {
 public:
  explicit WrenchMenuUI(TabContents* contents);

  // MenuUI overrides:
  virtual void ModelUpdated(const ui::MenuModel* new_model);

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Updates zoom controls to reflect the current zooming state.
  void UpdateZoomControls();

  // A convenient factory method to create Menu2 for wrench menu.
  static views::Menu2* CreateMenu2(ui::MenuModel* model);

 private:
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WrenchMenuUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_WRENCH_MENU_UI_H_
