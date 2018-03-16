// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_BUTTON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_BUTTON_CONTAINER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/frame/browser_view_button_provider.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

class AppMenu;
class HostedAppMenuModel;
class BrowserView;

// A container for hosted app buttons in the title bar.
class HostedAppButtonContainer : public views::View,
                                 public BrowserActionsContainer::Delegate,
                                 public BrowserViewButtonProvider {
 public:
  // |active_icon_color| and |inactive_icon_color| indicate the colors to use
  // for button icons when the window is focused and blurred respectively.
  HostedAppButtonContainer(BrowserView* browser_view,
                           SkColor active_icon_color,
                           SkColor inactive_icon_color);
  ~HostedAppButtonContainer() override;

  // Updates the visibility of each content setting.
  void RefreshContentSettingViews();

  // Sets the container to paints its buttons the active/inactive color.
  void SetPaintAsActive(bool active);

  // Animates the menu button and content setting icons. Intended to run in sync
  // with a FrameHeaderOriginText slide animation.
  void StartTitlebarAnimation(base::TimeDelta origin_text_slide_duration);

 private:
  FRIEND_TEST_ALL_PREFIXES(HostedAppNonClientFrameViewAshTest, HostedAppFrame);

  class ContentSettingsContainer;

  const std::vector<ContentSettingImageView*>&
  GetContentSettingViewsForTesting() const;

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

    // Fades the menu button highlight on and off.
    void StartHighlightAnimation(base::TimeDelta duration);

   private:
    void FadeHighlightOff();

    // The containing browser view.
    BrowserView* browser_view_;

    // App model and menu.
    // Note that the menu should be destroyed before the model it uses, so the
    // menu should be listed later.
    std::unique_ptr<HostedAppMenuModel> menu_model_;
    std::unique_ptr<AppMenu> menu_;

    base::OneShotTimer highlight_off_timer_;

    DISALLOW_COPY_AND_ASSIGN(AppMenuButton);
  };

  void FadeInContentSettingButtons();

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // BrowserActionsContainer::Delegate:
  views::MenuButton* GetOverflowReferenceView() override;
  base::Optional<int> GetMaxBrowserActionsWidth() const override;
  std::unique_ptr<ToolbarActionsBar> CreateToolbarActionsBar(
      ToolbarActionsBarDelegate* delegate,
      Browser* browser,
      ToolbarActionsBar* main_bar) const override;

  // BrowserViewButtonProvider:
  BrowserActionsContainer* GetBrowserActionsContainer() override;
  views::MenuButton* GetAppMenuButton() override;

  // The containing browser view.
  BrowserView* browser_view_;

  // Button colors.
  const SkColor active_icon_color_;
  const SkColor inactive_icon_color_;

  base::OneShotTimer fade_in_content_setting_buttons_timer_;

  // Owned by the views hierarchy.
  AppMenuButton* app_menu_button_;
  ContentSettingsContainer* content_settings_container_;
  BrowserActionsContainer* browser_actions_container_;

  base::OneShotTimer opening_animation_timer_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppButtonContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_BUTTON_CONTAINER_H_
