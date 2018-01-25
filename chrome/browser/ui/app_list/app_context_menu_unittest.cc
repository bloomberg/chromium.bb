// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_item.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/extension_app_context_menu.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/keyed_service/core/keyed_service.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  bool CanDoShowAppInfoFlow() override { return can_show_app_info_; }

 private:
  std::map<std::string, Pinnable> pinnable_apps_;
  std::unordered_set<std::string> open_apps_;
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
    controller_ = std::make_unique<FakeAppListControllerDelegate>();
    menu_delegate_ = std::make_unique<FakeAppContextMenuDelegate>();
    model_updater_ = std::make_unique<FakeAppListModelUpdater>();
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

  void ValidateItemState(const ui::MenuModel* menu_model,
                         int index,
                         const MenuState& state) {
    EXPECT_EQ(state.command_id, menu_model->GetCommandIdAt(index));
    if (state.command_id == -1)
      return;   // Don't check separator.
    EXPECT_EQ(state.is_enabled, menu_model->IsEnabledAt(index));
    EXPECT_EQ(state.is_checked, menu_model->IsItemCheckedAt(index));
  }

  void ValidateMenuState(const ui::MenuModel* menu_model,
                         const std::vector<MenuState>& states) {
    ASSERT_NE(nullptr, menu_model);
    size_t state_index = 0;
    for (int i = 0; i < menu_model->GetItemCount(); ++i) {
      ASSERT_LT(state_index, states.size());
      ValidateItemState(menu_model, i, states[state_index++]);
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

  void AddSeparator(std::vector<MenuState>* states) {
    if (states->empty() || states->back().command_id == -1)
      return;
    states->push_back(MenuState());
  }

  void TestExtensionApp(const std::string& app_id,
                        bool platform_app,
                        bool can_show_app_info,
                        AppListControllerDelegate::Pinnable pinnable,
                        extensions::LaunchType launch_type) {
    controller_.reset(new FakeAppListControllerDelegate());
    controller_->SetAppPinnable(app_id, pinnable);
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
      AddSeparator(&states);
      states.push_back(MenuState(
          app_list::AppContextMenu::TOGGLE_PIN,
          pinnable != AppListControllerDelegate::PIN_FIXED,
          false));
    }
    AddSeparator(&states);

    if (!platform_app) {
      if (extensions::util::CanHostedAppsOpenInWindows() &&
          extensions::util::IsNewBookmarkAppsEnabled()) {
        bool checked = launch_type == extensions::LAUNCH_TYPE_WINDOW ||
            launch_type == extensions::LAUNCH_TYPE_FULLSCREEN;
        states.push_back(MenuState(
            app_list::AppContextMenu::USE_LAUNCH_TYPE_WINDOW, true, checked));
      } else if (!extensions::util::IsNewBookmarkAppsEnabled()) {
        bool regular_checked = launch_type == extensions::LAUNCH_TYPE_REGULAR;
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
      AddSeparator(&states);
      states.push_back(MenuState(app_list::AppContextMenu::OPTIONS,
                                 false,
                                 false));
    }
    states.push_back(MenuState(app_list::AppContextMenu::UNINSTALL));
    if (can_show_app_info)
      states.push_back(MenuState(app_list::AppContextMenu::SHOW_APP_INFO));

    ValidateMenuState(menu_model, states);
  }

  void TestChromeApp(bool can_show_app_info) {
    controller_.reset(new FakeAppListControllerDelegate());
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
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;

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
      for (size_t combinations = 0; combinations < (1 << 2); ++combinations) {
        TestExtensionApp(AppListTestBase::kHostedAppId,
                         (combinations & (1 << 0)) != 0,
                         (combinations & (1 << 1)) != 0,
                         pinnable,
                         launch_type);
        TestExtensionApp(AppListTestBase::kPackagedApp1Id,
                         (combinations & (1 << 0)) != 0,
                         (combinations & (1 << 1)) != 0,
                         pinnable,
                         launch_type);
        TestExtensionApp(AppListTestBase::kPackagedApp2Id,
                         (combinations & (1 << 0)) != 0,
                         (combinations & (1 << 1)) != 0,
                         pinnable,
                         launch_type);
      }
    }
  }
}

TEST_F(AppContextMenuTest, ChromeApp) {
  app_list::ExtensionAppContextMenu::DisableInstalledExtensionCheckForTesting(
      true);
  for (bool can_show_app_info : {true, false})
    TestChromeApp(can_show_app_info);
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

TEST_F(AppContextMenuTest, ArcMenu) {
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  const arc::mojom::AppInfo& app_info = arc_test.fake_apps()[1];
  const std::string app_id = ArcAppTest::GetAppId(app_info);
  controller()->SetAppPinnable(app_id, AppListControllerDelegate::PIN_EDITABLE);

  arc_test.app_instance()->RefreshAppList();
  arc_test.app_instance()->SendRefreshAppList(arc_test.fake_apps());

  ArcAppItem item(profile(), nullptr, nullptr, app_id, std::string());

  ui::MenuModel* menu = item.GetContextMenuModel();
  ASSERT_NE(nullptr, menu);

  ASSERT_EQ(6, menu->GetItemCount());
  ValidateItemState(menu, 0, MenuState(app_list::AppContextMenu::LAUNCH_NEW));
  ValidateItemState(menu, 1, MenuState());  // separator
  ValidateItemState(menu, 2, MenuState(app_list::AppContextMenu::TOGGLE_PIN));
  ValidateItemState(menu, 3, MenuState());  // separator
  ValidateItemState(menu, 4, MenuState(app_list::AppContextMenu::UNINSTALL));
  ValidateItemState(
      menu, 5, MenuState(app_list::AppContextMenu::SHOW_APP_INFO));

  // Test activate request.
  EXPECT_EQ(0u, arc_test.app_instance()->launch_requests().size());

  menu->ActivatedAt(0);

  const std::vector<std::unique_ptr<arc::FakeAppInstance::Request>>&
      launch_requests = arc_test.app_instance()->launch_requests();
  ASSERT_EQ(1u, launch_requests.size());
  EXPECT_TRUE(launch_requests[0]->IsForApp(app_info));

  controller()->SetAppOpen(app_id, true);
  // It is not expected that menu model is unchanged on GetContextMenuModel.
  // ARC app menu requires model to be recalculated.
  menu = item.GetContextMenuModel();
  ASSERT_EQ(4, menu->GetItemCount());
  ValidateItemState(menu, 0, MenuState(app_list::AppContextMenu::TOGGLE_PIN));
  ValidateItemState(menu, 1, MenuState());  // separator
  ValidateItemState(menu, 2, MenuState(app_list::AppContextMenu::UNINSTALL));
  ValidateItemState(
      menu, 3, MenuState(app_list::AppContextMenu::SHOW_APP_INFO));

  // This makes all apps non-ready.
  controller()->SetAppOpen(app_id, false);
  arc::ConnectionObserver<arc::mojom::AppInstance>* connection_observer =
      arc_test.arc_app_list_prefs();
  connection_observer->OnConnectionClosed();

  menu = item.GetContextMenuModel();
  ASSERT_EQ(6, menu->GetItemCount());
  ValidateItemState(menu, 0, MenuState(app_list::AppContextMenu::LAUNCH_NEW));
  ValidateItemState(menu, 1, MenuState());  // separator
  ValidateItemState(menu, 2, MenuState(app_list::AppContextMenu::TOGGLE_PIN));
  ValidateItemState(menu, 3, MenuState());  // separator
  ValidateItemState(
      menu, 4, MenuState(app_list::AppContextMenu::UNINSTALL, false, false));
  ValidateItemState(
      menu, 5,
      MenuState(app_list::AppContextMenu::SHOW_APP_INFO, false, false));

  // Uninstall all apps.
  arc_test.app_instance()->RefreshAppList();
  arc_test.app_instance()->SendRefreshAppList(
      std::vector<arc::mojom::AppInfo>());
  controller()->SetAppOpen(app_id, false);

  // No app available case.
  menu = item.GetContextMenuModel();
  EXPECT_EQ(0, menu->GetItemCount());
}


TEST_F(AppContextMenuTest, ArcMenuShortcut) {
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  const arc::mojom::ShortcutInfo& shortcut_info = arc_test.fake_shortcuts()[0];
  const std::string app_id = ArcAppTest::GetAppId(shortcut_info);
  controller()->SetAppPinnable(app_id, AppListControllerDelegate::PIN_EDITABLE);

  arc_test.app_instance()->SendInstallShortcuts(arc_test.fake_shortcuts());

  ArcAppItem item(profile(), nullptr, nullptr, app_id, std::string());

  ui::MenuModel* menu = item.GetContextMenuModel();
  ASSERT_NE(nullptr, menu);

  ASSERT_EQ(6, menu->GetItemCount());
  ValidateItemState(menu, 0, MenuState(app_list::AppContextMenu::LAUNCH_NEW));
  ValidateItemState(menu, 1, MenuState());  // separator
  ValidateItemState(menu, 2, MenuState(app_list::AppContextMenu::TOGGLE_PIN));
  ValidateItemState(menu, 3, MenuState());  // separator
  ValidateItemState(menu, 4, MenuState(app_list::AppContextMenu::UNINSTALL));
  ValidateItemState(
      menu, 5, MenuState(app_list::AppContextMenu::SHOW_APP_INFO));

  // This makes all apps non-ready. Shortcut is still uninstall-able.
  arc::ConnectionObserver<arc::mojom::AppInstance>* connection_observer =
      arc_test.arc_app_list_prefs();
  connection_observer->OnConnectionClosed();

  menu = item.GetContextMenuModel();
  ASSERT_EQ(6, menu->GetItemCount());
  ValidateItemState(menu, 0, MenuState(app_list::AppContextMenu::LAUNCH_NEW));
  ValidateItemState(menu, 1, MenuState());  // separator
  ValidateItemState(menu, 2, MenuState(app_list::AppContextMenu::TOGGLE_PIN));
  ValidateItemState(menu, 3, MenuState());  // separator
  ValidateItemState(menu, 4, MenuState(app_list::AppContextMenu::UNINSTALL));
  ValidateItemState(
      menu, 5,
      MenuState(app_list::AppContextMenu::SHOW_APP_INFO, false, false));
}

TEST_F(AppContextMenuTest, ArcMenuStickyItem) {
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  arc_test.app_instance()->RefreshAppList();
  arc_test.app_instance()->SendRefreshAppList(arc_test.fake_apps());

  {
    // Verify menu of store
    const arc::mojom::AppInfo& store_info = arc_test.fake_apps()[0];
    const std::string store_id = ArcAppTest::GetAppId(store_info);
    controller()->SetAppPinnable(store_id,
                                 AppListControllerDelegate::PIN_EDITABLE);
    ArcAppItem item(profile(), nullptr, nullptr, store_id, std::string());
    ui::MenuModel* menu = item.GetContextMenuModel();
    ASSERT_NE(nullptr, menu);

    // There should be 5 items in the context menu. If you're adding a
    // context menu option ensure that you have added the enum to
    // tools/metrics/enums.xml and that you haven't modified the order of the
    // existing enums.
    ASSERT_EQ(5, menu->GetItemCount());
    ValidateItemState(menu, 0, MenuState(app_list::AppContextMenu::LAUNCH_NEW));
    ValidateItemState(menu, 1, MenuState());  // separator
    ValidateItemState(menu, 2, MenuState(app_list::AppContextMenu::TOGGLE_PIN));
    ValidateItemState(menu, 3, MenuState());  // separator
    ValidateItemState(
        menu, 4, MenuState(app_list::AppContextMenu::SHOW_APP_INFO));
  }
}

TEST_F(AppContextMenuTest, CommandIdsMatchEnumsForHistograms) {
  // Tests that CommandId enums are not changed as the values are used in
  // histograms.
  EXPECT_EQ(100, app_list::AppContextMenu::LAUNCH_NEW);
  EXPECT_EQ(101, app_list::AppContextMenu::TOGGLE_PIN);
  EXPECT_EQ(102, app_list::AppContextMenu::SHOW_APP_INFO);
  EXPECT_EQ(103, app_list::AppContextMenu::OPTIONS);
  EXPECT_EQ(104, app_list::AppContextMenu::UNINSTALL);
  EXPECT_EQ(105, app_list::AppContextMenu::REMOVE_FROM_FOLDER);
  EXPECT_EQ(106, app_list::AppContextMenu::MENU_NEW_WINDOW);
  EXPECT_EQ(107, app_list::AppContextMenu::MENU_NEW_INCOGNITO_WINDOW);
  EXPECT_EQ(108, app_list::AppContextMenu::INSTALL);
  EXPECT_EQ(200, app_list::AppContextMenu::USE_LAUNCH_TYPE_COMMAND_START);
  EXPECT_EQ(200, app_list::AppContextMenu::USE_LAUNCH_TYPE_PINNED);
  EXPECT_EQ(201, app_list::AppContextMenu::USE_LAUNCH_TYPE_REGULAR);
  EXPECT_EQ(202, app_list::AppContextMenu::USE_LAUNCH_TYPE_FULLSCREEN);
  EXPECT_EQ(203, app_list::AppContextMenu::USE_LAUNCH_TYPE_WINDOW);
}
