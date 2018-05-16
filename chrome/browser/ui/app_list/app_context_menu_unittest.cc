// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
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
#include "chrome/browser/ui/app_list/internal_app/internal_app_item.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/keyed_service/core/keyed_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

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

std::unique_ptr<ui::MenuModel> GetContextMenuModel(ChromeAppListItem* item) {
  base::RunLoop run_loop;
  std::unique_ptr<ui::MenuModel> menu;
  item->GetContextMenuModel(base::BindLambdaForTesting(
      [&](std::unique_ptr<ui::MenuModel> created_menu) {
        menu = std::move(created_menu);
        run_loop.Quit();
      }));
  run_loop.Run();
  return menu;
}

std::unique_ptr<ui::MenuModel> GetMenuModel(
    app_list::AppContextMenu* context_menu) {
  base::RunLoop run_loop;
  std::unique_ptr<ui::MenuModel> menu;
  context_menu->GetMenuModel(base::BindLambdaForTesting(
      [&](std::unique_ptr<ui::MenuModel> created_menu) {
        menu = std::move(created_menu);
        run_loop.Quit();
      }));
  run_loop.Run();
  return menu;
}

}  // namespace

class AppContextMenuTest : public AppListTestBase,
                           public testing::WithParamInterface<bool> {
 public:
  AppContextMenuTest() = default;
  ~AppContextMenuTest() override {
    // Release profile file in order to keep right sequence.
    profile_.reset();
  }

  void SetUp() override {
    AppListTestBase::SetUp();
    if (testing::UnitTest::GetInstance()->current_test_info()->value_param() &&
        GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kTouchableAppContextMenu);
      arc::IconDecodeRequest::DisableSafeDecodingForTesting();
    }
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
    // TODO(newcomer): Remove this function when touchable app context menus are
    // enabled by default.
    if (features::IsTouchableAppContextMenuEnabled())
      return;

    if (states->empty() || states->back().command_id == -1)
      return;
    states->push_back(MenuState());
  }

  void AddToStates(const app_list::ExtensionAppContextMenu& menu,
                   MenuState state,
                   std::vector<MenuState>* states) {
    // If the command is not enabled, it is not added to touchable app context
    // menus, so do not add it to states.
    if (features::IsTouchableAppContextMenuEnabled() &&
        !menu.IsCommandIdEnabled(state.command_id)) {
      return;
    }
    states->push_back(state);
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
    std::unique_ptr<ui::MenuModel> menu_model = GetMenuModel(&menu);
    ASSERT_NE(nullptr, menu_model);

    std::vector<MenuState> states;
    if (!platform_app) {
      AddToStates(menu, MenuState(app_list::AppContextMenu::LAUNCH_NEW),
                  &states);
      if (!features::IsTouchableAppContextMenuEnabled()) {
        AddSeparator(&states);

        if (extensions::util::CanHostedAppsOpenInWindows() &&
            extensions::util::IsNewBookmarkAppsEnabled()) {
          bool checked = launch_type == extensions::LAUNCH_TYPE_WINDOW ||
                         launch_type == extensions::LAUNCH_TYPE_FULLSCREEN;
          AddToStates(
              menu,
              MenuState(app_list::AppContextMenu::USE_LAUNCH_TYPE_WINDOW, true,
                        checked),
              &states);
        } else if (!extensions::util::IsNewBookmarkAppsEnabled()) {
          bool regular_checked = launch_type == extensions::LAUNCH_TYPE_REGULAR;

          AddToStates(
              menu,
              MenuState(app_list::AppContextMenu::USE_LAUNCH_TYPE_REGULAR, true,
                        regular_checked),
              &states);
          AddToStates(
              menu,
              MenuState(app_list::AppContextMenu::USE_LAUNCH_TYPE_PINNED, true,
                        launch_type == extensions::LAUNCH_TYPE_PINNED),
              &states);
          if (extensions::util::CanHostedAppsOpenInWindows()) {
            AddToStates(
                menu,
                MenuState(app_list::AppContextMenu::USE_LAUNCH_TYPE_WINDOW,
                          true, launch_type == extensions::LAUNCH_TYPE_WINDOW),
                &states);
          }
          AddToStates(
              menu,
              MenuState(app_list::AppContextMenu::USE_LAUNCH_TYPE_FULLSCREEN,
                        true,
                        launch_type == extensions::LAUNCH_TYPE_FULLSCREEN),
              &states);
        }
      }
    }
    if (pinnable != AppListControllerDelegate::NO_PIN) {
      AddSeparator(&states);
      AddToStates(
          menu,
          MenuState(app_list::AppContextMenu::TOGGLE_PIN,
                    pinnable != AppListControllerDelegate::PIN_FIXED, false),
          &states);
    }
    AddSeparator(&states);
    if (!platform_app) {
      AddToStates(menu,
                  MenuState(app_list::AppContextMenu::OPTIONS, false, false),
                  &states);
    }
    AddToStates(menu, MenuState(app_list::AppContextMenu::UNINSTALL), &states);
    if (can_show_app_info) {
      AddToStates(menu, MenuState(app_list::AppContextMenu::SHOW_APP_INFO),
                  &states);
    }

    ValidateMenuState(menu_model.get(), states);
  }

  void TestChromeApp(bool can_show_app_info) {
    controller_.reset(new FakeAppListControllerDelegate());
    controller_->SetCanShowAppInfo(can_show_app_info);
    app_list::ExtensionAppContextMenu menu(menu_delegate(),
                                           profile(),
                                           extension_misc::kChromeAppId,
                                           controller());
    std::unique_ptr<ui::MenuModel> menu_model = GetMenuModel(&menu);
    ASSERT_NE(nullptr, menu_model);

    std::vector<MenuState> states;
    AddToStates(menu, MenuState(app_list::AppContextMenu::MENU_NEW_WINDOW),
                &states);
    if (!profile()->IsOffTheRecord()) {
      AddToStates(
          menu, MenuState(app_list::AppContextMenu::MENU_NEW_INCOGNITO_WINDOW),
          &states);
    }
    if (can_show_app_info) {
      AddToStates(menu, MenuState(app_list::AppContextMenu::SHOW_APP_INFO),
                  &states);
    }
    ValidateMenuState(menu_model.get(), states);
  }

 private:
  std::unique_ptr<KeyedService> menu_manager_;
  std::unique_ptr<FakeAppListControllerDelegate> controller_;
  std::unique_ptr<FakeAppContextMenuDelegate> menu_delegate_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AppContextMenuTest);
};

INSTANTIATE_TEST_CASE_P(, AppContextMenuTest, testing::Bool());

TEST_P(AppContextMenuTest, ExtensionApp) {
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

TEST_P(AppContextMenuTest, ChromeApp) {
  app_list::ExtensionAppContextMenu::DisableInstalledExtensionCheckForTesting(
      true);
  for (bool can_show_app_info : {true, false})
    TestChromeApp(can_show_app_info);
}

TEST_P(AppContextMenuTest, NonExistingExtensionApp) {
  app_list::ExtensionAppContextMenu::DisableInstalledExtensionCheckForTesting(
      false);
  app_list::ExtensionAppContextMenu menu(menu_delegate(),
                                         profile(),
                                         "some_non_existing_extension_app",
                                         controller());
  std::unique_ptr<ui::MenuModel> menu_model = GetMenuModel(&menu);
  EXPECT_EQ(nullptr, menu_model);
}

TEST_P(AppContextMenuTest, ArcMenu) {
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  const arc::mojom::AppInfo& app_info = arc_test.fake_apps()[1];
  const std::string app_id = ArcAppTest::GetAppId(app_info);
  controller()->SetAppPinnable(app_id, AppListControllerDelegate::PIN_EDITABLE);

  arc_test.app_instance()->RefreshAppList();
  arc_test.app_instance()->SendRefreshAppList(arc_test.fake_apps());

  ArcAppItem item(profile(), nullptr, nullptr, app_id, std::string());

  std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(&item);
  ASSERT_NE(nullptr, menu);

  // Separators are not added to touchable app context menus. For touchable app
  // context menus, arc app has double separator, three more app shortcuts
  // provided by arc::FakeAppInstance and two separators between shortcuts.
  const int expected_items =
      features::IsTouchableAppContextMenuEnabled() ? 10 : 6;

  ASSERT_EQ(expected_items, menu->GetItemCount());
  int index = 0;
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::LAUNCH_NEW));
  if (!features::IsTouchableAppContextMenuEnabled())
    ValidateItemState(menu.get(), index++, MenuState());  // separator
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::TOGGLE_PIN));
  if (!features::IsTouchableAppContextMenuEnabled())
    ValidateItemState(menu.get(), index++, MenuState());  // separator
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::UNINSTALL));
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::SHOW_APP_INFO));

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
  menu = GetContextMenuModel(&item);

  // Separators are not added to touchable app context menus except for arc app
  // shortcuts, which have double separator, three more app shortcuts provided
  // by arc::FakeAppInstance and two separators between shortcuts.
  const int expected_items_app_open =
      features::IsTouchableAppContextMenuEnabled() ? 9 : 4;
  ASSERT_EQ(expected_items_app_open, menu->GetItemCount());
  index = 0;
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::TOGGLE_PIN));
  if (!features::IsTouchableAppContextMenuEnabled())
    ValidateItemState(menu.get(), index++, MenuState());  // separator
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::UNINSTALL));
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::SHOW_APP_INFO));

  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
  if (features::IsTouchableAppContextMenuEnabled()) {
    EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
    for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
      EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
                base::UTF16ToUTF8(menu->GetLabelAt(index++)));
      if (index < menu->GetItemCount())
        EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
    }

    // Test launching app shortcut item.
    EXPECT_EQ(0, arc_test.app_instance()->launch_app_shortcut_item_count());
    menu->ActivatedAt(menu->GetItemCount() - 1);
    EXPECT_EQ(1, arc_test.app_instance()->launch_app_shortcut_item_count());
  }

  // This makes all apps non-ready.
  controller()->SetAppOpen(app_id, false);
  arc::ConnectionObserver<arc::mojom::AppInstance>* connection_observer =
      arc_test.arc_app_list_prefs();
  connection_observer->OnConnectionClosed();

  menu = GetContextMenuModel(&item);

  // Separators and disabled options are not added to touchable app context
  // menus. For touchable app context menus, arc app has double separator,
  // three more app shortcuts provided by arc::FakeAppInstance and two
  // separators between shortcuts.
  const int expected_items_reopen =
      features::IsTouchableAppContextMenuEnabled() ? 8 : 6;
  ASSERT_EQ(expected_items_reopen, menu->GetItemCount());
  index = 0;
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::LAUNCH_NEW));
  if (!features::IsTouchableAppContextMenuEnabled())
    ValidateItemState(menu.get(), index++, MenuState());  // separator
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::TOGGLE_PIN));
  if (!features::IsTouchableAppContextMenuEnabled()) {
    ValidateItemState(menu.get(), index++, MenuState());  // separator
    ValidateItemState(
        menu.get(), index++,
        MenuState(app_list::AppContextMenu::UNINSTALL, false, false));
    ValidateItemState(
        menu.get(), index++,
        MenuState(app_list::AppContextMenu::SHOW_APP_INFO, false, false));
  }
  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
  if (features::IsTouchableAppContextMenuEnabled()) {
    EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
    for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
      EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
                base::UTF16ToUTF8(menu->GetLabelAt(index++)));
      if (index < menu->GetItemCount())
        EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
    }
  }

  // Uninstall all apps.
  arc_test.app_instance()->RefreshAppList();
  arc_test.app_instance()->SendRefreshAppList(
      std::vector<arc::mojom::AppInfo>());
  controller()->SetAppOpen(app_id, false);

  // No app available case.
  menu = GetContextMenuModel(&item);
  EXPECT_EQ(0, menu->GetItemCount());
}

TEST_P(AppContextMenuTest, ArcMenuShortcut) {
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  const arc::mojom::ShortcutInfo& shortcut_info = arc_test.fake_shortcuts()[0];
  const std::string app_id = ArcAppTest::GetAppId(shortcut_info);
  controller()->SetAppPinnable(app_id, AppListControllerDelegate::PIN_EDITABLE);

  arc_test.app_instance()->SendInstallShortcuts(arc_test.fake_shortcuts());

  ArcAppItem item(profile(), nullptr, nullptr, app_id, std::string());

  std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(&item);
  ASSERT_NE(nullptr, menu);
  // Separators are not added to touchable app context menus. For touchable app
  // context menus, arc app has double separator, three more app shortcuts
  // provided by arc::FakeAppInstance and two separators between shortcuts.
  const int expected_items =
      features::IsTouchableAppContextMenuEnabled() ? 10 : 6;
  int index = 0;
  ASSERT_EQ(expected_items, menu->GetItemCount());
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::LAUNCH_NEW));
  if (!features::IsTouchableAppContextMenuEnabled())
    ValidateItemState(menu.get(), index++, MenuState());  // separator
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::TOGGLE_PIN));
  if (!features::IsTouchableAppContextMenuEnabled())
    ValidateItemState(menu.get(), index++, MenuState());  // separator
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::UNINSTALL));
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::SHOW_APP_INFO));
  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
  if (features::IsTouchableAppContextMenuEnabled()) {
    EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
    for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
      EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
                base::UTF16ToUTF8(menu->GetLabelAt(index++)));
      if (index < menu->GetItemCount())
        EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
    }
  }

  // This makes all apps non-ready. Shortcut is still uninstall-able.
  arc::ConnectionObserver<arc::mojom::AppInstance>* connection_observer =
      arc_test.arc_app_list_prefs();
  connection_observer->OnConnectionClosed();

  menu = GetContextMenuModel(&item);
  // Separators and disabled options are not added to touchable app context
  // menus. For touchable app context menus, arc app has double separator,
  // three more app shortcuts provided by arc::FakeAppInstance and two
  // separators between shortcuts.
  const int expected_items_non_ready =
      features::IsTouchableAppContextMenuEnabled() ? 9 : 6;
  ASSERT_EQ(expected_items_non_ready, menu->GetItemCount());
  index = 0;
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::LAUNCH_NEW));
  if (!features::IsTouchableAppContextMenuEnabled())
    ValidateItemState(menu.get(), index++, MenuState());  // separator
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::TOGGLE_PIN));
  if (!features::IsTouchableAppContextMenuEnabled())
    ValidateItemState(menu.get(), index++, MenuState());  // separator
  ValidateItemState(menu.get(), index++,
                    MenuState(app_list::AppContextMenu::UNINSTALL));
  if (!features::IsTouchableAppContextMenuEnabled())
    ValidateItemState(
        menu.get(), index++,
        MenuState(app_list::AppContextMenu::SHOW_APP_INFO, false, false));

  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
  if (features::IsTouchableAppContextMenuEnabled()) {
    EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
    for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
      EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
                base::UTF16ToUTF8(menu->GetLabelAt(index++)));
      if (index < menu->GetItemCount())
        EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
    }
  }
}

TEST_P(AppContextMenuTest, ArcMenuStickyItem) {
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
    std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(&item);
    ASSERT_NE(nullptr, menu);

    // Separators are not added to touchable app context menus. For touchable
    // app context menus, arc app has double separator, three more app shortcuts
    // provided by arc::FakeAppInstance and two separators between shortcuts.
    int expected_items = features::IsTouchableAppContextMenuEnabled() ? 9 : 5;
    ASSERT_EQ(expected_items, menu->GetItemCount());
    int index = 0;
    ValidateItemState(menu.get(), index++,
                      MenuState(app_list::AppContextMenu::LAUNCH_NEW));
    if (!features::IsTouchableAppContextMenuEnabled())
      ValidateItemState(menu.get(), index++, MenuState());  // separator
    ValidateItemState(menu.get(), index++,
                      MenuState(app_list::AppContextMenu::TOGGLE_PIN));
    if (!features::IsTouchableAppContextMenuEnabled())
      ValidateItemState(menu.get(), index++, MenuState());  // separator
    ValidateItemState(menu.get(), index++,
                      MenuState(app_list::AppContextMenu::SHOW_APP_INFO));

    // Test that arc app shortcuts provided by arc::FakeAppInstance have a
    // separator between each app shortcut.
    if (features::IsTouchableAppContextMenuEnabled()) {
      EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
      for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
        EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
                  base::UTF16ToUTF8(menu->GetLabelAt(index++)));
        if (index < menu->GetItemCount())
          EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
      }
    }
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

// Tests that internal app's context menu is correct.
TEST_P(AppContextMenuTest, InternalAppMenu) {
  for (const auto& internal_app : app_list::GetInternalAppList()) {
    if (!internal_app.show_in_launcher)
      continue;

    controller()->SetAppPinnable(internal_app.app_id,
                                 AppListControllerDelegate::PIN_EDITABLE);
    InternalAppItem item(profile(), nullptr, internal_app);
    std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(&item);
    ASSERT_NE(nullptr, menu);
    EXPECT_EQ(1, menu->GetItemCount());
    ValidateItemState(menu.get(), 0,
                      MenuState(app_list::AppContextMenu::TOGGLE_PIN));
  }
}
