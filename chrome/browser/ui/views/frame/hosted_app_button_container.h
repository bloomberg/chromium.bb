// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_BUTTON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_BUTTON_CONTAINER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

class AppMenu;
class HostedAppMenuModel;
class BrowserView;

// A container for hosted app buttons in the title bar.
class HostedAppButtonContainer : public views::View,
                                 public ContentSettingImageView::Delegate {
 public:
  // |active_icon_color| and |inactive_icon_color| indicate the colors to use
  // for button icons when the window is focused and blurred respectively.
  HostedAppButtonContainer(BrowserView* browser_view,
                           SkColor active_icon_color,
                           SkColor inactive_icon_color);
  ~HostedAppButtonContainer() override;

  // Updates the visibility of each content setting view.
  void RefreshContentSettingViews();

  // Sets the container to paints its buttons the active/inactive color.
  void SetPaintAsActive(bool active);

 private:
  FRIEND_TEST_ALL_PREFIXES(HostedAppNonClientFrameViewAshTest, HostedAppFrame);

  // The 'app menu' button for the hosted app.
  class AppMenuButton : public views::MenuButton,
                        public views::MenuButtonListener {
   public:
    explicit AppMenuButton(BrowserView* browser_view);
    ~AppMenuButton() override;

    // Sets the color of the menu button icon.
    void SetIconColor(SkColor color);

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
    std::unique_ptr<HostedAppMenuModel> menu_model_;
    std::unique_ptr<AppMenu> menu_;

    DISALLOW_COPY_AND_ASSIGN(AppMenuButton);
  };

  // ContentSettingsImageView::Delegate:
  content::WebContents* GetContentSettingWebContents() override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

  // views::View:
  void ChildVisibilityChanged(views::View* child) override;

  // The containing browser view.
  BrowserView* browser_view_;

  // Button colors.
  const SkColor active_icon_color_;
  const SkColor inactive_icon_color_;

  // Owned by the views hierarchy.
  AppMenuButton* app_menu_button_;
  std::vector<ContentSettingImageView*> content_setting_views_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppButtonContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_BUTTON_CONTAINER_H_
