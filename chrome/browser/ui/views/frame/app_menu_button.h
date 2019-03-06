// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_APP_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_APP_MENU_BUTTON_H_

#include <memory>

#include "base/observer_list.h"
#include "ui/views/controls/button/menu_button.h"

class AppMenu;
class AppMenuButtonObserver;
class AppMenuModel;
class Browser;

namespace views {
class MenuButtonListener;
}  // namespace views

// The app menu button lives in the top right of browser windows. It shows three
// dots and adds a status badge when there's a need to alert the user. Clicking
// displays the app menu.
// TODO: Consider making ToolbarButton and AppMenuButton share a common base
// class https://crbug.com/819854.
class AppMenuButton : public views::MenuButton {
 public:
  explicit AppMenuButton(views::MenuButtonListener* menu_button_listener);
  ~AppMenuButton() override;

  // views::MenuButton:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  SkColor GetInkDropBaseColor() const override;

  void AddObserver(AppMenuButtonObserver* observer);
  void RemoveObserver(AppMenuButtonObserver* observer);

  // Closes the app menu, if it's open.
  void CloseMenu();

  // Called by the app menu when it closes.
  void OnMenuClosed();

  // Whether the app menu is currently showing.
  bool IsMenuShowing() const;

  AppMenu* app_menu() { return menu_.get(); }

 protected:
  // Show the menu. |menu_model| should be a newly created AppMenuModel.  The
  // other params are forwarded to the created AppMenu.
  void RunMenu(std::unique_ptr<AppMenuModel> menu_model,
               Browser* browser,
               int run_flags,
               bool alert_reopen_tab_items);

 private:
  // App model and menu.
  // Note that the menu should be destroyed before the model it uses, so the
  // menu should be listed later.
  // TODO(mgiuca): Simplify this model so that correctness does not depend on
  // destruction order. https://crbug.com/831902
  std::unique_ptr<AppMenuModel> menu_model_;
  std::unique_ptr<AppMenu> menu_;

  base::ObserverList<AppMenuButtonObserver>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(AppMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_APP_MENU_BUTTON_H_
