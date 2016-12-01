// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_impl.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/common/ash_switches.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_controller.h"
#include "ash/common/shelf/shelf_item_types.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_model_observer.h"
#include "ash/common/test/test_session_state_delegate.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_deferred_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/browser_status_monitor.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/extension_app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/exo/shell_surface.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/app_window/app_window_contents.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/base/models/menu_model.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;
using extensions::Extension;
using extensions::Manifest;
using extensions::UnloadedExtensionInfo;
using arc::mojom::OrientationLock;

namespace {
const char* offline_gmail_url = "https://mail.google.com/mail/mu/u";
const char* gmail_url = "https://mail.google.com/mail/u";
const char* kGmailLaunchURL = "https://mail.google.com/mail/ca";

// An extension prefix.
const char kCrxAppPrefix[] = "_crx_";

// Dummy app id is used to put at least one pin record to prevent initializing
// pin model with default apps that can affect some tests.
const char kDummyAppId[] = "dummyappid_dummyappid_dummyappid";

// ShelfModelObserver implementation that tracks what messages are invoked.
class TestShelfModelObserver : public ash::ShelfModelObserver {
 public:
  TestShelfModelObserver() {}
  ~TestShelfModelObserver() override {}

  // Overridden from ash::ShelfModelObserver:
  void ShelfItemAdded(int index) override {
    ++added_;
    last_index_ = index;
  }

  void ShelfItemRemoved(int index, ash::ShelfID id) override {
    ++removed_;
    last_index_ = index;
  }

  void ShelfItemChanged(int index, const ash::ShelfItem& old_item) override {
    ++changed_;
    last_index_ = index;
  }

  void ShelfItemMoved(int start_index, int target_index) override {
    last_index_ = target_index;
  }

  void OnSetShelfItemDelegate(ash::ShelfID id,
                              ash::ShelfItemDelegate* item_delegate) override {}

  void clear_counts() {
    added_ = 0;
    removed_ = 0;
    changed_ = 0;
    last_index_ = 0;
  }

  int added() const { return added_; }
  int removed() const { return removed_; }
  int changed() const { return changed_; }
  int last_index() const { return last_index_; }

 private:
  int added_ = 0;
  int removed_ = 0;
  int changed_ = 0;
  int last_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestShelfModelObserver);
};

// Test implementation of AppIconLoader.
class TestAppIconLoaderImpl : public AppIconLoader {
 public:
  TestAppIconLoaderImpl() = default;
  ~TestAppIconLoaderImpl() override = default;

  void AddSupportedApp(const std::string& id) { supported_apps_.insert(id); }

  // AppIconLoader implementation:
  bool CanLoadImageForApp(const std::string& id) override {
    return supported_apps_.find(id) != supported_apps_.end();
  }
  void FetchImage(const std::string& id) override { ++fetch_count_; }
  void ClearImage(const std::string& id) override { ++clear_count_; }
  void UpdateImage(const std::string& id) override {}

  int fetch_count() const { return fetch_count_; }
  int clear_count() const { return clear_count_; }

 private:
  int fetch_count_ = 0;
  int clear_count_ = 0;
  std::set<std::string> supported_apps_;

  DISALLOW_COPY_AND_ASSIGN(TestAppIconLoaderImpl);
};

// Test implementation of LauncherControllerHelper.
class TestLauncherControllerHelper : public LauncherControllerHelper {
 public:
  TestLauncherControllerHelper() : LauncherControllerHelper(nullptr) {}
  explicit TestLauncherControllerHelper(Profile* profile)
      : LauncherControllerHelper(profile) {}
  ~TestLauncherControllerHelper() override {}

  // Sets the id for the specified tab.
  void SetAppID(content::WebContents* tab, const std::string& id) {
    tab_id_map_[tab] = id;
  }

  // Returns true if there is an id registered for |tab|.
  bool HasAppID(content::WebContents* tab) const {
    return tab_id_map_.find(tab) != tab_id_map_.end();
  }

  // LauncherControllerHelper:
  std::string GetAppID(content::WebContents* tab) override {
    return tab_id_map_.find(tab) != tab_id_map_.end() ? tab_id_map_[tab] :
        std::string();
  }

  bool IsValidIDForCurrentUser(const std::string& id) const override {
    for (TabToStringMap::const_iterator i = tab_id_map_.begin();
         i != tab_id_map_.end(); ++i) {
      if (i->second == id)
        return true;
    }
    return false;
  }

  ArcAppListPrefs* GetArcAppListPrefs() const override { return nullptr; }

 private:
  typedef std::map<content::WebContents*, std::string> TabToStringMap;

  TabToStringMap tab_id_map_;

  DISALLOW_COPY_AND_ASSIGN(TestLauncherControllerHelper);
};

// Test implementation of a V2 app launcher item controller.
class TestV2AppLauncherItemController : public LauncherItemController {
 public:
  TestV2AppLauncherItemController(const std::string& app_id,
                                  ChromeLauncherController* controller)
      : LauncherItemController(LauncherItemController::TYPE_APP,
                               app_id,
                               "",
                               controller) {}

  ~TestV2AppLauncherItemController() override {}

  // Override for LauncherItemController:
  bool IsVisible() const override { return true; }
  void Launch(ash::LaunchSource source, int event_flags) override {}
  ash::ShelfItemDelegate::PerformedAction Activate(
      ash::LaunchSource source) override {
    return kExistingWindowActivated;
  }
  void Close() override {}
  ash::ShelfItemDelegate::PerformedAction ItemSelected(
      const ui::Event& event) override {
    return kExistingWindowActivated;
  }
  base::string16 GetTitle() override { return base::string16(); }
  ChromeLauncherAppMenuItems GetApplicationList(int event_flags) override {
    ChromeLauncherAppMenuItems items;
    items.push_back(
        new ChromeLauncherAppMenuItem(base::string16(), NULL, false));
    items.push_back(
        new ChromeLauncherAppMenuItem(base::string16(), NULL, false));
    return items;
  }
  ash::ShelfMenuModel* CreateApplicationMenu(int event_flags) override {
    return NULL;
  }
  bool IsDraggable() override { return false; }
  bool CanPin() const override {
    return GetPinnableForAppID(app_id(), launcher_controller()->profile()) ==
           AppListControllerDelegate::PIN_EDITABLE;
  }
  bool ShouldShowTooltip() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestV2AppLauncherItemController);
};

}  // namespace

class ChromeLauncherControllerImplTest : public BrowserWithTestWindowTest {
 protected:
  ChromeLauncherControllerImplTest()
      : BrowserWithTestWindowTest(Browser::TYPE_TABBED, false) {}

  ~ChromeLauncherControllerImplTest() override {}

  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kUseFirstDisplayAsInternal);
    command_line->AppendSwitch(ash::switches::kAshEnableTouchViewTesting);

    app_list::AppListSyncableServiceFactory::SetUseInTesting();

    BrowserWithTestWindowTest::SetUp();

    if (!profile_manager_) {
      profile_manager_.reset(
          new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
      ASSERT_TRUE(profile_manager_->SetUp());
    }

    model_ = ash::WmShell::Get()->shelf_controller()->model();
    model_observer_.reset(new TestShelfModelObserver);
    model_->AddObserver(model_observer_.get());

    base::DictionaryValue manifest;
    manifest.SetString(extensions::manifest_keys::kName,
                       "launcher controller test extension");
    manifest.SetString(extensions::manifest_keys::kVersion, "1");
    manifest.SetString(extensions::manifest_keys::kDescription,
                       "for testing pinned apps");

    base::DictionaryValue manifest_platform_app;
    manifest_platform_app.SetString(extensions::manifest_keys::kName,
                                    "launcher controller test platform app");
    manifest_platform_app.SetString(extensions::manifest_keys::kVersion, "1");
    manifest_platform_app.SetString(extensions::manifest_keys::kDescription,
                                    "for testing pinned platform apps");
    manifest_platform_app.SetString(extensions::manifest_keys::kApp, "true");
    manifest_platform_app.Set(extensions::manifest_keys::kPlatformAppBackground,
                              new base::DictionaryValue());
    base::ListValue* scripts = new base::ListValue();
    scripts->AppendString("main.js");
    manifest_platform_app.Set(
        extensions::manifest_keys::kPlatformAppBackgroundScripts, scripts);

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile())));
    extension_service_ = extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_->Init();

    if (auto_start_arc_test_)
      arc_test_.SetUp(profile());

    // Wait until |extension_system| is signaled as started.
    base::RunLoop run_loop;
    extension_system->ready().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    app_service_ =
        app_list::AppListSyncableServiceFactory::GetForProfile(profile());
    StartAppSyncService(syncer::SyncDataList());

    std::string error;
    extension1_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", &error);
    extension2_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", &error);
    // Fake gmail extension.
    base::DictionaryValue manifest_gmail;
    manifest_gmail.SetString(extensions::manifest_keys::kName,
                             "Gmail launcher controller test extension");
    manifest_gmail.SetString(extensions::manifest_keys::kVersion, "1");
    manifest_gmail.SetString(extensions::manifest_keys::kDescription,
                             "for testing pinned Gmail");
    manifest_gmail.SetString(extensions::manifest_keys::kLaunchWebURL,
                             kGmailLaunchURL);
    base::ListValue* list = new base::ListValue();
    list->AppendString("*://mail.google.com/mail/ca");
    manifest_gmail.Set(extensions::manifest_keys::kWebURLs, list);

    extension3_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest_gmail, Extension::NO_FLAGS,
                                    extension_misc::kGmailAppId, &error);

    // Fake google docs extension.
    extension4_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    extension_misc::kGoogleDocAppId, &error);
    extension5_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    "cccccccccccccccccccccccccccccccc", &error);
    extension6_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    "dddddddddddddddddddddddddddddddd", &error);
    extension7_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", &error);
    extension8_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    "ffffffffffffffffffffffffffffffff", &error);
    extension_platform_app_ = Extension::Create(
        base::FilePath(), Manifest::UNPACKED, manifest_platform_app,
        Extension::NO_FLAGS, "gggggggggggggggggggggggggggggggg", &error);
    arc_support_host_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                          manifest, Extension::NO_FLAGS,
                                          ArcSupportHost::kHostAppId, &error);
  }

  // Creates a running platform V2 app (not pinned) of type |app_id|.
  virtual void CreateRunningV2App(const std::string& app_id) {
    DCHECK(!test_controller_);
    ash::ShelfID id =
        launcher_controller_->CreateAppShortcutLauncherItemWithType(
            ash::launcher::AppLauncherId(app_id), model_->item_count(),
            ash::TYPE_APP);
    DCHECK(id);
    // Change the created launcher controller into a V2 app controller.
    test_controller_ = new TestV2AppLauncherItemController(app_id,
        launcher_controller_.get());
    launcher_controller_->SetItemController(id, test_controller_);
    DCHECK(launcher_controller_->IsPlatformApp(id));
  }

  // Sets the stage for a multi user test.
  virtual void SetUpMultiUserScenario(syncer::SyncChangeList* user_a,
                                      syncer::SyncChangeList* user_b) {
    InitLauncherController();
    EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());

    // Set an empty pinned pref to begin with.
    syncer::SyncChangeList sync_list;
    InsertAddPinChange(&sync_list, 0, extension_misc::kChromeAppId);
    SendPinChanges(sync_list, true);
    EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());

    // Assume all applications have been added already.
    extension_service_->AddExtension(extension1_.get());
    extension_service_->AddExtension(extension2_.get());
    extension_service_->AddExtension(extension3_.get());
    extension_service_->AddExtension(extension4_.get());
    extension_service_->AddExtension(extension5_.get());
    extension_service_->AddExtension(extension6_.get());
    extension_service_->AddExtension(extension7_.get());
    extension_service_->AddExtension(extension8_.get());
    extension_service_->AddExtension(extension_platform_app_.get());
    // There should be nothing in the list by now.
    EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());

    // Set user a preferences.
    InsertAddPinChange(user_a, 0, extension1_->id());
    InsertAddPinChange(user_a, 1, extension2_->id());
    InsertAddPinChange(user_a, 2, extension3_->id());
    InsertAddPinChange(user_a, 3, extension_platform_app_->id());
    InsertAddPinChange(user_a, 4, extension4_->id());
    InsertAddPinChange(user_a, 5, extension5_->id());
    InsertAddPinChange(user_a, 6, extension_misc::kChromeAppId);

    // Set user b preferences.
    InsertAddPinChange(user_b, 0, extension6_->id());
    InsertAddPinChange(user_b, 1, extension7_->id());
    InsertAddPinChange(user_b, 2, extension8_->id());
    InsertAddPinChange(user_b, 3, extension_misc::kChromeAppId);
  }

  void TearDown() override {
    arc_test_.TearDown();
    model_->RemoveObserver(model_observer_.get());
    model_observer_.reset();
    launcher_controller_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  BrowserWindow* CreateBrowserWindow() override {
    return CreateTestBrowserWindowAura();
  }

  std::unique_ptr<Browser> CreateBrowserWithTestWindowForProfile(
      Profile* profile) {
    TestBrowserWindow* browser_window = CreateTestBrowserWindowAura();
    new TestBrowserWindowOwner(browser_window);
    return base::WrapUnique(
        CreateBrowser(profile, Browser::TYPE_TABBED, false, browser_window));
  }

  void AddAppListLauncherItem() {
    ash::ShelfItem app_list;
    app_list.type = ash::TYPE_APP_LIST;
    model_->Add(app_list);
  }

  void InitLauncherController() {
    launcher_controller_.reset(
        new ChromeLauncherControllerImpl(profile(), model_));
    // TODO(crbug.com/654622): Some tests break with a non-null static instance.
    ChromeLauncherControllerImpl::set_instance_for_test(nullptr);
    launcher_controller_->Init();
  }

  void InitLauncherControllerWithBrowser() {
    InitLauncherController();
    chrome::NewTab(browser());
    browser()->window()->Show();
  }

  void RecreateChromeLauncher() {
    // Destroy controller first if it exists.
    launcher_controller_.reset();
    while (model_->item_count() > 0)
      model_->RemoveItemAt(0);
    AddAppListLauncherItem();
    launcher_controller_ =
        base::MakeUnique<ChromeLauncherControllerImpl>(profile(), model_);
    launcher_controller_->Init();
  }

  void StartAppSyncService(const syncer::SyncDataList& init_sync_list) {
    app_service_->MergeDataAndStartSyncing(
        syncer::APP_LIST, init_sync_list,
        base::MakeUnique<syncer::FakeSyncChangeProcessor>(),
        base::MakeUnique<syncer::SyncErrorFactoryMock>());
    EXPECT_EQ(init_sync_list.size(), app_service_->sync_items().size());
  }

  void StopAppSyncService() { app_service_->StopSyncing(syncer::APP_LIST); }

  void SetAppIconLoader(std::unique_ptr<AppIconLoader> loader) {
    std::vector<std::unique_ptr<AppIconLoader>> loaders;
    loaders.push_back(std::move(loader));
    launcher_controller_->SetAppIconLoadersForTest(loaders);
  }

  void SetAppIconLoaders(std::unique_ptr<AppIconLoader> loader1,
                         std::unique_ptr<AppIconLoader> loader2) {
    std::vector<std::unique_ptr<AppIconLoader>> loaders;
    loaders.push_back(std::move(loader1));
    loaders.push_back(std::move(loader2));
    launcher_controller_->SetAppIconLoadersForTest(loaders);
  }

  void SetLauncherControllerHelper(LauncherControllerHelper* helper) {
    launcher_controller_->SetLauncherControllerHelperForTest(
        base::WrapUnique<LauncherControllerHelper>(helper));
  }

  void InsertPrefValue(base::ListValue* pref_value,
                       int index,
                       const std::string& extension_id) {
    auto entry = base::MakeUnique<base::DictionaryValue>();
    entry->SetString(ash::launcher::kPinnedAppsPrefAppIDPath, extension_id);
    pref_value->Insert(index, std::move(entry));
  }

  void InsertRemoveAllPinsChange(syncer::SyncChangeList* list) {
    for (const auto& sync_peer : app_service_->sync_items()) {
      sync_pb::EntitySpecifics specifics;
      sync_pb::AppListSpecifics* app_list_specifics =
          specifics.mutable_app_list();
      app_list_specifics->set_item_id(sync_peer.first);
      app_list_specifics->set_item_type(sync_pb::AppListSpecifics::TYPE_APP);
      syncer::SyncData sync_data =
          syncer::SyncData::CreateLocalData(sync_peer.first, "Test", specifics);
      list->push_back(syncer::SyncChange(
          FROM_HERE, syncer::SyncChange::ACTION_DELETE, sync_data));
    }
  }

  syncer::StringOrdinal GeneratePinPosition(int position) {
    syncer::StringOrdinal ordinal_position =
        syncer::StringOrdinal::CreateInitialOrdinal();
    for (int i = 0; i < position; ++i)
      ordinal_position = ordinal_position.CreateAfter();
    return ordinal_position;
  }

  void InsertPinChange(syncer::SyncChangeList* list,
                       int position,
                       bool add_pin_change,
                       const std::string& app_id,
                       syncer::SyncChange::SyncChangeType type) {
    sync_pb::EntitySpecifics specifics;
    sync_pb::AppListSpecifics* app_list_specifics =
        specifics.mutable_app_list();
    app_list_specifics->set_item_id(app_id);
    app_list_specifics->set_item_type(sync_pb::AppListSpecifics::TYPE_APP);
    if (add_pin_change) {
      if (position >= 0) {
        app_list_specifics->set_item_pin_ordinal(
            GeneratePinPosition(position).ToInternalValue());
      } else {
        app_list_specifics->set_item_pin_ordinal(std::string());
      }
    }
    syncer::SyncData sync_data =
        syncer::SyncData::CreateLocalData(app_id, "Test", specifics);
    list->push_back(syncer::SyncChange(FROM_HERE, type, sync_data));
  }

  void InsertAddPinChange(syncer::SyncChangeList* list,
                          int position,
                          const std::string& app_id) {
    InsertPinChange(list, position, true, app_id,
                    syncer::SyncChange::ACTION_ADD);
  }

  void InsertUpdatePinChange(syncer::SyncChangeList* list,
                             int position,
                             const std::string& app_id) {
    InsertPinChange(list, position, true, app_id,
                    syncer::SyncChange::ACTION_UPDATE);
  }

  void InsertRemovePinChange(syncer::SyncChangeList* list,
                             const std::string& app_id) {
    InsertPinChange(list, -1, true, app_id, syncer::SyncChange::ACTION_UPDATE);
  }

  void InsertLegacyPinChange(syncer::SyncChangeList* list,
                             const std::string& app_id) {
    InsertPinChange(list, -1, false, app_id, syncer::SyncChange::ACTION_UPDATE);
  }

  void ResetPinModel() {
    syncer::SyncChangeList sync_list;
    InsertRemoveAllPinsChange(&sync_list);
    InsertAddPinChange(&sync_list, 0, kDummyAppId);
    app_service_->ProcessSyncChanges(FROM_HERE, sync_list);
  }

  void SendPinChanges(const syncer::SyncChangeList& sync_list,
                      bool reset_pin_model) {
    if (!reset_pin_model) {
      app_service_->ProcessSyncChanges(FROM_HERE, sync_list);
    } else {
      syncer::SyncChangeList combined_sync_list;
      InsertRemoveAllPinsChange(&combined_sync_list);
      combined_sync_list.insert(combined_sync_list.end(), sync_list.begin(),
                                sync_list.end());
      app_service_->ProcessSyncChanges(FROM_HERE, combined_sync_list);
    }
  }

  // Set the index at which the chrome icon should be.
  void SetShelfChromeIconIndex(int index) {
    DCHECK(
        app_service_->GetPinPosition(extension_misc::kChromeAppId).IsValid());
    syncer::StringOrdinal chrome_position;
    chrome_position = index == 0 ? GeneratePinPosition(0).CreateBefore()
                                 : GeneratePinPosition(index - 1).CreateBetween(
                                       GeneratePinPosition(index));

    syncer::SyncChangeList sync_list;
    sync_pb::EntitySpecifics specifics;
    sync_pb::AppListSpecifics* app_list_specifics =
        specifics.mutable_app_list();
    app_list_specifics->set_item_id(extension_misc::kChromeAppId);
    app_list_specifics->set_item_type(sync_pb::AppListSpecifics::TYPE_APP);
    app_list_specifics->set_item_pin_ordinal(chrome_position.ToInternalValue());
    syncer::SyncData sync_data = syncer::SyncData::CreateLocalData(
        extension_misc::kChromeAppId, "Test", specifics);
    sync_list.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE, sync_data));
    app_service_->ProcessSyncChanges(FROM_HERE, sync_list);
  }

  // Gets the currently configured app launchers from the controller.
  void GetAppLaunchers(ChromeLauncherControllerImpl* controller,
                       std::vector<std::string>* launchers) {
    launchers->clear();
    for (ash::ShelfItems::const_iterator iter(model_->items().begin());
         iter != model_->items().end(); ++iter) {
      ChromeLauncherControllerImpl::IDToItemControllerMap::const_iterator entry(
          controller->id_to_item_controller_map_.find(iter->id));
      if (iter->type == ash::TYPE_APP_SHORTCUT &&
          entry != controller->id_to_item_controller_map_.end()) {
        launchers->push_back(entry->second->app_id());
      }
    }
  }

  // Get the setup of the currently shown launcher items in one string.
  // Each pinned element will start with a big letter, each running but not
  // pinned V1 app will start with a small letter and each running but not
  // pinned V2 app will start with a '*' + small letter.
  std::string GetPinnedAppStatus() {
    std::string result;
    for (int i = 0; i < model_->item_count(); i++) {
      if (!result.empty())
        result.append(", ");
      switch (model_->items()[i].type) {
        case ash::TYPE_APP: {
          if (launcher_controller_->IsPlatformApp(model_->items()[i].id))
            result += "*";
          const std::string& app =
              launcher_controller_->GetAppIDForShelfID(model_->items()[i].id);
          EXPECT_FALSE(launcher_controller_->IsAppPinned(app));
          if (app == extension1_->id()) {
            result += "app1";
          } else if (app == extension2_->id()) {
            result += "app2";
          } else if (app == extension3_->id()) {
            result += "app3";
          } else if (app == extension4_->id()) {
            result += "app4";
          } else if (app == extension5_->id()) {
            result += "app5";
          } else if (app == extension6_->id()) {
            result += "app6";
          } else if (app == extension7_->id()) {
            result += "app7";
          } else if (app == extension8_->id()) {
            result += "app8";
          } else if (app == extension_platform_app_->id()) {
            result += "platform_app";
          } else {
            result += "unknown";
          }
          break;
        }
        case ash::TYPE_APP_SHORTCUT: {
          if (launcher_controller_->IsPlatformApp(model_->items()[i].id))
            result += "*";
          const std::string& app =
              launcher_controller_->GetAppIDForShelfID(model_->items()[i].id);
          EXPECT_TRUE(launcher_controller_->IsAppPinned(app));
          if (app == extension1_->id()) {
            result += "App1";
          } else if (app == extension2_->id()) {
            result += "App2";
          } else if (app == extension3_->id()) {
            result += "App3";
          } else if (app == extension4_->id()) {
            result += "App4";
          } else if (app == extension5_->id()) {
            result += "App5";
          } else if (app == extension6_->id()) {
            result += "App6";
          } else if (app == extension7_->id()) {
            result += "App7";
          } else if (app == extension8_->id()) {
            result += "App8";
          } else if (app == extension_platform_app_->id()) {
            result += "Platform_App";
          } else if (app == arc_support_host_->id()) {
            result += "Play Store";
          } else {
            bool arc_app_found = false;
            for (const auto& arc_app : arc_test_.fake_apps()) {
              if (app == ArcAppTest::GetAppId(arc_app)) {
                result += arc_app.name;
                arc_app_found = true;
                break;
              }
            }
            if (!arc_app_found)
              result += "unknown";
          }
          break;
        }
        case ash::TYPE_BROWSER_SHORTCUT:
          result += "Chrome";
          break;
        case ash::TYPE_APP_LIST:
          result += "AppList";
          break;
        default:
          result += "Unknown";
          break;
      }
    }
    return result;
  }

  // Remember the order of unpinned but running applications for the current
  // user.
  void RememberUnpinnedRunningApplicationOrder() {
    launcher_controller_->RememberUnpinnedRunningApplicationOrder();
  }

  // Restore the order of running but unpinned applications for a given user.
  void RestoreUnpinnedRunningApplicationOrder(const AccountId& account_id) {
    launcher_controller_->RestoreUnpinnedRunningApplicationOrder(
        account_id.GetUserEmail());
  }

  void SendListOfArcApps() {
    arc_test_.app_instance()->RefreshAppList();
    arc_test_.app_instance()->SendRefreshAppList(arc_test_.fake_apps());
  }

  void UninstallArcApps() {
    arc_test_.app_instance()->RefreshAppList();
    arc_test_.app_instance()->SendRefreshAppList(
        std::vector<arc::mojom::AppInfo>());
  }

  void EnableArc(bool enable) {
    enable ? arc_test_.arc_session_manager()->EnableArc()
           : arc_test_.arc_session_manager()->DisableArc();
    arc_test_.arc_session_manager()->OnSyncedPrefChanged(prefs::kArcEnabled,
                                                         false);
    base::RunLoop().RunUntilIdle();
  }

  void EnableTabletMode(bool enable) {
    ash::MaximizeModeController* controller =
        ash::WmShell::Get()->maximize_mode_controller();
    controller->EnableMaximizeModeWindowManager(enable);
  }

  void ValidateArcState(bool arc_enabled,
                        bool arc_managed,
                        arc::ArcSessionManager::State state,
                        const std::string& pin_status) {
    EXPECT_EQ(arc_managed, arc_test_.arc_session_manager()->IsArcManaged());
    EXPECT_EQ(arc_enabled, arc_test_.arc_session_manager()->IsArcEnabled());
    EXPECT_EQ(state, arc_test_.arc_session_manager()->state());
    EXPECT_EQ(pin_status, GetPinnedAppStatus());
  }

  // Creates app window and set optional Arc application id.
  views::Widget* CreateArcWindow(const std::string& window_app_id) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(5, 5, 20, 20);
    params.context = GetContext();
    views::Widget* widget = new views::Widget();
    widget->Init(params);
    // Set Arc id before showing the window to be recognized in
    // ArcAppWindowLauncherController.
    exo::ShellSurface::SetApplicationId(widget->GetNativeWindow(),
                                        window_app_id);
    widget->Show();
    widget->Activate();
    return widget;
  }

  arc::mojom::AppInfo CreateAppInfo(const std::string& name,
                                    const std::string& activity,
                                    const std::string& package_name,
                                    OrientationLock lock) {
    arc::mojom::AppInfo appinfo;
    appinfo.name = name;
    appinfo.package_name = package_name;
    appinfo.activity = activity;
    appinfo.orientation_lock = lock;
    return appinfo;
  }

  std::string AddArcAppAndShortcut(const arc::mojom::AppInfo& app_info) {
    ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
    // Adding app to the prefs, and check that the app is accessible by id.
    prefs->AddAppAndShortcut(
        true /* app_ready */, app_info.name, app_info.package_name,
        app_info.activity, std::string() /* intent_uri */,
        std::string() /* icon_resource_id */, false /* sticky */,
        true /* notifications_enabled */, false /* shortcut */,
        true /* launchable */, app_info.orientation_lock);
    const std::string app_id =
        ArcAppListPrefs::GetAppId(app_info.package_name, app_info.activity);
    EXPECT_TRUE(prefs->GetApp(app_id));
    return app_id;
  }

  void NotifyOnTaskCreated(const arc::mojom::AppInfo& appinfo,
                           int32_t task_id) {
    ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
    prefs->OnTaskCreated(task_id, appinfo.package_name, appinfo.activity,
                         appinfo.name, std::string());
  }

  void NotifyOnTaskOrientationLockRequested(int32_t task_id,
                                            OrientationLock lock) {
    ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
    prefs->OnTaskOrientationLockRequested(task_id, lock);
  }

  // Needed for extension service & friends to work.
  scoped_refptr<Extension> extension1_;
  scoped_refptr<Extension> extension2_;
  scoped_refptr<Extension> extension3_;
  scoped_refptr<Extension> extension4_;
  scoped_refptr<Extension> extension5_;
  scoped_refptr<Extension> extension6_;
  scoped_refptr<Extension> extension7_;
  scoped_refptr<Extension> extension8_;
  scoped_refptr<Extension> extension_platform_app_;
  scoped_refptr<Extension> arc_support_host_;

  ArcAppTest arc_test_;
  bool auto_start_arc_test_ = false;
  std::unique_ptr<ChromeLauncherControllerImpl> launcher_controller_;
  std::unique_ptr<TestShelfModelObserver> model_observer_;
  ash::ShelfModel* model_ = nullptr;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  // |item_delegate_manager_| owns |test_controller_|.
  LauncherItemController* test_controller_ = nullptr;

  ExtensionService* extension_service_ = nullptr;

  app_list::AppListSyncableService* app_service_ = nullptr;

 private:
  TestBrowserWindow* CreateTestBrowserWindowAura() {
    std::unique_ptr<aura::Window> window(new aura::Window(nullptr));
    window->set_id(0);
    window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    aura::client::ParentWindowWithContext(window.get(), GetContext(),
                                          gfx::Rect(200, 200));

    return new TestBrowserWindowAura(std::move(window));
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerImplTest);
};

class ChromeLauncherControllerImplWithArcTest
    : public ChromeLauncherControllerImplTest {
 protected:
  ChromeLauncherControllerImplWithArcTest() { auto_start_arc_test_ = true; }
  ~ChromeLauncherControllerImplWithArcTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerImplWithArcTest);
};

// Watches WebContents and blocks until it is destroyed. This is needed for
// the destruction of a V2 application.
class WebContentsDestroyedWatcher : public content::WebContentsObserver {
 public:
  explicit WebContentsDestroyedWatcher(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        message_loop_runner_(new content::MessageLoopRunner) {
    EXPECT_TRUE(web_contents != NULL);
  }
  ~WebContentsDestroyedWatcher() override {}

  // Waits until the WebContents is destroyed.
  void Wait() {
    message_loop_runner_->Run();
  }

 private:
  // Overridden WebContentsObserver methods.
  void WebContentsDestroyed() override { message_loop_runner_->Quit(); }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDestroyedWatcher);
};

// A V1 windowed application.
class V1App : public TestBrowserWindow {
 public:
  V1App(Profile* profile, const std::string& app_name) {
    // Create a window.
    native_window_.reset(new aura::Window(NULL));
    native_window_->set_id(0);
    native_window_->SetType(ui::wm::WINDOW_TYPE_POPUP);
    native_window_->Init(ui::LAYER_TEXTURED);
    native_window_->Show();
    aura::client::ParentWindowWithContext(native_window_.get(),
                                          ash::Shell::GetPrimaryRootWindow(),
                                          gfx::Rect(10, 10, 20, 30));
    Browser::CreateParams params = Browser::CreateParams::CreateForApp(
        kCrxAppPrefix + app_name, true /* trusted_source */, gfx::Rect(),
        profile);
    params.window = this;
    browser_.reset(new Browser(params));
    chrome::AddTabAt(browser_.get(), GURL(), 0, true);
  }

  ~V1App() override {
    // close all tabs. Note that we do not need to destroy the browser itself.
    browser_->tab_strip_model()->CloseAllTabs();
  }

  Browser* browser() { return browser_.get(); }

  // TestBrowserWindow override:
  gfx::NativeWindow GetNativeWindow() const override {
    return native_window_.get();
  }

 private:
  // The associated browser with this app.
  std::unique_ptr<Browser> browser_;

  // The native window we use.
  std::unique_ptr<aura::Window> native_window_;

  DISALLOW_COPY_AND_ASSIGN(V1App);
};

// A V2 application window created with an |extension| and for a |profile|.
// Upon destruction it will properly close the application; supports panels too.
class V2App {
 public:
  V2App(Profile* profile,
        const extensions::Extension* extension,
        extensions::AppWindow::WindowType window_type =
            extensions::AppWindow::WINDOW_TYPE_DEFAULT)
      : creator_web_contents_(
            content::WebContentsTester::CreateTestWebContents(profile,
                                                              nullptr)) {
    window_ = new extensions::AppWindow(profile, new ChromeAppDelegate(true),
                                        extension);
    extensions::AppWindow::CreateParams params;
    params.window_type = window_type;
    // Note: normally, the creator RFH is the background page of the
    // app/extension
    // calling chrome.app.window.create. For unit testing purposes, just passing
    // in a random RenderFrameHost is Good Enough™.
    window_->Init(GURL(std::string()),
                  new extensions::AppWindowContentsImpl(window_),
                  creator_web_contents_->GetMainFrame(), params);
  }

  virtual ~V2App() {
    WebContentsDestroyedWatcher destroyed_watcher(window_->web_contents());
    window_->GetBaseWindow()->Close();
    destroyed_watcher.Wait();
  }

  extensions::AppWindow* window() { return window_; }

 private:
  std::unique_ptr<content::WebContents> creator_web_contents_;

  // The app window which represents the application. Note that the window
  // deletes itself asynchronously after window_->GetBaseWindow()->Close() gets
  // called.
  extensions::AppWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(V2App);
};

// The testing framework to test multi profile scenarios.
class MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest
    : public ChromeLauncherControllerImplTest {
 protected:
  MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest() {}

  ~MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest()
      override {}

  // Overwrite the Setup function to enable multi profile and needed objects.
  void SetUp() override {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));

    ASSERT_TRUE(profile_manager_->SetUp());

    // AvatarMenu and multiple profiles works after user logged in.
    profile_manager_->SetLoggedIn(true);

    // Initialize the UserManager singleton to a fresh FakeUserManager instance.
    user_manager_enabler_.reset(new chromeos::ScopedUserManagerEnabler(
        new chromeos::FakeChromeUserManager));

    // Initialize the WallpaperManager singleton.
    chromeos::WallpaperManager::Initialize();

    // Initialize the rest.
    ChromeLauncherControllerImplTest::SetUp();

    // Get some base objects.
    ash::test::AshTestHelper::GetTestSessionStateDelegate()
        ->set_logged_in_users(2);
    shell_delegate_ = static_cast<ash::test::TestShellDelegate*>(
        ash::WmShell::Get()->delegate());
    shell_delegate_->set_multi_profiles_enabled(true);
  }

  void TearDown() override {
    ChromeLauncherControllerImplTest::TearDown();
    user_manager_enabler_.reset();
    for (ProfileToNameMap::iterator it = created_profiles_.begin();
         it != created_profiles_.end(); ++it)
      profile_manager_->DeleteTestingProfile(it->second);
    chromeos::WallpaperManager::Shutdown();

    // A Task is leaked if we don't destroy everything, then run the message
    // loop.
    base::RunLoop().RunUntilIdle();
  }

  // Creates a profile for a given |user_name|. Note that this class will keep
  // the ownership of the created object.
  TestingProfile* CreateMultiUserProfile(const std::string& user_name) {
    const std::string email_string = user_name + "@example.com";
    const AccountId account_id(AccountId::FromUserEmail(email_string));
    ash::test::AshTestHelper::GetTestSessionStateDelegate()->AddUser(
        account_id);
    // Add a user to the fake user manager.
    ash::test::AshTestHelper::GetTestSessionStateDelegate()->AddUser(
        account_id);
    GetFakeUserManager()->AddUser(account_id);

    GetFakeUserManager()->LoginUser(account_id);

    TestingProfile* profile =
        profile_manager()->CreateTestingProfile(account_id.GetUserEmail());
    EXPECT_TRUE(profile);

    // Remember the profile name so that we can destroy it upon destruction.
    created_profiles_[profile] = account_id.GetUserEmail();
    if (chrome::MultiUserWindowManager::GetInstance())
      chrome::MultiUserWindowManager::GetInstance()->AddUser(profile);
    if (launcher_controller_)
      launcher_controller_->AdditionalUserAddedToSession(profile);
    return profile;
  }

  // Switch to another user.
  void SwitchActiveUser(const AccountId& account_id) {
    ash::test::AshTestHelper::GetTestSessionStateDelegate()->SwitchActiveUser(
        account_id);
    GetFakeUserManager()->SwitchActiveUser(account_id);
    chrome::MultiUserWindowManagerChromeOS* manager =
        static_cast<chrome::MultiUserWindowManagerChromeOS*>(
            chrome::MultiUserWindowManager::GetInstance());
    manager->SetAnimationSpeedForTest(
        chrome::MultiUserWindowManagerChromeOS::ANIMATION_SPEED_DISABLED);
    manager->ActiveUserChanged(account_id);
    launcher_controller_->browser_status_monitor_for_test()->ActiveUserChanged(
        account_id.GetUserEmail());

    for (const auto& controller :
         launcher_controller_->app_window_controllers_for_test()) {
      controller->ActiveUserChanged(account_id.GetUserEmail());
    }
  }

  // Creates a browser with a |profile| and load a tab with a |title| and |url|.
  std::unique_ptr<Browser> CreateBrowserAndTabWithProfile(
      Profile* profile,
      const std::string& title,
      const std::string& url) {
    std::unique_ptr<Browser> browser(
        CreateBrowserWithTestWindowForProfile(profile));
    chrome::NewTab(browser.get());

    browser->window()->Show();
    NavigateAndCommitActiveTabWithTitle(browser.get(), GURL(url),
                                        ASCIIToUTF16(title));
    return browser;
  }

  // Creates a running V1 application.
  // Note that with the use of the launcher_controller_helper as done below,
  // this is only usable with a single v1 application.
  V1App* CreateRunningV1App(Profile* profile,
                            const std::string& app_name,
                            const std::string& url) {
    V1App* v1_app = new V1App(profile, app_name);
    // Create a new launcher controller helper and assign it to the launcher so
    // that this app gets properly detected.
    // TODO(skuhne): Create a more intelligent launcher controller helper that
    // is able to detect all running apps properly.
    TestLauncherControllerHelper* helper = new TestLauncherControllerHelper;
    helper->SetAppID(v1_app->browser()->tab_strip_model()->GetWebContentsAt(0),
                     app_name);
    SetLauncherControllerHelper(helper);

    NavigateAndCommitActiveTabWithTitle(v1_app->browser(), GURL(url),
                                        base::string16());
    return v1_app;
  }

  ash::test::TestShellDelegate* shell_delegate() { return shell_delegate_; }

  // Override BrowserWithTestWindowTest:
  TestingProfile* CreateProfile() override {
    return CreateMultiUserProfile("user1");
  }
  void DestroyProfile(TestingProfile* profile) override {
    // Delete the profile through our profile manager.
    ProfileToNameMap::iterator it = created_profiles_.find(profile);
    DCHECK(it != created_profiles_.end());
    profile_manager_->DeleteTestingProfile(it->second);
    created_profiles_.erase(it);
  }

 private:
  typedef std::map<Profile*, std::string> ProfileToNameMap;
  TestingProfileManager* profile_manager() { return profile_manager_.get(); }

  chromeos::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;

  ash::test::TestShellDelegate* shell_delegate_;

  ProfileToNameMap created_profiles_;

  DISALLOW_COPY_AND_ASSIGN(
      MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest);
};

class ChromeLauncherControllerImplMultiProfileWithArcTest
    : public MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest {  // NOLINT(whitespace/line_length)
 protected:
  ChromeLauncherControllerImplMultiProfileWithArcTest() {
    auto_start_arc_test_ = true;
  }
  ~ChromeLauncherControllerImplMultiProfileWithArcTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerImplMultiProfileWithArcTest);
};

TEST_F(ChromeLauncherControllerImplTest, DefaultApps) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, Chrome, App3", GetPinnedAppStatus());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
}

TEST_F(ChromeLauncherControllerImplWithArcTest,
       ArcAppPinCrossPlatformWorkflow) {
  // Work on Arc-disabled platform first.
  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[1]);
  const std::string arc_app_id3 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[2]);

  InitLauncherController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension3_.get());

  // extension 1, 3 are pinned by user
  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, arc_app_id1);
  InsertAddPinChange(&sync_list, 2, extension2_->id());
  InsertAddPinChange(&sync_list, 3, arc_app_id2);
  InsertAddPinChange(&sync_list, 4, extension3_->id());
  SendPinChanges(sync_list, true);
  SetShelfChromeIconIndex(1);

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id3));
  EXPECT_EQ("AppList, App1, Chrome, App2, App3", GetPinnedAppStatus());

  // Persist pin state, we don't have active pin for Arc apps yet, but pin
  // model should have it.
  syncer::SyncDataList copy_sync_list =
      app_service_->GetAllSyncData(syncer::APP_LIST);

  launcher_controller_.reset();
  SendPinChanges(syncer::SyncChangeList(), true);
  StopAppSyncService();
  EXPECT_EQ(0U, app_service_->sync_items().size());

  // Move to Arc-enabled platform, restart syncing with stored data.
  StartAppSyncService(copy_sync_list);
  RecreateChromeLauncher();

  // Pins must be automatically updated.
  SendListOfArcApps();
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id3));

  EXPECT_EQ("AppList, App1, Chrome, Fake App 0, App2, Fake App 1, App3",
            GetPinnedAppStatus());

  // Now move pins on Arc-enabled platform.
  model_->Move(1, 4);
  model_->Move(3, 1);
  model_->Move(3, 5);
  model_->Move(4, 2);
  EXPECT_EQ("AppList, App2, Fake App 1, Chrome, App1, Fake App 0, App3",
            GetPinnedAppStatus());

  copy_sync_list = app_service_->GetAllSyncData(syncer::APP_LIST);

  launcher_controller_.reset();
  ResetPinModel();

  SendPinChanges(syncer::SyncChangeList(), true);
  StopAppSyncService();
  EXPECT_EQ(0U, app_service_->sync_items().size());

  // Move back to Arc-disabled platform.
  EnableArc(false);
  StartAppSyncService(copy_sync_list);
  RecreateChromeLauncher();

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id3));
  EXPECT_EQ("AppList, App2, Chrome, App1, App3", GetPinnedAppStatus());

  // Now move/remove pins on Arc-disabled platform.
  model_->Move(4, 2);
  launcher_controller_->UnpinAppWithID(extension2_->id());
  EXPECT_EQ("AppList, App3, Chrome, App1", GetPinnedAppStatus());
  EnableArc(true);

  SendListOfArcApps();

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id3));
  EXPECT_EQ("AppList, Fake App 1, App3, Chrome, App1, Fake App 0",
            GetPinnedAppStatus());
}

/*
 * Test ChromeLauncherControllerImpl correctly merges policy pinned apps
 * and user pinned apps
 */
TEST_F(ChromeLauncherControllerImplTest, MergePolicyAndUserPrefPinnedApps) {
  InitLauncherController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());
  extension_service_->AddExtension(extension5_.get());
  // extension 1, 3 are pinned by user
  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list, 2, extension3_->id());
  SendPinChanges(sync_list, true);

  base::ListValue policy_value;
  // extension 2 4 are pinned by policy
  InsertPrefValue(&policy_value, 0, extension2_->id());
  InsertPrefValue(&policy_value, 1, extension4_->id());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.DeepCopy());

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  // 2 is not pinned as it's not installed
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension4_->id()));
  // install extension 2 and check
  extension_service_->AddExtension(extension2_.get());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));

  // Check user can manually pin or unpin these apps
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(extension1_->id(), profile()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(extension2_->id(), profile()));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(extension3_->id(), profile()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(extension4_->id(), profile()));

  // Check the order of shelf pinned apps
  EXPECT_EQ("AppList, App2, App4, App1, Chrome, App3", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced reverse
// order.
TEST_F(ChromeLauncherControllerImplTest, RestoreDefaultAppsReverseOrder) {
  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 2, extension3_->id());
  SendPinChanges(sync_list, true);

  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the shelf - behind the
  // chrome icon.
  ash::ShelfItem item;
  extension_service_->AddExtension(extension3_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ("AppList, Chrome, App3", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ("AppList, Chrome, App2, App3", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("AppList, Chrome, App1, App2, App3", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced random
// order.
TEST_F(ChromeLauncherControllerImplTest, RestoreDefaultAppsRandomOrder) {
  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 2, extension3_->id());
  SendPinChanges(sync_list, true);

  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome, App2", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome, App1, App2", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, Chrome, App1, App2, App3", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned / moved them (on another system) when they are synced
// random order - including the chrome icon.
TEST_F(ChromeLauncherControllerImplTest,
       RestoreDefaultAppsRandomOrderChromeMoved) {
  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list, 2, extension2_->id());
  InsertAddPinChange(&sync_list, 3, extension3_->id());
  SendPinChanges(sync_list, true);

  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the shelf - behind the
  // chrome icon.
  ash::ShelfItem item;
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome, App2", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, App1, Chrome, App2", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, App1, Chrome, App2, App3", GetPinnedAppStatus());
}

// Check that syncing to a different state does the correct thing.
TEST_F(ChromeLauncherControllerImplTest, RestoreDefaultAppsResyncOrder) {
  InitLauncherController();

  syncer::SyncChangeList sync_list0;
  InsertAddPinChange(&sync_list0, 0, extension1_->id());
  InsertAddPinChange(&sync_list0, 1, extension2_->id());
  InsertAddPinChange(&sync_list0, 2, extension3_->id());
  SendPinChanges(sync_list0, true);

  // The shelf layout has always one static item at the beginning (App List).
  extension_service_->AddExtension(extension2_.get());
  EXPECT_EQ("AppList, Chrome, App2", GetPinnedAppStatus());
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("AppList, Chrome, App1, App2", GetPinnedAppStatus());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, Chrome, App1, App2, App3", GetPinnedAppStatus());

  // Change the order with increasing chrome position and decreasing position.
  syncer::SyncChangeList sync_list1;
  InsertAddPinChange(&sync_list1, 0, extension3_->id());
  InsertAddPinChange(&sync_list1, 1, extension1_->id());
  InsertAddPinChange(&sync_list1, 2, extension2_->id());
  InsertAddPinChange(&sync_list1, 3, extension_misc::kChromeAppId);
  SendPinChanges(sync_list1, true);
  EXPECT_EQ("AppList, App3, App1, App2, Chrome", GetPinnedAppStatus());

  syncer::SyncChangeList sync_list2;
  InsertAddPinChange(&sync_list2, 0, extension2_->id());
  InsertAddPinChange(&sync_list2, 1, extension3_->id());
  InsertAddPinChange(&sync_list2, 2, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list2, 3, extension1_->id());
  SendPinChanges(sync_list2, true);
  EXPECT_EQ("AppList, App2, App3, Chrome, App1", GetPinnedAppStatus());

  // Check that the chrome icon can also be at the first possible location.
  syncer::SyncChangeList sync_list3;
  InsertAddPinChange(&sync_list3, 0, extension3_->id());
  InsertAddPinChange(&sync_list3, 1, extension2_->id());
  InsertAddPinChange(&sync_list3, 2, extension1_->id());
  SendPinChanges(sync_list3, true);
  EXPECT_EQ("AppList, Chrome, App3, App2, App1", GetPinnedAppStatus());

  // Check that unloading of extensions works as expected.
  extension_service_->UnloadExtension(extension1_->id(),
                                      UnloadedExtensionInfo::REASON_UNINSTALL);
  EXPECT_EQ("AppList, Chrome, App3, App2", GetPinnedAppStatus());

  extension_service_->UnloadExtension(extension2_->id(),
                                      UnloadedExtensionInfo::REASON_UNINSTALL);
  EXPECT_EQ("AppList, Chrome, App3", GetPinnedAppStatus());

  // Check that an update of an extension does not crash the system.
  extension_service_->UnloadExtension(extension3_->id(),
                                      UnloadedExtensionInfo::REASON_UPDATE);
  EXPECT_EQ("AppList, Chrome, App3", GetPinnedAppStatus());
}

// Check that simple locking of an application will 'create' a launcher item.
TEST_F(ChromeLauncherControllerImplTest, CheckLockApps) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension2_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[2].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension2_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension2_->id()));
}

// Check that multiple locks of an application will be properly handled.
TEST_F(ChromeLauncherControllerImplTest, CheckMultiLockApps) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  for (int i = 0; i < 2; i++) {
    launcher_controller_->LockV1AppWithID(extension1_->id());

    EXPECT_EQ(3, model_->item_count());
    EXPECT_EQ(ash::TYPE_APP, model_->items()[2].type);
    EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
    EXPECT_NE(ash::kInvalidShelfID,
              launcher_controller_->GetShelfIDForAppID(extension1_->id()));
  }

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[2].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));
}

// Check that already pinned items are not effected by locks.
TEST_F(ChromeLauncherControllerImplTest, CheckAlreadyPinnedLockApps) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  launcher_controller_->PinAppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  launcher_controller_->UnpinAppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));
}

// Check that already pinned items which get locked stay after unpinning.
TEST_F(ChromeLauncherControllerImplTest, CheckPinnedAppsStayAfterUnlock) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  launcher_controller_->PinAppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  launcher_controller_->UnpinAppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[2].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));
}

// Check that running applications wich are not pinned get properly restored
// upon user change.
TEST_F(ChromeLauncherControllerImplTest, CheckRunningAppOrder) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());

  // Add a few running applications.
  launcher_controller_->LockV1AppWithID(extension1_->id());
  launcher_controller_->LockV1AppWithID(extension2_->id());
  launcher_controller_->LockV1AppWithID(extension3_->id());
  EXPECT_EQ(5, model_->item_count());
  // Note that this not only checks the order of applications but also the
  // running type.
  EXPECT_EQ("AppList, Chrome, app1, app2, app3", GetPinnedAppStatus());

  // Remember the current order of applications for the current user.
  const AccountId& current_account_id =
      multi_user_util::GetAccountIdFromProfile(profile());
  RememberUnpinnedRunningApplicationOrder();

  // Switch some items and check that restoring a user which was not yet
  // remembered changes nothing.
  model_->Move(2, 3);
  EXPECT_EQ("AppList, Chrome, app2, app1, app3", GetPinnedAppStatus());
  const AccountId second_fake_account_id(
      AccountId::FromUserEmail("second-fake-user@fake.com"));
  RestoreUnpinnedRunningApplicationOrder(second_fake_account_id);
  EXPECT_EQ("AppList, Chrome, app2, app1, app3", GetPinnedAppStatus());

  // Restoring the stored user should however do the right thing.
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("AppList, Chrome, app1, app2, app3", GetPinnedAppStatus());

  // Switch again some items and even delete one - making sure that the missing
  // item gets properly handled.
  model_->Move(3, 4);
  launcher_controller_->UnlockV1AppWithID(extension1_->id());
  EXPECT_EQ("AppList, Chrome, app3, app2", GetPinnedAppStatus());
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("AppList, Chrome, app2, app3", GetPinnedAppStatus());

  // Check that removing more items does not crash and changes nothing.
  launcher_controller_->UnlockV1AppWithID(extension2_->id());
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("AppList, Chrome, app3", GetPinnedAppStatus());
  launcher_controller_->UnlockV1AppWithID(extension3_->id());
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());
}

TEST_F(ChromeLauncherControllerImplWithArcTest, ArcDeferredLaunch) {
  RecreateChromeLauncher();

  const arc::mojom::AppInfo& app1 = arc_test_.fake_apps()[0];
  const arc::mojom::AppInfo& app2 = arc_test_.fake_apps()[1];
  const arc::mojom::AppInfo& app3 = arc_test_.fake_apps()[2];
  const std::string arc_app_id1 = ArcAppTest::GetAppId(app1);
  const std::string arc_app_id2 = ArcAppTest::GetAppId(app2);
  const std::string arc_app_id3 = ArcAppTest::GetAppId(app3);

  SendListOfArcApps();

  arc_test_.StopArcInstance();

  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id1));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id2));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id3));

  arc::LaunchApp(profile(), arc_app_id1, ui::EF_LEFT_MOUSE_BUTTON);
  arc::LaunchApp(profile(), arc_app_id1, ui::EF_LEFT_MOUSE_BUTTON);
  arc::LaunchApp(profile(), arc_app_id2, ui::EF_LEFT_MOUSE_BUTTON);
  arc::LaunchApp(profile(), arc_app_id3, ui::EF_LEFT_MOUSE_BUTTON);
  arc::LaunchApp(profile(), arc_app_id3, ui::EF_LEFT_MOUSE_BUTTON);

  const ash::ShelfID shelf_id_app_1 =
      launcher_controller_->GetShelfIDForAppID(arc_app_id1);
  const ash::ShelfID shelf_id_app_2 =
      launcher_controller_->GetShelfIDForAppID(arc_app_id2);
  const ash::ShelfID shelf_id_app_3 =
      launcher_controller_->GetShelfIDForAppID(arc_app_id3);
  EXPECT_NE(ash::kInvalidShelfID, shelf_id_app_1);
  EXPECT_NE(ash::kInvalidShelfID, shelf_id_app_2);
  EXPECT_NE(ash::kInvalidShelfID, shelf_id_app_3);

  // We activated arc_app_id1 twice but expect one close for item controller
  // stops launching request.
  LauncherItemController* item_controller =
      launcher_controller_->GetLauncherItemController(shelf_id_app_1);
  ASSERT_NE(nullptr, item_controller);
  item_controller->Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id1));
  EXPECT_EQ(shelf_id_app_2,
            launcher_controller_->GetShelfIDForAppID(arc_app_id2));
  EXPECT_EQ(shelf_id_app_3,
            launcher_controller_->GetShelfIDForAppID(arc_app_id3));

  arc_test_.RestartArcInstance();
  SendListOfArcApps();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id1));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id2));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id3));

  ASSERT_EQ(2U, arc_test_.app_instance()->launch_requests().size());

  const arc::FakeAppInstance::Request* request1 =
      arc_test_.app_instance()->launch_requests()[0].get();
  const arc::FakeAppInstance::Request* request2 =
      arc_test_.app_instance()->launch_requests()[1].get();

  EXPECT_TRUE((request1->IsForApp(app2) && request2->IsForApp(app3)) ||
              (request1->IsForApp(app3) && request2->IsForApp(app2)));
}

TEST_F(ChromeLauncherControllerImplMultiProfileWithArcTest, ArcMultiUser) {
  SendListOfArcApps();

  InitLauncherController();
  SetLauncherControllerHelper(new TestLauncherControllerHelper);

  // App1 exists all the time.
  // App2 is created when primary user is active and destroyed when secondary
  // user is active.
  // App3 created when secondary user is active.

  const std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));

  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[1]);
  const std::string arc_app_id3 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[2]);

  std::string window_app_id1("org.chromium.arc.1");
  views::Widget* arc_window1 = CreateArcWindow(window_app_id1);
  arc_test_.app_instance()->SendTaskCreated(1, arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id1));

  std::string window_app_id2("org.chromium.arc.2");
  views::Widget* arc_window2 = CreateArcWindow(window_app_id2);
  arc_test_.app_instance()->SendTaskCreated(2, arc_test_.fake_apps()[1],
                                            std::string());
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id2));

  launcher_controller_->SetProfileForTest(profile2);
  SwitchActiveUser(account_id2);

  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id1));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id2));

  std::string window_app_id3("org.chromium.arc.3");
  views::Widget* arc_window3 = CreateArcWindow(window_app_id3);
  arc_test_.app_instance()->SendTaskCreated(3, arc_test_.fake_apps()[2],
                                            std::string());
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id3));

  arc_window2->CloseNow();
  arc_test_.app_instance()->SendTaskDestroyed(2);

  launcher_controller_->SetProfileForTest(profile());
  SwitchActiveUser(account_id);

  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id1));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id2));
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id3));

  // Close active window to let test passes.
  arc_window1->CloseNow();
  arc_window3->CloseNow();
}

TEST_F(ChromeLauncherControllerImplWithArcTest, ArcRunningApp) {
  InitLauncherController();

  const std::string arc_app_id = ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  SendListOfArcApps();
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id));

  // Normal flow, create/destroy tasks.
  std::string window_app_id1("org.chromium.arc.1");
  std::string window_app_id2("org.chromium.arc.2");
  std::string window_app_id3("org.chromium.arc.3");
  CreateArcWindow(window_app_id1);
  arc_test_.app_instance()->SendTaskCreated(1, arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id));
  CreateArcWindow(window_app_id2);
  arc_test_.app_instance()->SendTaskCreated(2, arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id));
  arc_test_.app_instance()->SendTaskDestroyed(1);
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id));
  arc_test_.app_instance()->SendTaskDestroyed(2);
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id));

  // Stopping bridge removes apps.
  CreateArcWindow(window_app_id3);
  arc_test_.app_instance()->SendTaskCreated(3, arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id));
  arc_test_.StopArcInstance();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id));
}

// Test race creation/deletion of Arc app.
// TODO(khmel): Remove after moving everything to wayland protocol.
TEST_F(ChromeLauncherControllerImplWithArcTest, ArcRaceCreateClose) {
  InitLauncherController();

  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[1]);
  SendListOfArcApps();

  // Arc window created before and closed after mojom notification.
  std::string window_app_id1("org.chromium.arc.1");
  views::Widget* arc_window = CreateArcWindow(window_app_id1);
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id1));
  ASSERT_TRUE(arc_window);
  arc_test_.app_instance()->SendTaskCreated(1, arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id1));
  arc_test_.app_instance()->SendTaskDestroyed(1);
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id1));
  arc_window->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id1));

  // Arc window created after and closed before mojom notification.
  std::string window_app_id2("org.chromium.arc.2");
  arc_test_.app_instance()->SendTaskCreated(2, arc_test_.fake_apps()[1],
                                            std::string());
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id2));
  arc_window = CreateArcWindow(window_app_id2);
  ASSERT_TRUE(arc_window);
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id2));
  arc_window->Close();
  base::RunLoop().RunUntilIdle();
  // Closing window does not close shelf item. It is closed on task destroy.
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id2));
  arc_test_.app_instance()->SendTaskDestroyed(2);
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(arc_app_id2));
}

TEST_F(ChromeLauncherControllerImplWithArcTest, ArcWindowRecreation) {
  InitLauncherController();

  const std::string arc_app_id = ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  SendListOfArcApps();

  std::string window_app_id("org.chromium.arc.1");
  views::Widget* arc_window = CreateArcWindow(window_app_id);
  ASSERT_TRUE(arc_window);
  arc_test_.app_instance()->SendTaskCreated(1, arc_test_.fake_apps()[0],
                                            std::string());
  const ash::ShelfID shelf_id =
      launcher_controller_->GetShelfIDForAppID(arc_app_id);
  EXPECT_NE(ash::kInvalidShelfID, shelf_id);

  for (int i = 0; i < 3; ++i) {
    arc_window->Close();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(shelf_id, launcher_controller_->GetShelfIDForAppID(arc_app_id));

    arc_window = CreateArcWindow(window_app_id);
    ASSERT_TRUE(arc_window);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(shelf_id, launcher_controller_->GetShelfIDForAppID(arc_app_id));
  }
}

// Validate that Arc app is pinned correctly and pin is removed automatically
// once app is uninstalled.
TEST_F(ChromeLauncherControllerImplWithArcTest, ArcAppPin) {
  InitLauncherController();

  const std::string arc_app_id = ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);

  SendListOfArcApps();
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));

  launcher_controller_->PinAppWithID(extension1_->id());
  launcher_controller_->PinAppWithID(arc_app_id);
  launcher_controller_->PinAppWithID(extension2_->id());

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));

  EXPECT_EQ("AppList, Chrome, App1, Fake App 0, App2", GetPinnedAppStatus());
  UninstallArcApps();
  EXPECT_EQ("AppList, Chrome, App1, App2", GetPinnedAppStatus());
  SendListOfArcApps();
  EXPECT_EQ("AppList, Chrome, App1, App2", GetPinnedAppStatus());

  // Disable/Enable Arc should persist pin state.
  launcher_controller_->PinAppWithID(arc_app_id);
  EXPECT_EQ("AppList, Chrome, App1, App2, Fake App 0", GetPinnedAppStatus());
  arc::ArcSessionManager::Get()->Shutdown();
  EXPECT_EQ("AppList, Chrome, App1, App2, Fake App 0", GetPinnedAppStatus());
  arc::ArcSessionManager::Get()->OnPrimaryUserProfilePrepared(profile());
  EXPECT_EQ("AppList, Chrome, App1, App2, Fake App 0", GetPinnedAppStatus());

  // Opt-Out/Opt-In remove item from the shelf.
  EnableArc(false);
  EXPECT_EQ("AppList, Chrome, App1, App2", GetPinnedAppStatus());
  EnableArc(true);
  EXPECT_EQ("AppList, Chrome, App1, App2", GetPinnedAppStatus());
  SendListOfArcApps();
  EXPECT_EQ("AppList, Chrome, App1, App2, Fake App 0", GetPinnedAppStatus());
}

// Validates that Arc app pins persist across OptOut/OptIn.
TEST_F(ChromeLauncherControllerImplWithArcTest, ArcAppPinOptOutOptIn) {
  InitLauncherController();

  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[1]);

  SendListOfArcApps();
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  launcher_controller_->PinAppWithID(extension1_->id());
  launcher_controller_->PinAppWithID(arc_app_id2);
  launcher_controller_->PinAppWithID(extension2_->id());
  launcher_controller_->PinAppWithID(arc_app_id1);

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_EQ("AppList, Chrome, App1, Fake App 1, App2, Fake App 0",
            GetPinnedAppStatus());

  EnableArc(false);

  EXPECT_EQ("AppList, Chrome, App1, App2", GetPinnedAppStatus());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id2));

  EnableArc(true);
  SendListOfArcApps();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id2));

  EXPECT_EQ("AppList, Chrome, App1, Fake App 1, App2, Fake App 0",
            GetPinnedAppStatus());
}

// Check that with multi profile V1 apps are properly added / removed from the
// shelf.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest,
       V1AppUpdateOnUserSwitch) {
  // Create a browser item in the LauncherController.
  InitLauncherController();
  EXPECT_EQ(2, model_->item_count());
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(
        CreateRunningV1App(profile(), extension_misc::kGmailAppId, gmail_url));
    EXPECT_EQ(3, model_->item_count());

    // After switching to a second user the item should be gone.
    std::string user2 = "user2";
    TestingProfile* profile2 = CreateMultiUserProfile(user2);
    const AccountId account_id2(
        multi_user_util::GetAccountIdFromProfile(profile2));
    const AccountId account_id(
        multi_user_util::GetAccountIdFromProfile(profile()));
    SwitchActiveUser(account_id2);
    EXPECT_EQ(2, model_->item_count());

    // After switching back the item should be back.
    SwitchActiveUser(account_id);
    EXPECT_EQ(3, model_->item_count());
    // Note we destroy now the gmail app with the closure end.
  }
  EXPECT_EQ(2, model_->item_count());
}

// Check edge cases with multi profile V1 apps in the shelf.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest,
       V1AppUpdateOnUserSwitchEdgecases) {
  // Create a browser item in the LauncherController.
  InitLauncherController();

  // First test: Create an app when the user is not active.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(
        CreateRunningV1App(profile2, extension_misc::kGmailAppId, gmail_url));
    EXPECT_EQ(2, model_->item_count());

    // However - switching to the user should show it.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(3, model_->item_count());

    // Second test: Remove the app when the user is not active and see that it
    // works.
    SwitchActiveUser(account_id);
    EXPECT_EQ(2, model_->item_count());
    // Note: the closure ends and the browser will go away.
  }
  EXPECT_EQ(2, model_->item_count());
  SwitchActiveUser(account_id2);
  EXPECT_EQ(2, model_->item_count());
  SwitchActiveUser(account_id);
  EXPECT_EQ(2, model_->item_count());
}

// Check edge case where a visiting V1 app gets closed (crbug.com/321374).
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest,
       V1CloseOnVisitingDesktop) {
  // Create a browser item in the LauncherController.
  InitLauncherController();

  chrome::MultiUserWindowManager* manager =
      chrome::MultiUserWindowManager::GetInstance();

  // First create an app when the user is active.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(CreateRunningV1App(
        profile(), extension_misc::kGmailAppId, kGmailLaunchURL));
    EXPECT_EQ(3, model_->item_count());

    // Transfer the app to the other screen and switch users.
    manager->ShowWindowForUser(v1_app->browser()->window()->GetNativeWindow(),
                               account_id2);
    EXPECT_EQ(3, model_->item_count());
    SwitchActiveUser(account_id2);
    EXPECT_EQ(2, model_->item_count());
  }
  // After the app was destroyed, switch back. (which caused already a crash).
  SwitchActiveUser(account_id);

  // Create the same app again - which was also causing the crash.
  EXPECT_EQ(2, model_->item_count());
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(CreateRunningV1App(
        profile(), extension_misc::kGmailAppId, kGmailLaunchURL));
    EXPECT_EQ(3, model_->item_count());
  }
  SwitchActiveUser(account_id2);
  EXPECT_EQ(2, model_->item_count());
}

// Check edge cases with multi profile V1 apps in the shelf.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest,
       V1AppUpdateOnUserSwitchEdgecases2) {
  // Create a browser item in the LauncherController.
  InitLauncherController();
  SetLauncherControllerHelper(new TestLauncherControllerHelper);

  // First test: Create an app when the user is not active.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  SwitchActiveUser(account_id2);
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(
        CreateRunningV1App(profile(), extension_misc::kGmailAppId, gmail_url));
    EXPECT_EQ(2, model_->item_count());

    // However - switching to the user should show it.
    SwitchActiveUser(account_id);
    EXPECT_EQ(3, model_->item_count());

    // Second test: Remove the app when the user is not active and see that it
    // works.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(2, model_->item_count());
    v1_app.reset();
  }
  EXPECT_EQ(2, model_->item_count());
  SwitchActiveUser(account_id);
  EXPECT_EQ(2, model_->item_count());
  SwitchActiveUser(account_id2);
  EXPECT_EQ(2, model_->item_count());
}

// Check that activating an item which is on another user's desktop, will bring
// it back.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest,
       TestLauncherActivationPullsBackWindow) {
  // Create a browser item in the LauncherController.
  InitLauncherController();
  chrome::MultiUserWindowManager* manager =
      chrome::MultiUserWindowManager::GetInstance();

  // Create a second test profile. The first is the one in profile() created in
  // BrowserWithTestWindowTest::SetUp().
  // No need to add the profiles to the MultiUserWindowManager here.
  // CreateMultiUserProfile() already does that.
  TestingProfile* profile2 = CreateMultiUserProfile("user2");
  const AccountId current_user =
      multi_user_util::GetAccountIdFromProfile(profile());

  // Create a browser window with a native window for the current user.
  Browser::CreateParams params(profile());
  std::unique_ptr<Browser> browser(
      chrome::CreateBrowserWithAuraTestWindowForParams(nullptr, &params));
  BrowserWindow* browser_window = browser->window();
  aura::Window* window = browser_window->GetNativeWindow();
  manager->SetWindowOwner(window, current_user);

  // Check that an activation of the window on its owner's desktop does not
  // change the visibility to another user.
  launcher_controller_->ActivateWindowOrMinimizeIfActive(browser_window, false);
  EXPECT_TRUE(manager->IsWindowOnDesktopOfUser(window, current_user));

  // Transfer the window to another user's desktop and check that activating it
  // does pull it back to that user.
  manager->ShowWindowForUser(
      window, multi_user_util::GetAccountIdFromProfile(profile2));
  EXPECT_FALSE(manager->IsWindowOnDesktopOfUser(window, current_user));
  launcher_controller_->ActivateWindowOrMinimizeIfActive(browser_window, false);
  EXPECT_TRUE(manager->IsWindowOnDesktopOfUser(window, current_user));
}

// Check that lock -> pin -> unlock -> unpin does properly transition.
TEST_F(ChromeLauncherControllerImplTest, CheckLockPinUnlockUnpin) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[2].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  launcher_controller_->PinAppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));

  launcher_controller_->UnpinAppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(extension1_->id()));
}

// Check that a locked (windowed V1 application) will be properly converted
// between locked and pinned when the order gets changed through a profile /
// policy change.
TEST_F(ChromeLauncherControllerImplTest,
       RestoreDefaultAndLockedAppsResyncOrder) {
  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension3_->id());
  SendPinChanges(sync_list, true);

  // The shelf layout has always one static item at the beginning (App List).
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("AppList, Chrome, App1", GetPinnedAppStatus());
  extension_service_->AddExtension(extension2_.get());
  // No new app icon will be generated.
  EXPECT_EQ("AppList, Chrome, App1", GetPinnedAppStatus());

  // Add the app as locked app which will add it (un-pinned).
  launcher_controller_->LockV1AppWithID(extension2_->id());
  EXPECT_EQ("AppList, Chrome, App1, app2", GetPinnedAppStatus());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, Chrome, App1, App3, app2", GetPinnedAppStatus());

  // Now request to pin all items which should convert the locked item into a
  // pinned item.
  syncer::SyncChangeList sync_list1;
  InsertAddPinChange(&sync_list1, 0, extension3_->id());
  InsertAddPinChange(&sync_list1, 1, extension2_->id());
  InsertAddPinChange(&sync_list1, 2, extension1_->id());
  SendPinChanges(sync_list1, true);
  EXPECT_EQ("AppList, Chrome, App3, App2, App1", GetPinnedAppStatus());

  // Going back to a status where there is no requirement for app 2 to be pinned
  // should convert it back to locked but not pinned and state. The position
  // is determined by the |ShelfModel|'s weight system and since running
  // applications are not allowed to be mixed with shortcuts, it should show up
  // at the end of the list.
  syncer::SyncChangeList sync_list2;
  InsertAddPinChange(&sync_list2, 0, extension3_->id());
  InsertAddPinChange(&sync_list2, 1, extension1_->id());
  SendPinChanges(sync_list2, true);
  EXPECT_EQ("AppList, Chrome, App3, App1, app2", GetPinnedAppStatus());

  // Removing an item should simply close it and everything should shift.
  SendPinChanges(syncer::SyncChangeList(), true);
  EXPECT_EQ("AppList, Chrome, App3, app2", GetPinnedAppStatus());
}

// Check that a running and not pinned V2 application will be properly converted
// between locked and pinned when the order gets changed through a profile /
// policy change.
TEST_F(ChromeLauncherControllerImplTest,
       RestoreDefaultAndRunningV2AppsResyncOrder) {
  InitLauncherController();
  syncer::SyncChangeList sync_list0;
  InsertAddPinChange(&sync_list0, 0, extension1_->id());
  InsertAddPinChange(&sync_list0, 1, extension3_->id());
  SendPinChanges(sync_list0, true);
  // The shelf layout has always one static item at the beginning (app List).
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("AppList, Chrome, App1", GetPinnedAppStatus());
  extension_service_->AddExtension(extension_platform_app_.get());
  // No new app icon will be generated.
  EXPECT_EQ("AppList, Chrome, App1", GetPinnedAppStatus());
  // Add an unpinned but running V2 app.
  CreateRunningV2App(extension_platform_app_->id());
  EXPECT_EQ("AppList, Chrome, App1, *platform_app", GetPinnedAppStatus());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, Chrome, App1, App3, *platform_app", GetPinnedAppStatus());

  // Now request to pin all items which should convert the locked item into a
  // pinned item.
  syncer::SyncChangeList sync_list1;
  InsertAddPinChange(&sync_list1, 0, extension3_->id());
  InsertAddPinChange(&sync_list1, 1, extension_platform_app_->id());
  InsertAddPinChange(&sync_list1, 2, extension1_->id());
  SendPinChanges(sync_list1, true);
  EXPECT_EQ("AppList, Chrome, App3, *Platform_App, App1", GetPinnedAppStatus());

  // Going back to a status where there is no requirement for the V2 app to be
  // pinned should convert it back to running V2 app. Since the position is
  // determined by the |ShelfModel|'s weight system, it will be after last
  // pinned item.
  syncer::SyncChangeList sync_list2;
  InsertAddPinChange(&sync_list2, 0, extension3_->id());
  InsertAddPinChange(&sync_list2, 1, extension1_->id());
  SendPinChanges(sync_list2, true);
  EXPECT_EQ("AppList, Chrome, App3, App1, *platform_app", GetPinnedAppStatus());

  // Removing an item should simply close it and everything should shift.
  syncer::SyncChangeList sync_list3;
  InsertAddPinChange(&sync_list3, 0, extension3_->id());
  SendPinChanges(sync_list3, true);
  EXPECT_EQ("AppList, Chrome, App3, *platform_app", GetPinnedAppStatus());
}

// Each user has a different set of applications pinned. Check that when
// switching between the two users, the state gets properly set.
TEST_F(ChromeLauncherControllerImplTest, UserSwitchIconRestore) {
  syncer::SyncChangeList user_a;
  syncer::SyncChangeList user_b;

  SetUpMultiUserScenario(&user_a, &user_b);

  // Show user 1.
  SendPinChanges(user_a, true);
  EXPECT_EQ("AppList, App1, App2, App3, *Platform_App, App4, App5, Chrome",
            GetPinnedAppStatus());

  // Show user 2.
  SendPinChanges(user_b, true);
  EXPECT_EQ("AppList, App6, App7, App8, Chrome", GetPinnedAppStatus());

  // Switch back to 1.
  SendPinChanges(user_a, true);
  EXPECT_EQ("AppList, App1, App2, App3, *Platform_App, App4, App5, Chrome",
            GetPinnedAppStatus());

  // Switch back to 2.
  SendPinChanges(user_b, true);
  EXPECT_EQ("AppList, App6, App7, App8, Chrome", GetPinnedAppStatus());
}

// Each user has a different set of applications pinned, and one user has an
// application running. Check that when switching between the two users, the
// state gets properly set.
TEST_F(ChromeLauncherControllerImplTest,
       UserSwitchIconRestoreWithRunningV2App) {
  syncer::SyncChangeList user_a;
  syncer::SyncChangeList user_b;

  SetUpMultiUserScenario(&user_a, &user_b);

  // Run the platform (V2) app.
  CreateRunningV2App(extension_platform_app_->id());

  // Show user 1.
  SendPinChanges(user_a, true);
  EXPECT_EQ("AppList, App1, App2, App3, *Platform_App, App4, App5, Chrome",
            GetPinnedAppStatus());

  // Show user 2.
  SendPinChanges(user_b, true);
  EXPECT_EQ("AppList, App6, App7, App8, Chrome, *platform_app",
            GetPinnedAppStatus());

  // Switch back to 1.
  SendPinChanges(user_a, true);
  EXPECT_EQ("AppList, App1, App2, App3, *Platform_App, App4, App5, Chrome",
            GetPinnedAppStatus());

  // Switch back to 2.
  SendPinChanges(user_b, true);
  EXPECT_EQ("AppList, App6, App7, App8, Chrome, *platform_app",
            GetPinnedAppStatus());
}

// Each user has a different set of applications pinned, and one user has an
// application running. The chrome icon is not the last item in the list.
// Check that when switching between the two users, the state gets properly set.
// There was once a bug associated with this.
TEST_F(ChromeLauncherControllerImplTest,
       UserSwitchIconRestoreWithRunningV2AppChromeInMiddle) {
  syncer::SyncChangeList user_a;
  syncer::SyncChangeList user_b;
  SetUpMultiUserScenario(&user_a, &user_b);

  // Run the platform (V2) app.
  CreateRunningV2App(extension_platform_app_->id());

  // Show user 1.
  SendPinChanges(user_a, true);
  SetShelfChromeIconIndex(5);
  EXPECT_EQ("AppList, App1, App2, App3, *Platform_App, App4, Chrome, App5",
            GetPinnedAppStatus());

  // Show user 2.
  SendPinChanges(user_b, true);
  SetShelfChromeIconIndex(4);
  EXPECT_EQ("AppList, App6, App7, App8, Chrome, *platform_app",
            GetPinnedAppStatus());

  // Switch back to 1.
  SendPinChanges(user_a, true);
  SetShelfChromeIconIndex(5);
  EXPECT_EQ("AppList, App1, App2, App3, *Platform_App, App4, Chrome, App5",
            GetPinnedAppStatus());
}

TEST_F(ChromeLauncherControllerImplTest, Policy) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());

  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension_misc::kChromeAppId);
  SendPinChanges(sync_list, true);

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.DeepCopy());

  // Only |extension1_| should get pinned. |extension2_| is specified but not
  // installed, and |extension3_| is part of the default set, but that shouldn't
  // take effect when the policy override is in place.
  ASSERT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension2_| should add it to the launcher.
  extension_service_->AddExtension(extension2_.get());
  ASSERT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Removing |extension1_| from the policy should not be reflected in the
  // launcher and pin will exist.
  policy_value.Remove(0, NULL);
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.DeepCopy());
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
}

TEST_F(ChromeLauncherControllerImplTest, UnpinWithUninstall) {
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());

  InitLauncherController();

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension4_->id()));

  extension_service_->UnloadExtension(extension3_->id(),
                                      UnloadedExtensionInfo::REASON_UNINSTALL);

  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension4_->id()));
}

TEST_F(ChromeLauncherControllerImplTest, SyncUpdates) {
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());

  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 10, extension_misc::kChromeAppId);
  SendPinChanges(sync_list, true);

  std::vector<std::string> expected_launchers;
  std::vector<std::string> actual_launchers;
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Unavailable extensions don't create launcher items.
  sync_list.clear();
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 3, extension4_->id());
  SendPinChanges(sync_list, false);

  expected_launchers.push_back(extension2_->id());
  expected_launchers.push_back(extension4_->id());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  sync_list.clear();
  InsertAddPinChange(&sync_list, 2, extension3_->id());
  SendPinChanges(sync_list, false);
  expected_launchers.insert(expected_launchers.begin() + 1, extension3_->id());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  sync_list.clear();
  InsertUpdatePinChange(&sync_list, 0, extension4_->id());
  InsertUpdatePinChange(&sync_list, 1, extension3_->id());
  InsertUpdatePinChange(&sync_list, 2, extension2_->id());
  SendPinChanges(sync_list, false);
  std::reverse(expected_launchers.begin(), expected_launchers.end());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Sending legacy sync change without pin info should not affect pin model.
  sync_list.clear();
  InsertLegacyPinChange(&sync_list, extension4_->id());
  SendPinChanges(sync_list, false);
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  sync_list.clear();
  InsertRemovePinChange(&sync_list, extension4_->id());
  SendPinChanges(sync_list, false);
  expected_launchers.erase(expected_launchers.begin());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  sync_list.clear();
  InsertRemovePinChange(&sync_list, extension3_->id());
  InsertRemovePinChange(&sync_list, extension2_->id());
  SendPinChanges(sync_list, false);
  expected_launchers.clear();
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);
}

TEST_F(ChromeLauncherControllerImplTest, ImportLegacyPin) {
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());

  // Initially pins are imported from legacy pref based model.
  base::ListValue value;
  InsertPrefValue(&value, 0, extension4_->id());
  InsertPrefValue(&value, 1, extension2_->id());
  profile()->GetTestingPrefService()->SetManagedPref(prefs::kPinnedLauncherApps,
                                                     value.DeepCopy());

  InitLauncherController();

  EXPECT_EQ("AppList, Chrome, App4, App2", GetPinnedAppStatus());

  // At this point changing old pref based model does not affect pin model.
  InsertPrefValue(&value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetManagedPref(prefs::kPinnedLauncherApps,
                                                     value.DeepCopy());
  EXPECT_EQ("AppList, Chrome, App4, App2", GetPinnedAppStatus());
  RecreateChromeLauncher();
  EXPECT_EQ("AppList, Chrome, App4, App2", GetPinnedAppStatus());
}

TEST_F(ChromeLauncherControllerImplTest, PendingInsertionOrder) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());

  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 2, extension3_->id());
  SendPinChanges(sync_list, true);

  std::vector<std::string> expected_launchers;
  expected_launchers.push_back(extension1_->id());
  expected_launchers.push_back(extension3_->id());
  std::vector<std::string> actual_launchers;

  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Install |extension2| and verify it shows up between the other two.
  extension_service_->AddExtension(extension2_.get());
  expected_launchers.insert(expected_launchers.begin() + 1, extension2_->id());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);
}

// Checks the created menus and menu lists for correctness. It uses the given
// |controller| to create the objects for the given |item| and checks the
// found item count against the |expected_items|. The |title| list contains the
// menu titles in the order of their appearance in the menu (not including the
// application name).
bool CheckMenuCreation(ChromeLauncherControllerImpl* controller,
                       const ash::ShelfItem& item,
                       size_t expected_items,
                       base::string16 title[],
                       bool is_browser) {
  ChromeLauncherAppMenuItems items = controller->GetApplicationList(item, 0);
  // A new behavior has been added: Only show menus if there is at least one
  // item available.
  if (expected_items < 1 && is_browser) {
    EXPECT_EQ(0u, items.size());
    return items.size() == 0;
  }
  // There should be one item in there: The title.
  EXPECT_EQ(expected_items + 1, items.size());
  EXPECT_FALSE(items[0]->IsEnabled());
  for (size_t i = 0; i < expected_items; i++) {
    EXPECT_EQ(title[i], items[1 + i]->title());
    // Check that the first real item has a leading separator.
    if (i == 1)
      EXPECT_TRUE(items[i]->HasLeadingSeparator());
    else
      EXPECT_FALSE(items[i]->HasLeadingSeparator());
  }

  std::unique_ptr<ash::ShelfMenuModel> menu(
      new LauncherApplicationMenuItemModel(
          controller->GetApplicationList(item, 0)));
  // The first element in the menu is a spacing separator. On some systems
  // (e.g. Windows) such things do not exist. As such we check the existence
  // and adjust dynamically.
  int first_item = menu->GetTypeAt(0) == ui::MenuModel::TYPE_SEPARATOR ? 1 : 0;
  int expected_menu_items = first_item +
                            (expected_items ? (expected_items + 3) : 2);
  EXPECT_EQ(expected_menu_items, menu->GetItemCount());
  EXPECT_FALSE(menu->IsEnabledAt(first_item));
  if (expected_items) {
    EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
              menu->GetTypeAt(first_item + 1));
  }
  return items.size() == expected_items + 1;
}

// Check that browsers get reflected correctly in the launcher menu.
TEST_F(ChromeLauncherControllerImplTest, BrowserMenuGeneration) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  chrome::NewTab(browser());

  InitLauncherController();

  // Check that the browser list is empty at this time.
  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id =
      launcher_controller_->GetShelfIDForAppID(extension_misc::kChromeAppId);
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 0, NULL, true));

  // Now make the created browser() visible by showing its browser window.
  browser()->window()->Show();
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL("http://test1"), title1);
  base::string16 one_menu_item[] = { title1 };

  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, one_menu_item, true));

  // Create one more browser/window and check that one more was added.
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForProfile(profile()));
  chrome::NewTab(browser2.get());
  browser2->window()->Show();
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser2.get(), GURL("http://test2"),
                                      title2);

  // Check that the list contains now two entries - make furthermore sure that
  // the active item is the first entry.
  base::string16 two_menu_items[] = {title1, title2};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 2, two_menu_items, true));

  // Apparently we have to close all tabs we have.
  chrome::CloseTab(browser2.get());
}

// Check the multi profile case where only user related browsers should show
// up.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest,
       BrowserMenuGenerationTwoUsers) {
  // Create a browser item in the LauncherController.
  InitLauncherController();

  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id =
      launcher_controller_->GetShelfIDForAppID(extension_misc::kChromeAppId);

  // Check that the menu is empty.
  chrome::NewTab(browser());
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 0, NULL, true));

  // Show the created |browser()| by showing its window.
  browser()->window()->Show();
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL("http://test1"), title1);
  base::string16 one_menu_item1[] = { title1 };
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, one_menu_item1, true));

  // Create a browser for another user and check that it is not included in the
  // users running browser list.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  std::unique_ptr<Browser> browser2(
      CreateBrowserAndTabWithProfile(profile2, user2, "http://test2"));
  base::string16 one_menu_item2[] = { ASCIIToUTF16(user2) };
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, one_menu_item1, true));

  // Switch to the other user and make sure that only that browser window gets
  // shown.
  SwitchActiveUser(account_id2);
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, one_menu_item2, true));

  // Transferred browsers of other users should not show up in the list.
  chrome::MultiUserWindowManager::GetInstance()->ShowWindowForUser(
      browser()->window()->GetNativeWindow(), account_id2);
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, one_menu_item2, true));

  chrome::CloseTab(browser2.get());
}

// Check that V1 apps are correctly reflected in the launcher menu using the
// refocus logic.
// Note that the extension matching logic is tested by the extension system
// and does not need a separate test here.
TEST_F(ChromeLauncherControllerImplTest, V1AppMenuGeneration) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());

  InitLauncherControllerWithBrowser();

  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension3_| adds it to the launcher.
  ash::ShelfID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_->item_count());
  int gmail_index = model_->ItemIndexByID(gmail_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[gmail_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(gmail_url));

  // Check the menu content.
  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id =
      launcher_controller_->GetShelfIDForAppID(extension_misc::kChromeAppId);

  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 0, NULL, false));

  // Set the gmail URL to a new tab.
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title1);

  base::string16 one_menu_item[] = { title1 };
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 1, one_menu_item, false));

  // Create one empty tab.
  chrome::NewTab(browser());
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(
      browser(),
      GURL("https://bla"),
      title2);

  // and another one with another gmail instance.
  chrome::NewTab(browser());
  base::string16 title3 = ASCIIToUTF16("Test3");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title3);
  base::string16 two_menu_items[] = {title1, title3};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 2, two_menu_items, false));

  // Even though the item is in the V1 app list, it should also be in the
  // browser list.
  base::string16 browser_menu_item[] = {title3};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, browser_menu_item, false));

  // Test that closing of (all) the item(s) does work (and all menus get
  // updated properly).
  launcher_controller_->Close(item_gmail.id);

  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 0, NULL, false));
  base::string16 browser_menu_item2[] = { title2 };
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, browser_menu_item2, false));
}

// Check the multi profile case where only user related apps should show up.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest,
       V1AppMenuGenerationTwoUsers) {
  // Create a browser item in the LauncherController.
  InitLauncherController();
  chrome::NewTab(browser());

  // Installing |extension3_| adds it to the launcher.
  ash::ShelfID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_->item_count());
  int gmail_index = model_->ItemIndexByID(gmail_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[gmail_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(gmail_url));

  // Check the menu content.
  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id =
      launcher_controller_->GetShelfIDForAppID(extension_misc::kChromeAppId);

  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 0, NULL, false));

  // Set the gmail URL to a new tab.
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title1);

  base::string16 one_menu_item[] = { title1 };
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 1, one_menu_item, false));

  // Create a second profile and switch to that user.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  SwitchActiveUser(account_id2);

  // No item should have content yet.
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 0, NULL, true));
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 0, NULL, false));

  // Transfer the browser of the first user - it should still not show up.
  chrome::MultiUserWindowManager::GetInstance()->ShowWindowForUser(
      browser()->window()->GetNativeWindow(), account_id2);

  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 0, NULL, true));
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 0, NULL, false));
}

// Check that V2 applications are creating items properly in the launcher when
// instantiated by the current user.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest,
       V2AppHandlingTwoUsers) {
  InitLauncherController();
  // Create a profile for our second user (will be destroyed by the framework).
  TestingProfile* profile2 = CreateMultiUserProfile("user2");
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  // Check that there is a browser and a app launcher.
  EXPECT_EQ(2, model_->item_count());

  // Add a v2 app.
  V2App v2_app(profile(), extension1_.get());
  EXPECT_EQ(3, model_->item_count());

  // After switching users the item should go away.
  SwitchActiveUser(account_id2);
  EXPECT_EQ(2, model_->item_count());

  // And it should come back when switching back.
  SwitchActiveUser(account_id);
  EXPECT_EQ(3, model_->item_count());
}

// Check that V2 applications are creating items properly in edge cases:
// a background user creates a V2 app, gets active and inactive again and then
// deletes the app.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest,
       V2AppHandlingTwoUsersEdgeCases) {
  InitLauncherController();
  // Create a profile for our second user (will be destroyed by the framework).
  TestingProfile* profile2 = CreateMultiUserProfile("user2");
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  // Check that there is a browser and a app launcher.
  EXPECT_EQ(2, model_->item_count());

  // Switch to an inactive user.
  SwitchActiveUser(account_id2);
  EXPECT_EQ(2, model_->item_count());

  // Add the v2 app to the inactive user and check that no item was added to
  // the launcher.
  {
    V2App v2_app(profile(), extension1_.get());
    EXPECT_EQ(2, model_->item_count());

    // Switch to the primary user and check that the item is shown.
    SwitchActiveUser(account_id);
    EXPECT_EQ(3, model_->item_count());

    // Switch to the second user and check that the item goes away - even if the
    // item gets closed.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(2, model_->item_count());
  }

  // After the application was killed there should be still 2 items.
  EXPECT_EQ(2, model_->item_count());

  // Switching then back to the default user should not show the additional item
  // anymore.
  SwitchActiveUser(account_id);
  EXPECT_EQ(2, model_->item_count());
}

// Check that V2 applications will be made visible on the target desktop if
// another window of the same type got previously teleported there.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest,
       V2AppFollowsTeleportedWindow) {
  InitLauncherController();
  chrome::MultiUserWindowManager* manager =
      chrome::MultiUserWindowManager::GetInstance();

  // Create and add three users / profiles, and go to #1's desktop.
  TestingProfile* profile1 = CreateMultiUserProfile("user-1");
  TestingProfile* profile2 = CreateMultiUserProfile("user-2");
  TestingProfile* profile3 = CreateMultiUserProfile("user-3");
  const AccountId account_id1(
      multi_user_util::GetAccountIdFromProfile(profile1));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  const AccountId account_id3(
      multi_user_util::GetAccountIdFromProfile(profile3));
  SwitchActiveUser(account_id1);

  // A v2 app for user #1 should be shown first and get hidden when switching to
  // desktop #2.
  V2App v2_app_1(profile1, extension1_.get());
  EXPECT_TRUE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  SwitchActiveUser(account_id2);
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());

  // Add a v2 app for user #1 while on desktop #2 should not be shown.
  V2App v2_app_2(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());

  // Teleport the app from user #1 to the desktop #2 should show it.
  manager->ShowWindowForUser(v2_app_1.window()->GetNativeWindow(), account_id2);
  EXPECT_TRUE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());

  // Creating a new application for user #1 on desktop #2 should teleport it
  // there automatically.
  V2App v2_app_3(profile1, extension1_.get());
  EXPECT_TRUE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_3.window()->GetNativeWindow()->IsVisible());

  // Switching back to desktop#1 and creating an app for user #1 should move
  // the app on desktop #1.
  SwitchActiveUser(account_id1);
  V2App v2_app_4(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_3.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_4.window()->GetNativeWindow()->IsVisible());

  // Switching to desktop #3 and creating an app for user #1 should place it on
  // that user's desktop (#1).
  SwitchActiveUser(account_id3);
  V2App v2_app_5(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_5.window()->GetNativeWindow()->IsVisible());
  SwitchActiveUser(account_id1);
  EXPECT_TRUE(v2_app_5.window()->GetNativeWindow()->IsVisible());

  // Switching to desktop #2, hiding the app window and creating an app should
  // teleport there automatically.
  SwitchActiveUser(account_id2);
  v2_app_1.window()->Hide();
  V2App v2_app_6(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_6.window()->GetNativeWindow()->IsVisible());
}

// Check that V2 applications hide correctly on the shelf when the app window
// is hidden.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerImplTest,
       V2AppHiddenWindows) {
  InitLauncherController();

  TestingProfile* profile2 = CreateMultiUserProfile("user-2");
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  SwitchActiveUser(account_id);
  EXPECT_EQ(2, model_->item_count());

  V2App v2_app_1(profile(), extension1_.get());
  EXPECT_EQ(3, model_->item_count());
  {
    // Hide and show the app.
    v2_app_1.window()->Hide();
    EXPECT_EQ(2, model_->item_count());

    v2_app_1.window()->Show(extensions::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(3, model_->item_count());
  }
  {
    // Switch user, hide and show the app and switch back.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(2, model_->item_count());

    v2_app_1.window()->Hide();
    EXPECT_EQ(2, model_->item_count());

    v2_app_1.window()->Show(extensions::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(2, model_->item_count());

    SwitchActiveUser(account_id);
    EXPECT_EQ(3, model_->item_count());
  }
  {
    // Switch user, hide the app, switch back and then show it again.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(2, model_->item_count());

    v2_app_1.window()->Hide();
    EXPECT_EQ(2, model_->item_count());

    SwitchActiveUser(account_id);
    EXPECT_EQ(2, model_->item_count());

    v2_app_1.window()->Show(extensions::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(3, model_->item_count());
  }
  {
    // Create a second app, hide and show it and then hide both apps.
    V2App v2_app_2(profile(), extension1_.get());
    EXPECT_EQ(3, model_->item_count());

    v2_app_2.window()->Hide();
    EXPECT_EQ(3, model_->item_count());

    v2_app_2.window()->Show(extensions::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(3, model_->item_count());

    v2_app_1.window()->Hide();
    v2_app_2.window()->Hide();
    EXPECT_EQ(2, model_->item_count());
  }
}

// Checks that the generated menu list properly activates items.
TEST_F(ChromeLauncherControllerImplTest, V1AppMenuExecution) {
  InitLauncherControllerWithBrowser();

  // Add |extension3_| to the launcher and add two items.
  GURL gmail = GURL("https://mail.google.com/mail/u");
  ash::ShelfID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(gmail_url));
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title1);
  chrome::NewTab(browser());
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title2);

  // Check that the menu is properly set.
  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  base::string16 two_menu_items[] = {title1, title2};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 2, two_menu_items, false));
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  // Execute the second item in the list (which shouldn't do anything since that
  // item is per definition already the active tab).
  {
    std::unique_ptr<ash::ShelfMenuModel> menu(
        new LauncherApplicationMenuItemModel(
            launcher_controller_->GetApplicationList(item_gmail, 0)));
    // The first element in the menu is a spacing separator. On some systems
    // (e.g. Windows) such things do not exist. As such we check the existence
    // and adjust dynamically.
    int first_item =
        (menu->GetTypeAt(0) == ui::MenuModel::TYPE_SEPARATOR) ? 1 : 0;
    menu->ActivatedAt(first_item + 3);
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Execute the first item.
  {
    std::unique_ptr<ash::ShelfMenuModel> menu(
        new LauncherApplicationMenuItemModel(
            launcher_controller_->GetApplicationList(item_gmail, 0)));
    int first_item =
        (menu->GetTypeAt(0) == ui::MenuModel::TYPE_SEPARATOR) ? 1 : 0;
    menu->ActivatedAt(first_item + 2);
  }
  // Now the active tab should be the second item.
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
}

// Checks that the generated menu list properly deletes items.
TEST_F(ChromeLauncherControllerImplTest, V1AppMenuDeletionExecution) {
  InitLauncherControllerWithBrowser();

  // Add |extension3_| to the launcher and add two items.
  GURL gmail = GURL("https://mail.google.com/mail/u");
  ash::ShelfID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(gmail_url));
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title1);
  chrome::NewTab(browser());
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title2);

  // Check that the menu is properly set.
  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  base::string16 two_menu_items[] = {title1, title2};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 2, two_menu_items, false));

  int tabs = browser()->tab_strip_model()->count();
  // Activate the proper tab through the menu item.
  {
    ChromeLauncherAppMenuItems items =
        launcher_controller_->GetApplicationList(item_gmail, 0);
    items[1]->Execute(0);
    EXPECT_EQ(tabs, browser()->tab_strip_model()->count());
  }

  // Delete one tab through the menu item.
  {
    ChromeLauncherAppMenuItems items =
        launcher_controller_->GetApplicationList(item_gmail, 0);
    items[1]->Execute(ui::EF_SHIFT_DOWN);
    EXPECT_EQ(--tabs, browser()->tab_strip_model()->count());
  }
}

// Tests that panels create launcher items correctly
TEST_F(ChromeLauncherControllerImplTest, AppPanels) {
  InitLauncherController();
  model_observer_->clear_counts();
  const std::string app_id = extension1_->id();

  // app_icon_loader is owned by ChromeLauncherControllerImpl.
  TestAppIconLoaderImpl* app_icon_loader = new TestAppIconLoaderImpl();
  app_icon_loader->AddSupportedApp(app_id);
  SetAppIconLoader(std::unique_ptr<AppIconLoader>(app_icon_loader));

  // Make an app panel; the ShelfItem is added by ash::ShelfWindowWatcher.
  std::unique_ptr<V2App> app_panel1 = base::MakeUnique<V2App>(
      profile(), extension1_.get(), extensions::AppWindow::WINDOW_TYPE_PANEL);
  EXPECT_TRUE(app_panel1->window()->GetNativeWindow()->IsVisible());
  int panel_index = model_observer_->last_index();
  EXPECT_EQ(1, model_observer_->added());
  EXPECT_EQ(1, app_icon_loader->fetch_count());
  model_observer_->clear_counts();

  // App panels should have a separate identifier than the app id
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(app_id));

  // Setting the app image should not change the panel, which has a window icon.
  gfx::ImageSkia image;
  launcher_controller_->OnAppImageUpdated(app_id, image);
  EXPECT_EQ(0, model_observer_->changed());
  model_observer_->clear_counts();

  // Make a second app panel and verify that it gets the same index as the first
  // panel, being added to the left of the existing panel.
  std::unique_ptr<V2App> app_panel2 = base::MakeUnique<V2App>(
      profile(), extension2_.get(), extensions::AppWindow::WINDOW_TYPE_PANEL);
  EXPECT_EQ(panel_index, model_observer_->last_index());
  EXPECT_EQ(1, model_observer_->added());
  model_observer_->clear_counts();

  app_panel1.reset();
  app_panel2.reset();
  EXPECT_EQ(2, model_observer_->removed());
}

// Tests that the Gmail extension matches more than the app itself claims with
// the manifest file.
TEST_F(ChromeLauncherControllerImplTest, GmailMatching) {
  InitLauncherControllerWithBrowser();

  // Create a Gmail browser tab.
  chrome::NewTab(browser());
  base::string16 title = ASCIIToUTF16("Test");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title);
  content::WebContents* content =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Check that the launcher controller does not recognize the running app.
  EXPECT_FALSE(launcher_controller_->ContentCanBeHandledByGmailApp(content));

  // Installing |extension3_| adds it to the launcher.
  ash::ShelfID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_->item_count());
  int gmail_index = model_->ItemIndexByID(gmail_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[gmail_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Check that it is now handled.
  EXPECT_TRUE(launcher_controller_->ContentCanBeHandledByGmailApp(content));

  // Check also that the app has detected that properly.
  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  EXPECT_EQ(2U, launcher_controller_->GetApplicationList(item_gmail, 0).size());
}

// Tests that the Gmail extension does not match the offline verison.
TEST_F(ChromeLauncherControllerImplTest, GmailOfflineMatching) {
  InitLauncherControllerWithBrowser();

  // Create a Gmail browser tab.
  chrome::NewTab(browser());
  base::string16 title = ASCIIToUTF16("Test");
  NavigateAndCommitActiveTabWithTitle(browser(),
                                      GURL(offline_gmail_url),
                                      title);
  content::WebContents* content =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Installing |extension3_| adds it to the launcher.
  ash::ShelfID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_->item_count());
  int gmail_index = model_->ItemIndexByID(gmail_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[gmail_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));

  // The content should not be able to be handled by the app.
  EXPECT_FALSE(launcher_controller_->ContentCanBeHandledByGmailApp(content));
}

// Verify that the launcher item positions are persisted and restored.
TEST_F(ChromeLauncherControllerImplTest, PersistLauncherItemPositions) {
  InitLauncherController();

  TestLauncherControllerHelper* helper = new TestLauncherControllerHelper;
  SetLauncherControllerHelper(helper);

  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[1].type);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(0, tab_strip_model->count());
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  EXPECT_EQ(2, tab_strip_model->count());
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  helper->SetAppID(tab_strip_model->GetWebContentsAt(1), "2");

  EXPECT_FALSE(launcher_controller_->IsAppPinned("1"));
  launcher_controller_->PinAppWithID("1");
  EXPECT_TRUE(launcher_controller_->IsAppPinned("1"));
  launcher_controller_->PinAppWithID("2");

  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[3].type);

  // Move browser shortcut item from index 1 to index 3.
  model_->Move(1, 3);
  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[3].type);

  launcher_controller_.reset();
  while (!model_->items().empty())
    model_->RemoveItemAt(0);

  AddAppListLauncherItem();
  launcher_controller_ =
      base::MakeUnique<ChromeLauncherControllerImpl>(profile(), model_);
  helper = new TestLauncherControllerHelper(profile());
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  helper->SetAppID(tab_strip_model->GetWebContentsAt(1), "2");
  SetLauncherControllerHelper(helper);
  launcher_controller_->Init();

  // Check ShelfItems are restored after resetting ChromeLauncherControllerImpl.
  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[3].type);
}

// Verifies pinned apps are persisted and restored.
TEST_F(ChromeLauncherControllerImplTest, PersistPinned) {
  InitLauncherControllerWithBrowser();
  size_t initial_size = model_->items().size();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model->count());

  TestLauncherControllerHelper* helper = new TestLauncherControllerHelper;
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  SetLauncherControllerHelper(helper);

  // app_icon_loader is owned by ChromeLauncherControllerImpl.
  TestAppIconLoaderImpl* app_icon_loader = new TestAppIconLoaderImpl;
  app_icon_loader->AddSupportedApp("1");
  SetAppIconLoader(std::unique_ptr<AppIconLoader>(app_icon_loader));
  EXPECT_EQ(0, app_icon_loader->fetch_count());

  launcher_controller_->PinAppWithID("1");
  ash::ShelfID id = launcher_controller_->GetShelfIDForAppID("1");
  int app_index = model_->ItemIndexByID(id);
  EXPECT_EQ(1, app_icon_loader->fetch_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[app_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned("1"));
  EXPECT_FALSE(launcher_controller_->IsAppPinned("0"));
  EXPECT_EQ(initial_size + 1, model_->items().size());

  launcher_controller_.reset();
  while (!model_->items().empty())
    model_->RemoveItemAt(0);

  AddAppListLauncherItem();
  launcher_controller_ =
      base::MakeUnique<ChromeLauncherControllerImpl>(profile(), model_);
  helper = new TestLauncherControllerHelper(profile());
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  SetLauncherControllerHelper(helper);
  // app_icon_loader is owned by ChromeLauncherControllerImpl.
  app_icon_loader = new TestAppIconLoaderImpl;
  app_icon_loader->AddSupportedApp("1");
  SetAppIconLoader(std::unique_ptr<AppIconLoader>(app_icon_loader));
  launcher_controller_->Init();

  EXPECT_EQ(1, app_icon_loader->fetch_count());
  ASSERT_EQ(initial_size + 1, model_->items().size());
  EXPECT_TRUE(launcher_controller_->IsAppPinned("1"));
  EXPECT_FALSE(launcher_controller_->IsAppPinned("0"));
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[app_index].type);

  launcher_controller_->UnpinAppWithID("1");
  ASSERT_EQ(initial_size, model_->items().size());
}

TEST_F(ChromeLauncherControllerImplTest, MultipleAppIconLoaders) {
  InitLauncherControllerWithBrowser();

  const std::string app_id1 = extension1_->id();
  const std::string app_id2 = extension2_->id();
  const std::string app_id3 = extension3_->id();
  // app_icon_loader1 and app_icon_loader2 are owned by
  // ChromeLauncherControllerImpl.
  TestAppIconLoaderImpl* app_icon_loader1 = new TestAppIconLoaderImpl();
  TestAppIconLoaderImpl* app_icon_loader2 = new TestAppIconLoaderImpl();
  app_icon_loader1->AddSupportedApp(app_id1);
  app_icon_loader2->AddSupportedApp(app_id2);
  SetAppIconLoaders(std::unique_ptr<AppIconLoader>(app_icon_loader1),
                    std::unique_ptr<AppIconLoader>(app_icon_loader2));

  AppWindowLauncherItemController* app_controller3 =
      new ExtensionAppWindowLauncherItemController(
          LauncherItemController::TYPE_APP, app_id3, "id",
          launcher_controller_.get());
  const ash::ShelfID shelfId3 = launcher_controller_->CreateAppLauncherItem(
      app_controller3, app_id3, ash::STATUS_RUNNING);
  EXPECT_EQ(0, app_icon_loader1->fetch_count());
  EXPECT_EQ(0, app_icon_loader1->clear_count());
  EXPECT_EQ(0, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  AppWindowLauncherItemController* app_controller2 =
      new ExtensionAppWindowLauncherItemController(
          LauncherItemController::TYPE_APP, app_id2, "id",
          launcher_controller_.get());
  const ash::ShelfID shelfId2 = launcher_controller_->CreateAppLauncherItem(
      app_controller2, app_id2, ash::STATUS_RUNNING);
  EXPECT_EQ(0, app_icon_loader1->fetch_count());
  EXPECT_EQ(0, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  AppWindowLauncherItemController* app_controller1 =
      new ExtensionAppWindowLauncherItemController(
          LauncherItemController::TYPE_APP, app_id1, "id",
          launcher_controller_.get());

  const ash::ShelfID shelfId1 = launcher_controller_->CreateAppLauncherItem(
      app_controller1, app_id1, ash::STATUS_RUNNING);
  EXPECT_EQ(1, app_icon_loader1->fetch_count());
  EXPECT_EQ(0, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  launcher_controller_->CloseLauncherItem(shelfId1);
  EXPECT_EQ(1, app_icon_loader1->fetch_count());
  EXPECT_EQ(1, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  launcher_controller_->CloseLauncherItem(shelfId2);
  EXPECT_EQ(1, app_icon_loader1->fetch_count());
  EXPECT_EQ(1, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(1, app_icon_loader2->clear_count());

  launcher_controller_->CloseLauncherItem(shelfId3);
  EXPECT_EQ(1, app_icon_loader1->fetch_count());
  EXPECT_EQ(1, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(1, app_icon_loader2->clear_count());
}

TEST_F(ChromeLauncherControllerImplWithArcTest, ArcAppPinPolicy) {
  InitLauncherControllerWithBrowser();
  arc::mojom::AppInfo appinfo = CreateAppInfo(
      "Some App", "SomeActivity", "com.example.app", OrientationLock::NONE);
  const std::string app_id = AddArcAppAndShortcut(appinfo);

  // Set policy, that makes pins ARC app. Unlike native extension, for ARC app
  // package_name (not hash) specified as id. In this test we check that
  // by hash we can determine that appropriate package was set by policy.
  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, appinfo.package_name);
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.DeepCopy());

  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(app_id, profile()));
}

TEST_F(ChromeLauncherControllerImplWithArcTest, ArcManaged) {
  extension_service_->AddExtension(arc_support_host_.get());
  // Test enables Arc, so turn it off for initial values.
  EnableArc(false);

  InitLauncherController();
  arc::ArcSessionManager::SetShelfDelegateForTesting(
      launcher_controller_.get());

  // Initial run, Arc is not managed and disabled, Play Store pin should be
  // available.
  ValidateArcState(false, false, arc::ArcSessionManager::State::STOPPED,
                   "AppList, Chrome, Play Store");

  // Arc is managed and enabled, Play Store pin should be available.
  // Note: SHOWING_TERMS_OF_SERVICE here means that opt-in flow starts.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, new base::FundamentalValue(true));
  base::RunLoop().RunUntilIdle();
  ValidateArcState(true, true,
                   arc::ArcSessionManager::State::SHOWING_TERMS_OF_SERVICE,
                   "AppList, Chrome, Play Store");

  // Arc is managed and disabled, Play Store pin should not be available.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, new base::FundamentalValue(false));
  base::RunLoop().RunUntilIdle();
  ValidateArcState(false, true, arc::ArcSessionManager::State::STOPPED,
                   "AppList, Chrome");

  // Arc is not managed and disabled, Play Store pin should be available.
  profile()->GetTestingPrefService()->RemoveManagedPref(prefs::kArcEnabled);
  base::RunLoop().RunUntilIdle();
  ValidateArcState(false, false, arc::ArcSessionManager::State::STOPPED,
                   "AppList, Chrome, Play Store");

  // Arc is not managed and enabled, Play Store pin should be available.
  EnableArc(true);
  ValidateArcState(true, false,
                   arc::ArcSessionManager::State::SHOWING_TERMS_OF_SERVICE,
                   "AppList, Chrome, Play Store");

  // User disables Arc. Arc is not managed and disabled, Play Store pin should
  // be automatically removed.
  EnableArc(false);
  ValidateArcState(false, false, arc::ArcSessionManager::State::STOPPED,
                   "AppList, Chrome");

  // Even if re-enable it again, Play Store pin does not appear automatically.
  EnableArc(true);
  ValidateArcState(true, false,
                   arc::ArcSessionManager::State::SHOWING_TERMS_OF_SERVICE,
                   "AppList, Chrome");
}

namespace {

class ChromeLauncherControllerOrientationTest
    : public ChromeLauncherControllerImplWithArcTest {
 public:
  ChromeLauncherControllerOrientationTest() {}
  ~ChromeLauncherControllerOrientationTest() override {}

 protected:
  void InitApps() {
    appinfo_none_ =
        CreateAppInfo("None", "None", "com.example.app", OrientationLock::NONE);
    appinfo_landscape_ =
        CreateAppInfo("Landscape", "Landscape", "com.example.app",
                      OrientationLock::LANDSCAPE);
    appinfo_portrait_ = CreateAppInfo("Portrait", "Portrait", "com.example.app",
                                      OrientationLock::PORTRAIT);
    appinfo_current_ = CreateAppInfo(
        "LockCurrent", "current", "com.example.app", OrientationLock::CURRENT);

    AddArcAppAndShortcut(appinfo_none_);
    AddArcAppAndShortcut(appinfo_landscape_);
    AddArcAppAndShortcut(appinfo_portrait_);
    AddArcAppAndShortcut(appinfo_current_);

    ash::ScreenOrientationController* controller =
        ash::Shell::GetInstance()->screen_orientation_controller();

    // Creating a window with NONE orientation will not lock the screen.
    window_none_ = CreateArcWindow(window_app_id_none_);
    NotifyOnTaskCreated(appinfo_none_, task_id_none_);
    EXPECT_FALSE(controller->rotation_locked());
    EXPECT_EQ(display::Display::ROTATE_0,
              display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

    // Create a arc window with PORTRAIT orientation locks the screen to 90.
    window_portrait_ = CreateArcWindow(window_app_id_portrait_);
    NotifyOnTaskCreated(appinfo_portrait_, task_id_portrait_);
    EXPECT_TRUE(controller->rotation_locked());
    EXPECT_EQ(display::Display::ROTATE_90,
              display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

    // Create a arc window with LANDSCAPE orientation locks the screen to 0.
    window_landscape_ = CreateArcWindow(window_app_id_landscape_);
    NotifyOnTaskCreated(appinfo_landscape_, task_id_landscape_);
    EXPECT_TRUE(controller->rotation_locked());
    EXPECT_EQ(display::Display::ROTATE_0,
              display::Screen::GetScreen()->GetPrimaryDisplay().rotation());
  }

  int32_t task_id_none_ = 1;
  int32_t task_id_landscape_ = 2;
  int32_t task_id_portrait_ = 3;
  int32_t task_id_current_ = 4;

  // This needs to be kept on the instance because window's property has
  // refeference to this.
  std::string window_app_id_none_ = {"org.chromium.arc.1"};
  std::string window_app_id_landscape_ = {"org.chromium.arc.2"};
  std::string window_app_id_portrait_ = {"org.chromium.arc.3"};
  std::string window_app_id_current_ = {"org.chromium.arc.4"};

  arc::mojom::AppInfo appinfo_none_;
  arc::mojom::AppInfo appinfo_landscape_;
  arc::mojom::AppInfo appinfo_portrait_;
  arc::mojom::AppInfo appinfo_current_;

  views::Widget* window_none_ = nullptr;
  views::Widget* window_landscape_ = nullptr;
  views::Widget* window_portrait_ = nullptr;
  views::Widget* window_current_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerOrientationTest);
};

class ChromeLauncherControllerArcDefaultAppsTest
    : public ChromeLauncherControllerImplTest {
 public:
  ChromeLauncherControllerArcDefaultAppsTest() {}
  ~ChromeLauncherControllerArcDefaultAppsTest() override {}

 protected:
  void SetUp() override {
    ArcDefaultAppList::UseTestAppsDirectory();
    ChromeLauncherControllerImplTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerArcDefaultAppsTest);
};

}  // namespace

TEST_F(ChromeLauncherControllerOrientationTest,
       ArcOrientationLockBeforeWindowReady) {
  ASSERT_TRUE(display::Display::HasInternalDisplay());

  extension_service_->AddExtension(arc_support_host_.get());
  EnableArc(true);

  InitLauncherController();
  arc::ArcSessionManager::SetShelfDelegateForTesting(
      launcher_controller_.get());

  ash::ScreenOrientationController* controller =
      ash::Shell::GetInstance()->screen_orientation_controller();

  std::string app_id1("org.chromium.arc.1");
  int task_id1 = 1;
  arc::mojom::AppInfo appinfo1 =
      CreateAppInfo("Test1", "test", "com.example.app", OrientationLock::NONE);

  AddArcAppAndShortcut(appinfo1);
  NotifyOnTaskCreated(appinfo1, task_id1);
  NotifyOnTaskOrientationLockRequested(task_id1, OrientationLock::PORTRAIT);

  // Widgets will be deleted by the system.
  CreateArcWindow(app_id1);

  EXPECT_FALSE(controller->rotation_locked());

  EnableTabletMode(true);
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_90,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  std::string app_id2("org.chromium.arc.2");
  int task_id2 = 2;
  arc::mojom::AppInfo appinfo2 =
      CreateAppInfo("Test2", "test", "com.example.app", OrientationLock::NONE);
  // Create in tablet mode.
  AddArcAppAndShortcut(appinfo2);
  NotifyOnTaskCreated(appinfo2, task_id2);
  NotifyOnTaskOrientationLockRequested(task_id2, OrientationLock::LANDSCAPE);
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_90,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // The screen will be locked when the window is created.
  CreateArcWindow(app_id2);
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_0,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());
}

TEST_F(ChromeLauncherControllerOrientationTest, ArcOrientationLock) {
  ASSERT_TRUE(display::Display::HasInternalDisplay());

  extension_service_->AddExtension(arc_support_host_.get());
  EnableArc(true);
  EnableTabletMode(true);

  InitLauncherController();
  arc::ArcSessionManager::SetShelfDelegateForTesting(
      launcher_controller_.get());

  InitApps();
  ash::ScreenOrientationController* controller =
      ash::Shell::GetInstance()->screen_orientation_controller();

  // Activating a window with NON orientation unlocks the screen.
  window_none_->Activate();
  EXPECT_FALSE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_0,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // Activating a window with PORTRAIT orientation locks the screen to 90.
  window_portrait_->Activate();
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_90,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // Disable Tablet mode, and make sure the screen is unlocked.
  EnableTabletMode(false);
  EXPECT_FALSE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_0,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // Re-enable Tablet mode, and make sure the screen is locked to 90.
  EnableTabletMode(true);
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_90,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  window_portrait_->Activate();
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_90,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  window_landscape_->Activate();
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_0,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // OnTaskOrientationLockRequested can overwrite the current lock.
  NotifyOnTaskOrientationLockRequested(task_id_landscape_,
                                       OrientationLock::NONE);
  EXPECT_FALSE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_0,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  NotifyOnTaskOrientationLockRequested(task_id_landscape_,
                                       OrientationLock::PORTRAIT);
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_90,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // Non active window won't change the lock.
  NotifyOnTaskOrientationLockRequested(task_id_none_,
                                       OrientationLock::LANDSCAPE);
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_90,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // But activating it will change the locked orinetation.
  window_none_->Activate();
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_0,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // OnTaskOrientationLockRequested will not lock the screen in non Tablet mode.
  EnableTabletMode(false);
  EXPECT_FALSE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_0,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  NotifyOnTaskOrientationLockRequested(task_id_none_,
                                       OrientationLock::PORTRAIT);
  EXPECT_FALSE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_0,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // But it remembers the orientation lock and use it when Tablet mode is
  // enabled.
  EnableTabletMode(true);
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_90,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // Manually unlock first.
  NotifyOnTaskOrientationLockRequested(task_id_none_, OrientationLock::NONE);
  EXPECT_FALSE(controller->rotation_locked());
}

TEST_F(ChromeLauncherControllerOrientationTest, CurrentWithLandscapeDisplay) {
  ASSERT_TRUE(display::Display::HasInternalDisplay());

  extension_service_->AddExtension(arc_support_host_.get());
  EnableArc(true);
  EnableTabletMode(true);

  InitLauncherController();
  arc::ArcSessionManager::SetShelfDelegateForTesting(
      launcher_controller_.get());

  InitApps();
  ash::ScreenOrientationController* controller =
      ash::Shell::GetInstance()->screen_orientation_controller();

  // Start with portrait.
  window_portrait_->Activate();

  // Create a arc window to lock the CURRENT orientation.
  views::Widget* window_current = CreateArcWindow(window_app_id_current_);
  NotifyOnTaskCreated(appinfo_current_, task_id_current_);
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_90,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // Re-activating changes the orientation to previously locked orientation.
  window_landscape_->Activate();
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_0,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());
  window_current->Activate();
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_90,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // Exitting and re-entering tablet mode re-locks the orientation.
  EnableTabletMode(false);
  EXPECT_FALSE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_0,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  EnableTabletMode(true);
  EXPECT_TRUE(window_current->IsActive());
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_90,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // Manually unlock, and lock again at landscape.
  NotifyOnTaskOrientationLockRequested(task_id_current_, OrientationLock::NONE);
  window_landscape_->Activate();
  EXPECT_TRUE(controller->rotation_locked());
  window_current->Activate();
  EXPECT_FALSE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_0,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  NotifyOnTaskOrientationLockRequested(task_id_current_,
                                       OrientationLock::CURRENT);
  EXPECT_TRUE(controller->rotation_locked());
  EXPECT_EQ(display::Display::ROTATE_0,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());
}

TEST_F(ChromeLauncherControllerArcDefaultAppsTest, DefaultApps) {
  arc_test_.SetUp(profile());
  InitLauncherController();
  ChromeLauncherController::set_instance_for_test(launcher_controller_.get());
  arc::ArcSessionManager::SetShelfDelegateForTesting(
      launcher_controller_.get());

  ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
  EnableArc(false);
  EXPECT_FALSE(arc_test_.arc_session_manager()->IsArcEnabled());
  ASSERT_TRUE(prefs->GetAppIds().size());

  const std::string app_id =
      ArcAppTest::GetAppId(arc_test_.fake_default_apps()[0]);
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(app_id));
  EXPECT_TRUE(arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(arc_test_.arc_session_manager()->IsArcEnabled());
  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(app_id));

  // Stop Arc again. Shelf item should go away.
  EnableArc(false);
  EXPECT_EQ(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(app_id));

  EXPECT_TRUE(arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(arc_test_.arc_session_manager()->IsArcEnabled());

  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(app_id));
  EXPECT_TRUE(launcher_controller_->GetArcDeferredLauncher()->HasApp(app_id));

  std::string window_app_id("org.chromium.arc.1");
  CreateArcWindow(window_app_id);
  arc_test_.app_instance()->SendTaskCreated(1, arc_test_.fake_default_apps()[0],
                                            std::string());

  EXPECT_NE(ash::kInvalidShelfID,
            launcher_controller_->GetShelfIDForAppID(app_id));
  EXPECT_FALSE(launcher_controller_->GetArcDeferredLauncher()->HasApp(app_id));
}

// Checks the case when several app items have the same ordinal position (which
// is valid case).
TEST_F(ChromeLauncherControllerImplTest, CheckPositionConflict) {
  InitLauncherController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension3_.get());

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list, 1, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 1, extension3_->id());
  SendPinChanges(sync_list, true);

  EXPECT_EQ("AppList, Chrome, App1, App2, App3", GetPinnedAppStatus());

  const syncer::StringOrdinal position_chrome =
      app_service_->GetPinPosition(extension_misc::kChromeAppId);
  const syncer::StringOrdinal position_1 =
      app_service_->GetPinPosition(extension1_->id());
  const syncer::StringOrdinal position_2 =
      app_service_->GetPinPosition(extension2_->id());
  const syncer::StringOrdinal position_3 =
      app_service_->GetPinPosition(extension3_->id());
  EXPECT_TRUE(position_chrome.LessThan(position_1));
  EXPECT_TRUE(position_1.Equals(position_2));
  EXPECT_TRUE(position_2.Equals(position_3));

  // Move Chrome between App1 and App2.
  // Note, move target_index is in context when moved element is removed from
  // array first.
  model_->Move(1, 2);
  EXPECT_EQ("AppList, App1, Chrome, App2, App3", GetPinnedAppStatus());

  // Expect sync positions for only Chrome is updated and its resolution is
  // after all duplicated ordinals.
  EXPECT_TRUE(position_3.LessThan(
      app_service_->GetPinPosition(extension_misc::kChromeAppId)));
  EXPECT_TRUE(
      position_1.Equals(app_service_->GetPinPosition(extension1_->id())));
  EXPECT_TRUE(
      position_1.Equals(app_service_->GetPinPosition(extension1_->id())));
  EXPECT_TRUE(
      position_2.Equals(app_service_->GetPinPosition(extension2_->id())));
  EXPECT_TRUE(
      position_3.Equals(app_service_->GetPinPosition(extension3_->id())));
}

// Test the case when sync app is turned off and we need to use local copy to
// support user's pins.
TEST_F(ChromeLauncherControllerImplTest, SyncOffLocalUpdate) {
  InitLauncherController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list, 1, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  SendPinChanges(sync_list, true);

  EXPECT_EQ("AppList, Chrome, App1, App2", GetPinnedAppStatus());

  syncer::SyncDataList copy_sync_list =
      app_service_->GetAllSyncData(syncer::APP_LIST);

  app_service_->StopSyncing(syncer::APP_LIST);
  RecreateChromeLauncher();

  // Pinned state should not change.
  EXPECT_EQ("AppList, Chrome, App1, App2", GetPinnedAppStatus());
  launcher_controller_->UnpinAppWithID(extension2_->id());
  EXPECT_EQ("AppList, Chrome, App1", GetPinnedAppStatus());

  // Resume syncing and sync information overrides local copy.
  StartAppSyncService(copy_sync_list);
  EXPECT_EQ("AppList, Chrome, App1, App2", GetPinnedAppStatus());
}
