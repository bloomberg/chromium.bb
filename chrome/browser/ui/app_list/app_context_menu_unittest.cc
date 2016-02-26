// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
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
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
const char kArcFakeAppName[] = "ArcFakeApp";
const char kArcFakeAppActivity[] = "arc.fake.app.activity";
const char kArcFakeAppPackage[] = "arc.fake.app";
#endif

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
  bool can_create_shortcuts_ = false;
  bool can_show_app_info_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeAppListControllerDelegate);
};

scoped_ptr<KeyedService> MenuManagerFactory(content::BrowserContext* context) {
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

#if defined(OS_CHROMEOS)
  void CreateArcBridge() {
    arc_bridge_service_.reset(new arc::FakeArcBridgeService());
    arc_app_instance_.reset(
        new arc::FakeAppInstance(ArcAppListPrefs::Get(profile())));
    arc::AppInstancePtr instance;
    arc_app_instance_->Bind(mojo::GetProxy(&instance));
    arc_bridge_service_->OnAppInstanceReady(std::move(instance));
    arc_app_instance_->WaitForOnAppInstanceReady();

    arc_fake_app_.name = kArcFakeAppName;
    arc_fake_app_.package_name = kArcFakeAppPackage;
    arc_fake_app_.activity = kArcFakeAppActivity;

    arc_bridge_service_->SetReady();
  }
#endif

  FakeAppListControllerDelegate* controller() {
    return controller_.get();
  }

  FakeAppContextMenuDelegate* menu_delegate() {
    return menu_delegate_.get();
  }

  Profile* profile() {
    return profile_.get();
  }

#if defined(OS_CHROMEOS)
  arc::FakeAppInstance* arc_app_instance() {
    return arc_app_instance_.get();
  }

  const arc::AppInfo& arc_fake_app() const {
    return arc_fake_app_;
  }
#endif

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
  scoped_ptr<KeyedService> menu_manager_;
  scoped_ptr<FakeAppListControllerDelegate> controller_;
  scoped_ptr<FakeAppContextMenuDelegate> menu_delegate_;
#if defined(OS_CHROMEOS)
  arc::AppInfo arc_fake_app_;
  scoped_ptr<arc::FakeArcBridgeService> arc_bridge_service_;
  scoped_ptr<arc::FakeAppInstance> arc_app_instance_;
#endif

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
  CreateArcBridge();

  const std::string app_id = ArcAppListPrefs::GetAppId(
      kArcFakeAppPackage, kArcFakeAppActivity);
  controller()->SetAppPinnable(app_id,
      AppListControllerDelegate::PIN_EDITABLE);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  std::vector<arc::AppInfo> fake_apps;
  fake_apps.push_back(arc_fake_app());
  arc_app_instance()->RefreshAppList();
  arc_app_instance()->SendRefreshAppList(fake_apps);

  ArcAppItem item(profile(), nullptr, app_id, std::string(), true);

  ui::MenuModel* menu = item.GetContextMenuModel();
  ASSERT_NE(nullptr, menu);

  ASSERT_EQ(3, menu->GetItemCount());
  EXPECT_EQ(app_list::AppContextMenu::LAUNCH_NEW, menu->GetCommandIdAt(0));
  EXPECT_EQ(true, menu->IsEnabledAt(0));
  EXPECT_EQ(false, menu->IsItemCheckedAt(0));
  EXPECT_EQ(-1, menu->GetCommandIdAt(1));  // separator
  EXPECT_EQ(app_list::AppContextMenu::TOGGLE_PIN, menu->GetCommandIdAt(2));
  EXPECT_EQ(true, menu->IsEnabledAt(2));
  EXPECT_EQ(false, menu->IsItemCheckedAt(2));

  // Test activate request.
  EXPECT_EQ(0u, arc_app_instance()->launch_requests().
      size());

  menu->ActivatedAt(0);
  arc_app_instance()->WaitForIncomingMethodCall();

  const ScopedVector<arc::FakeAppInstance::Request>& launch_requests =
      arc_app_instance()->launch_requests();
  ASSERT_EQ(1u, launch_requests.size());
  EXPECT_EQ(true, launch_requests[0]->IsForApp(arc_fake_app()));

  fake_apps.clear();
  arc_app_instance()->RefreshAppList();
  arc_app_instance()->SendRefreshAppList(fake_apps);
  item.SetReady(false);

  EXPECT_EQ(item.GetContextMenuModel(), menu);
  ASSERT_EQ(3, menu->GetItemCount());
  EXPECT_EQ(app_list::AppContextMenu::LAUNCH_NEW, menu->GetCommandIdAt(0));
  EXPECT_EQ(false, menu->IsEnabledAt(0));
  EXPECT_EQ(false, menu->IsItemCheckedAt(0));
  EXPECT_EQ(-1, menu->GetCommandIdAt(1));  // separator
  EXPECT_EQ(app_list::AppContextMenu::TOGGLE_PIN, menu->GetCommandIdAt(2));
  EXPECT_EQ(true, menu->IsEnabledAt(2));
  EXPECT_EQ(false, menu->IsItemCheckedAt(2));
}
#endif
