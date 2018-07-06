// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_CONTEXTUAL_MENU_H_
#define CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_CONTEXTUAL_MENU_H_

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;

// The class for the contextual menu for the Media Router action.
class MediaRouterContextualMenu : public ui::SimpleMenuModel::Delegate {
 public:
  class Observer {
   public:
    virtual void OnContextMenuShown() = 0;
    virtual void OnContextMenuHidden() = 0;
  };

  // Creates an instance for a Media Router Action shown in the toolbar.
  // |observer| must outlive the context menu.
  static std::unique_ptr<MediaRouterContextualMenu> CreateForToolbar(
      Browser* browser,
      Observer* observer);

  // Creates an instance for a Media Router Action shown in the overflow menu.
  // |observer| must outlive the context menu.
  static std::unique_ptr<MediaRouterContextualMenu> CreateForOverflowMenu(
      Browser* browser,
      Observer* observer);

  // Constructor called by the static Create* methods above and tests.
  MediaRouterContextualMenu(Browser* browser,
                            bool is_action_in_toolbar,
                            bool shown_by_policy,
                            Observer* observer);
  ~MediaRouterContextualMenu() override;

  ui::SimpleMenuModel* menu_model() { return &menu_model_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaRouterContextualMenuUnitTest,
                           ToggleCloudServicesItem);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterContextualMenuUnitTest,
                           ShowCloudServicesDialog);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterContextualMenuUnitTest,
                           ToggleAlwaysShowIconItem);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterContextualMenuUnitTest,
                           ActionShownByPolicy);

  // Gets or sets the "Always show icon" option.
  bool GetAlwaysShowActionPref() const;
  void SetAlwaysShowActionPref(bool always_show);

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // Toggles the enabled/disabled state of cloud services. This may show a
  // dialog asking the user to acknowledge the Google Privacy Policy before
  // enabling the services.
  void ToggleCloudServices();

  // Opens feedback page loaded from the media router extension.
  void ReportIssue();

  // Gets the ID corresponding to the text for the menu option to change the
  // visibility of the action (e.g. "Hide in Chrome menu" / "Show in toolbar")
  // depending on the location of the action.
  int GetChangeVisibilityTextId();

  Observer* const observer_;

  Browser* const browser_;
  ui::SimpleMenuModel menu_model_;

  // Whether the action icon is in the toolbar, as opposed to the overflow menu.
  const bool is_action_in_toolbar_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterContextualMenu);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_CONTEXTUAL_MENU_H_
