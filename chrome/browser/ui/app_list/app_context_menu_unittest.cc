// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"

#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/extension_app_context_menu.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/arc/arc_app_item.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#endif

namespace {

class FakeAppContextMenuDelegate : public app_list::AppContextMenuDelegate {
 public:
  FakeAppContextMenuDelegate() = default;
  ~FakeAppContextMenuDelegate() override = default;

  // app_list::AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAppContextMenuDelegate);
};

class FakeAppListControllerDelegate :
    public test::TestAppListControllerDelegate {
 public:
  FakeAppListControllerDelegate() = default;
  ~FakeAppListControllerDelegate() override = default;

  void SetAppPinnable(const std::string& app_id, Pinnable type) {
    pinnable_apps_[app_id] = type;
  }

  void SetAppOpen(const std::string& app_id, bool open) {
    if (open)
      open_apps_.insert(app_id);
    else
      open_apps_.erase(app_id);
  }

  bool IsAppOpen(const std::string& app_id) const override {
    return open_apps_.count(app_id) != 0;
  }

  void SetCanCreateShortcuts(bool can_create_shortcuts) {
    can_create_shortcuts_ = can_create_shortcuts;
  }

  void SetCanShowAppInfo(bool can_show_app_info) {
    can_show_app_info_ = can_show_app_info;
  }

  // test::TestAppListControllerDelegate overrides:
  Pinnable GetPinnable(const std::string& app_id) override {
    std::map<std::string, Pinnable>::const_iterator it;
    it = pinnable_apps_.find(app_id);
    if (it == pinnable_apps_.end())
      return NO_PIN;
    return it->second;
  }
  bool CanDoCreateShortcutsFlow() override { return can_create_shortcuts_; }
  bool CanDoShowAppInfoFlow() override { return can_show_app_info_; }

 private:
  std::map<std::string, Pinnable> pinnable_apps_;
  std::unordered_set<std::string> open_apps_;
  bool can_create_shortcuts_ = false;
  bool can_show_app_info_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeAppListControllerDelegate);
};

std::unique_ptr<KeyedService> MenuManagerFactory(
    content::BrowserContext* context) {
  return extensions::MenuManagerFactory::BuildServiceInstanceForTesting(
              context);
}

}  // namespace

class AppContextMenuTest : public AppListTestBase {
 public:
  AppContextMenuTest() = default;
  ~AppContextMenuTest() override {
    // Release profile file in order to keep right sequence.
    profile_.reset();
  }

  void SetUp() override {
    AppListTestBase::SetUp();

    extensions::MenuManagerFactory::GetInstance()->SetTestingFactory(
        profile(), MenuManagerFactory);
    controller_.reset(new FakeAppListControllerDelegate());
    menu_delegate_.reset(new FakeAppContextMenuDelegate());
    ChromeAppListItem::OverrideAppListControllerDelegateForTesting(
        controller());
  }

  void TearDown() override {
    menu_delegate_.reset();
    controller_.reset();
    menu_manager_.reset();
  }

 protected:
  struct MenuState {
    // Defines separator.
    MenuState() : command_id(-1), is_enabled(true), is_checked(false) {
    }

    // Defines enabled unchecked command.
    explicit MenuState(int command_id)
        : command_id(command_id),
          is_enabled(true),
          is_checked(false) {
    }

    MenuState(int command_id, bool enabled, bool checked)
        : command_id(command_id),
          is_enabled(enabled),
          is_checked(checked) {
    }

    int command_id;
    bool is_enabled;
    bool is_checked;
  };

  void ValidateMenuState(const ui::MenuModel* menu_model,
                         const std::vector<MenuState>& states) {
    ASSERT_NE(nullptr, menu_model);
    size_t state_index = 0;
    for (int i = 0; i < menu_model->GetItemCount(); ++i) {
      ASSERT_LT(state_index, states.size());
      const MenuState& state = states[state_index++];
      EXPECT_EQ(state.command_id, menu_model->GetCommandIdAt(i));
      if (state.command_id == -1)
        continue;   // Don't check separator.
      EXPECT_EQ(state.is_enabled, menu_model->IsEnabledAt(i));
      EXPECT_EQ(state.is_checked, menu_model->IsItemCheckedAt(i));
    }
    EXPECT_EQ(state_index, states.size());
  }

  FakeAppListControllerDelegate* controller() {
    return controller_.get();
  }

  FakeAppContextMenuDelegate* menu_delegate() {
    return menu_delegate_.get();
  }

  Profile* profile() {
    return profile_.get();
  }

  void AddSeparator(std::vector<MenuState>& states) {
    if (states.empty() || states.back().command_id == -1)
      return;
    states.push_back(MenuState());
  }

  void TestExtensionApp(const std::string& app_id,
                        bool platform_app,
                        bool can_create_shortcuts,
                        bool can_show_app_info,
                        AppListControllerDelegate::Pinnable pinnable,
                        extensions::LaunchType launch_type) {
    controller_.reset(new FakeAppListControllerDelegate());
    controller_->SetAppPinnable(app_id, pinnable);
    controller_->SetCanCreateShortcuts(can_create_shortcuts);
    controller_->SetCanShowAppInfo(can_show_app_info);
    controller_->SetExtensionLaunchType(profile(), app_id, launch_type);
    app_list::ExtensionAppContextMenu menu(menu_delegate(),
                                           profile(),
                                           app_id,
                                           controller());
    menu.set_is_platform_app(platform_app);
    ui::MenuModel* menu_model = menu.GetMenuModel();
    ASSERT_NE(nullptr, menu_model);

    std::vector<MenuState> states;
    if (!platform_app)
      states.push_back(MenuState(app_list::AppContextMenu::LAUNCH_NEW));
    if (pinnable != AppListControllerDelegate::NO_PIN) {
      AddSeparator(states);
      states.push_back(MenuState(
          app_list::AppContextMenu::TOGGLE_PIN,
          pinnable != AppListControllerDelegate::PIN_FIXED,
          false));
    }
    if (can_create_shortcuts)
      states.push_back(MenuState(app_list::AppContextMenu::CREATE_SHORTCUTS));
    AddSeparator(states);

    if (!platform_app) {
      if (extensions::util::CanHostedAppsOpenInWindows() &&
          extensions::util::IsNewBookmarkAppsEnabled()) {
        // MACOS has different logic for this case.
        // See chrome/browser/extensions/launch_util.cc for more details.
        bool checked = launch_type == extensions::LAUNCH_TYPE_WINDOW;
#if !defined(OS_MACOSX)
        checked |= launch_type == extensions::LAUNCH_TYPE_FULLSCREEN;
#endif
        states.push_back(MenuState(
            app_list::AppContextMenu::USE_LAUNCH_TYPE_WINDOW, true, checked));
      } else if (!extensions::util::IsNewBookmarkAppsEnabled()) {
        bool regular_checked = launch_type == extensions::LAUNCH_TYPE_REGULAR;
#if defined(OS_MACOSX)
        regular_checked |= (!extensions::util::CanHostedAppsOpenInWindows() &&
            launch_type == extensions::LAUNCH_TYPE_WINDOW);
#endif
        states.push_back(MenuState(
            app_list::AppContextMenu::USE_LAUNCH_TYPE_REGULAR,
            true,
            regular_checked));
        states.push_back(MenuState(
            app_list::AppContextMenu::USE_LAUNCH_TYPE_PINNED,
            true,
            launch_type == extensions::LAUNCH_TYPE_PINNED));
        if (extensions::util::CanHostedAppsOpenInWindows()) {
          states.push_back(MenuState(
              app_list::AppContextMenu::USE_LAUNCH_TYPE_WINDOW,
              true,
              launch_type == extensions::LAUNCH_TYPE_WINDOW));
        }
        states.push_back(MenuState(
            app_list::AppContextMenu::USE_LAUNCH_TYPE_FULLSCREEN,
            true,
            launch_type == extensions::LAUNCH_TYPE_FULLSCREEN));
      }
      AddSeparator(states);
      states.push_back(MenuState(app_list::AppContextMenu::OPTIONS,
                                 false,
                                 false));
    }
    states.push_back(MenuState(app_list::AppContextMenu::UNINSTALL));
    if (can_show_app_info)
      states.push_back(MenuState(app_list::AppContextMenu::SHOW_APP_INFO));

    ValidateMenuState(menu_model, states);
  }

  void TestChromeApp(bool can_create_shortcuts,
                     bool can_show_app_info) {
    controller_.reset(new FakeAppListControllerDelegate());
    controller_->SetCanCreateShortcuts(can_create_shortcuts);
    controller_->SetCanShowAppInfo(can_show_app_info);
    app_list::ExtensionAppContextMenu menu(menu_delegate(),
                                           profile(),
                                           extension_misc::kChromeAppId,
                                           controller());
    ui::MenuModel* menu_model = menu.GetMenuModel();
    ASSERT_NE(nullptr, menu_model);

    std::vector<MenuState> states;
    states.push_back(MenuState(app_list::AppContextMenu::MENU_NEW_WINDOW));
    if (!profile()->IsOffTheRecord()) {
      states.push_back(MenuState(
          app_list::AppContextMenu::MENU_NEW_INCOGNITO_WINDOW));
    }
    if (can_show_app_info)
      states.push_back(MenuState(app_list::AppContextMenu::SHOW_APP_INFO));

    ValidateMenuState(menu_model, states);
  }

 private:
  std::unique_ptr<KeyedService> menu_manager_;
  std::unique_ptr<FakeAppListControllerDelegate> controller_;
  std::unique_ptr<FakeAppContextMenuDelegate> menu_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppContextMenuTest);
};

TEST_F(AppContextMenuTest, ExtensionApp) {
  app_list::ExtensionAppContextMenu::DisableInstalledExtensionCheckForTesting(
      false);
  for (extensions::LaunchType launch_type = extensions::LAUNCH_TYPE_FIRST;
      launch_type < extensions::NUM_LAUNCH_TYPES;
      launch_type = static_cast<extensions::LaunchType>(launch_type+1)) {
    AppListControllerDelegate::Pinnable pinnable;
    for (pinnable = AppListControllerDelegate::NO_PIN;
        pinnable <= AppListControllerDelegate::PIN_FIXED;
        pinnable =
            static_cast<AppListControllerDelegate::Pinnable>(pinnable+1)) {
      for (size_t combinations = 0; combinations < (1 << 3); ++combinations) {
        TestExtensionApp(AppListTestBase::kHostedAppId,
                         (combinations & (1 << 0)) != 0,
                         (combinations & (1 << 1)) != 0,
                         (combinations & (1 << 2)) != 0,
                         pinnable,
                         launch_type);
        TestExtensionApp(AppListTestBase::kPackagedApp1Id,
                         (combinations & (1 << 0)) != 0,
                         (combinations & (1 << 1)) != 0,
                         (combinations & (1 << 2)) != 0,
                         pinnable,
                         launch_type);
        TestExtensionApp(AppListTestBase::kPackagedApp2Id,
                         (combinations & (1 << 0)) != 0,
                         (combinations & (1 << 1)) != 0,
                         (combinations & (1 << 2)) != 0,
                         pinnable,
                         launch_type);
      }
    }
  }
}

TEST_F(AppContextMenuTest, ChromeApp) {
  app_list::ExtensionAppContextMenu::DisableInstalledExtensionCheckForTesting(
      true);
  for (size_t combinations = 0; combinations < (1 << 2); ++combinations) {
    TestChromeApp((combinations & (1 << 0)) != 0,
                  (combinations & (1 << 1)) != 0);
  }
}

TEST_F(AppContextMenuTest, NonExistingExtensionApp) {
  app_list::ExtensionAppContextMenu::DisableInstalledExtensionCheckForTesting(
      false);
  app_list::ExtensionAppContextMenu menu(menu_delegate(),
                                         profile(),
                                         "some_non_existing_extension_app",
                                         controller());
  ui::MenuModel* menu_model = menu.GetMenuModel();
  EXPECT_EQ(nullptr, menu_model);
}

#if defined(OS_CHROMEOS)
TEST_F(AppContextMenuTest, ArcMenu) {
  ArcAppTest arc_test;
  arc_test.SetUp(profile());
  arc_test.bridge_service()->SetReady();

  const arc::mojom::AppInfo& app_info = arc_test.fake_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app_info);
  controller()->SetAppPinnable(app_id, AppListControllerDelegate::PIN_EDITABLE);

  arc_test.app_instance()->RefreshAppList();
  arc_test.app_instance()->SendRefreshAppList(arc_test.fake_apps());

  ArcAppItem item(profile(), nullptr, app_id, std::string(), true);

  ui::MenuModel* menu = item.GetContextMenuModel();
  ASSERT_NE(nullptr, menu);

  ASSERT_EQ(3, menu->GetItemCount());
  EXPECT_EQ(app_list::AppContextMenu::LAUNCH_NEW, menu->GetCommandIdAt(0));
  EXPECT_TRUE(menu->IsEnabledAt(0));
  EXPECT_FALSE(menu->IsItemCheckedAt(0));
  EXPECT_EQ(-1, menu->GetCommandIdAt(1));  // separator
  EXPECT_EQ(app_list::AppContextMenu::TOGGLE_PIN, menu->GetCommandIdAt(2));
  EXPECT_TRUE(menu->IsEnabledAt(2));
  EXPECT_FALSE(menu->IsItemCheckedAt(2));

  // Test activate request.
  EXPECT_EQ(0u, arc_test.app_instance()->launch_requests().size());

  menu->ActivatedAt(0);
  arc_test.app_instance()->WaitForIncomingMethodCall();

  const ScopedVector<arc::FakeAppInstance::Request>& launch_requests =
      arc_test.app_instance()->launch_requests();
  ASSERT_EQ(1u, launch_requests.size());
  EXPECT_TRUE(launch_requests[0]->IsForApp(app_info));

  controller()->SetAppOpen(app_id, true);
  // It is not expected that menu model is unchanged on GetContextMenuModel. Arc
  // app menu requires model to be recalculated.
  menu = item.GetContextMenuModel();
  ASSERT_EQ(1, menu->GetItemCount());
  EXPECT_EQ(app_list::AppContextMenu::TOGGLE_PIN, menu->GetCommandIdAt(0));
  EXPECT_TRUE(menu->IsEnabledAt(0));
  EXPECT_FALSE(menu->IsItemCheckedAt(0));

  arc_test.app_instance()->RefreshAppList();
  arc_test.app_instance()->SendRefreshAppList(
      std::vector<arc::mojom::AppInfo>());
  item.SetReady(false);
  controller()->SetAppOpen(app_id, false);

  menu = item.GetContextMenuModel();
  ASSERT_EQ(3, menu->GetItemCount());
  EXPECT_EQ(app_list::AppContextMenu::LAUNCH_NEW, menu->GetCommandIdAt(0));
  EXPECT_FALSE(menu->IsEnabledAt(0));
  EXPECT_FALSE(menu->IsItemCheckedAt(0));
  EXPECT_EQ(-1, menu->GetCommandIdAt(1));  // separator
  EXPECT_EQ(app_list::AppContextMenu::TOGGLE_PIN, menu->GetCommandIdAt(2));
  EXPECT_TRUE(menu->IsEnabledAt(2));
  EXPECT_FALSE(menu->IsItemCheckedAt(2));
}

TEST_F(AppContextMenuTest, ArcMenuStickyItem) {
  ArcAppTest arc_test;
  arc_test.SetUp(profile());
  arc_test.bridge_service()->SetReady();

  arc_test.app_instance()->RefreshAppList();
  arc_test.app_instance()->SendRefreshAppList(arc_test.fake_apps());

  {
    // Verify menu of store
    const arc::mojom::AppInfo& store_info = arc_test.fake_apps()[0];
    const std::string store_id = ArcAppTest::GetAppId(store_info);
    controller()->SetAppPinnable(store_id,
                                 AppListControllerDelegate::PIN_EDITABLE);
    ArcAppItem item(profile(), nullptr, store_id, std::string(), true);
    ui::MenuModel* menu = item.GetContextMenuModel();
    ASSERT_NE(nullptr, menu);

    ASSERT_EQ(3, menu->GetItemCount());
    EXPECT_EQ(app_list::AppContextMenu::LAUNCH_NEW, menu->GetCommandIdAt(0));
    EXPECT_TRUE(menu->IsEnabledAt(0));
    EXPECT_FALSE(menu->IsItemCheckedAt(0));
    EXPECT_EQ(-1, menu->GetCommandIdAt(1));  // separator
    EXPECT_EQ(app_list::AppContextMenu::TOGGLE_PIN, menu->GetCommandIdAt(2));
    EXPECT_TRUE(menu->IsEnabledAt(2));
    EXPECT_FALSE(menu->IsItemCheckedAt(2));
    // No "uninstall" entry.
  }

  {
    // Verify normal app menu
    const arc::mojom::AppInfo& app_info = arc_test.fake_apps()[1];
    const std::string app_id = ArcAppTest::GetAppId(app_info);
    controller()->SetAppPinnable(app_id,
                                 AppListControllerDelegate::PIN_EDITABLE);
    ArcAppItem item(profile(), nullptr, app_id, std::string(), true);
    ui::MenuModel* menu = item.GetContextMenuModel();
    ASSERT_NE(nullptr, menu);

    ASSERT_EQ(5, menu->GetItemCount());
    EXPECT_EQ(app_list::AppContextMenu::LAUNCH_NEW, menu->GetCommandIdAt(0));
    EXPECT_TRUE(menu->IsEnabledAt(0));
    EXPECT_FALSE(menu->IsItemCheckedAt(0));
    EXPECT_EQ(-1, menu->GetCommandIdAt(1));  // separator
    EXPECT_EQ(app_list::AppContextMenu::TOGGLE_PIN, menu->GetCommandIdAt(2));
    EXPECT_TRUE(menu->IsEnabledAt(2));
    EXPECT_FALSE(menu->IsItemCheckedAt(2));
    EXPECT_EQ(-1, menu->GetCommandIdAt(3));  // separator
    EXPECT_EQ(app_list::AppContextMenu::UNINSTALL, menu->GetCommandIdAt(4));
    EXPECT_TRUE(menu->IsEnabledAt(4));
    EXPECT_FALSE(menu->IsItemCheckedAt(4));
  }
}
#endif
