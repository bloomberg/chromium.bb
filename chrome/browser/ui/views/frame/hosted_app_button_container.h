// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_BUTTON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_BUTTON_CONTAINER_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

class AppMenu;
class AppMenuModel;
class BrowserView;

// A container for hosted app buttons in the title bar.
class HostedAppButtonContainer : public views::View {
 public:
  // |use_light| determines whether the buttons contained in the frame will use
  // dark or light colors for drawing their button images.
  HostedAppButtonContainer(BrowserView* browser_view, bool use_light);
  ~HostedAppButtonContainer() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(HostedAppNonClientFrameViewAshTest, HostedAppFrame);

  // The 'app menu' button for the hosted app.
  class AppMenuButton : public views::MenuButton,
                        public views::MenuButtonListener {
   public:
    AppMenuButton(BrowserView* browser_view, bool use_light);
    ~AppMenuButton() override;

    // views::MenuButtonListener:
    void OnMenuButtonClicked(views::MenuButton* source,
                             const gfx::Point& point,
                             const ui::Event* event) override;

    AppMenu* menu() { return menu_.get(); }

   private:
    // The containing browser view.
    BrowserView* browser_view_;

    // App model and menu.
    // Note that the menu should be destroyed before the model it uses, so the
    // menu should be listed later.
    std::unique_ptr<AppMenuModel> menu_model_;
    std::unique_ptr<AppMenu> menu_;

    DISALLOW_COPY_AND_ASSIGN(AppMenuButton);
  };

  // Owned by the views hierarchy.
  AppMenuButton* app_menu_button_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppButtonContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_BUTTON_CONTAINER_H_
