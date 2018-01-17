// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/sessions/sessions_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/sync/device_info/local_device_info_provider_mock.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/model/attachments/attachment_id.h"
#include "components/sync/model/attachments/attachment_service_proxy_for_test.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync_sessions/sessions_sync_manager.h"
#include "extensions/browser/api_test_utils.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

namespace utils = extension_function_test_utils;

namespace extensions {

namespace {

// Fake session tabs (used to construct arbitrary device info) and tab IDs
// (used to construct arbitrary tab info) to use in all tests.
const char* const kSessionTags[] = {"tag0", "tag1", "tag2", "tag3", "tag4"};
const SessionID::id_type kTabIDs[] = {5, 10, 13, 17};

void BuildSessionSpecifics(const std::string& tag,
                           sync_pb::SessionSpecifics* meta) {
  meta->set_session_tag(tag);
  sync_pb::SessionHeader* header = meta->mutable_header();
  header->set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
  header->set_client_name(tag);
}

void BuildWindowSpecifics(int window_id,
                          const std::vector<int>& tab_list,
                          sync_pb::SessionSpecifics* meta) {
  sync_pb::SessionHeader* header = meta->mutable_header();
  sync_pb::SessionWindow* window = header->add_window();
  window->set_window_id(window_id);
  window->set_selected_tab_index(0);
  window->set_browser_type(sync_pb::SessionWindow_BrowserType_TYPE_TABBED);
  for (std::vector<int>::const_iterator iter = tab_list.begin();
       iter != tab_list.end(); ++iter) {
    window->add_tab(*iter);
  }
}

void BuildTabSpecifics(const std::string& tag,
                       int tab_id,
                       sync_pb::SessionSpecifics* tab_base) {
  tab_base->set_session_tag(tag);
  tab_base->set_tab_node_id(0);
  sync_pb::SessionTab* tab = tab_base->mutable_tab();
  tab->set_tab_id(tab_id);
  tab->set_tab_visual_index(1);
  tab->set_current_navigation_index(0);
  tab->set_pinned(true);
  tab->set_extension_app_id("app_id");
  sync_pb::TabNavigation* navigation = tab->add_navigation();
  navigation->set_virtual_url("http://foo/1");
  navigation->set_favicon_url("http://foo/favicon.ico");
  navigation->set_referrer("MyReferrer");
  navigation->set_title("MyTitle");
  navigation->set_page_transition(sync_pb::SyncEnums_PageTransition_TYPED);
}

testing::AssertionResult CheckSessionModels(const base::ListValue& devices,
                                            size_t num_sessions) {
  EXPECT_EQ(5u, devices.GetSize());
  const base::DictionaryValue* device = NULL;
  const base::ListValue* sessions = NULL;
  for (size_t i = 0; i < devices.GetSize(); ++i) {
    EXPECT_TRUE(devices.GetDictionary(i, &device));
    EXPECT_EQ(kSessionTags[i], api_test_utils::GetString(device, "info"));
    EXPECT_EQ(kSessionTags[i], api_test_utils::GetString(device, "deviceName"));
    EXPECT_TRUE(device->GetList("sessions", &sessions));
    EXPECT_EQ(num_sessions, sessions->GetSize());
    // Because this test is hurried, really there are only ever 0 or 1
    // sessions, and if 1, that will be a Window. Grab it.
    if (num_sessions == 0)
      continue;
    const base::DictionaryValue* session = NULL;
    EXPECT_TRUE(sessions->GetDictionary(0, &session));
    const base::DictionaryValue* window = NULL;
    EXPECT_TRUE(session->GetDictionary("window", &window));
    // Only the tabs are interesting.
    const base::ListValue* tabs = NULL;
    EXPECT_TRUE(window->GetList("tabs", &tabs));
    EXPECT_EQ(arraysize(kTabIDs), tabs->GetSize());
    for (size_t j = 0; j < tabs->GetSize(); ++j) {
      const base::DictionaryValue* tab = NULL;
      EXPECT_TRUE(tabs->GetDictionary(j, &tab));
      EXPECT_FALSE(tab->HasKey("id"));  // sessions API does not give tab IDs
      EXPECT_EQ(static_cast<int>(j), api_test_utils::GetInteger(tab, "index"));
      EXPECT_EQ(0, api_test_utils::GetInteger(tab, "windowId"));
      // Test setup code always sets tab 0 to selected (which means active in
      // extension terminology).
      EXPECT_EQ(j == 0, api_test_utils::GetBoolean(tab, "active"));
      // While selected/highlighted are different to active, and should always
      // be false.
      EXPECT_FALSE(api_test_utils::GetBoolean(tab, "selected"));
      EXPECT_FALSE(api_test_utils::GetBoolean(tab, "highlighted"));
      EXPECT_FALSE(api_test_utils::GetBoolean(tab, "incognito"));
      EXPECT_TRUE(api_test_utils::GetBoolean(tab, "pinned"));
      EXPECT_EQ("http://foo/1", api_test_utils::GetString(tab, "url"));
      EXPECT_EQ("MyTitle", api_test_utils::GetString(tab, "title"));
      EXPECT_EQ("http://foo/favicon.ico",
                api_test_utils::GetString(tab, "favIconUrl"));
      EXPECT_EQ(base::StringPrintf("%s.%d", kSessionTags[i], kTabIDs[j]),
                api_test_utils::GetString(tab, "sessionId"));
    }
  }
  return testing::AssertionSuccess();
}

}  // namespace

class ExtensionSessionsTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 protected:
  static std::unique_ptr<KeyedService> BuildProfileSyncService(
      content::BrowserContext* profile);

  void CreateTestProfileSyncService();
  void CreateTestExtension();
  void CreateSessionModels();

  template <class T>
  scoped_refptr<T> CreateFunction(bool has_callback) {
    scoped_refptr<T> fn(new T());
    fn->set_extension(extension_.get());
    fn->set_has_callback(has_callback);
    return fn;
  }

  Browser* browser_;
  scoped_refptr<Extension> extension_;
};

void ExtensionSessionsTest::SetUpCommandLine(base::CommandLine* command_line) {
#if defined(OS_CHROMEOS)
  command_line->AppendSwitch(
      chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
}

void ExtensionSessionsTest::SetUpOnMainThread() {
  CreateTestProfileSyncService();
  CreateTestExtension();
}

std::unique_ptr<KeyedService> ExtensionSessionsTest::BuildProfileSyncService(
    content::BrowserContext* context) {
  std::unique_ptr<syncer::SyncApiComponentFactoryMock> factory(
      new syncer::SyncApiComponentFactoryMock());

  factory->SetLocalDeviceInfoProvider(
      std::unique_ptr<syncer::LocalDeviceInfoProvider>(
          new syncer::LocalDeviceInfoProviderMock(
              kSessionTags[0], "machine name", "Chromium 10k", "Chrome 10k",
              sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id")));

  Profile* profile = static_cast<Profile*>(context);
  browser_sync::ProfileSyncServiceMock* sync_service =
      new browser_sync::ProfileSyncServiceMock(
          CreateProfileSyncServiceParamsForTest(
              std::make_unique<browser_sync::ChromeSyncClient>(profile),
              profile));
  static_cast<browser_sync::ChromeSyncClient*>(sync_service->GetSyncClient())
      ->SetSyncApiComponentFactoryForTesting(std::move(factory));
  return base::WrapUnique(sync_service);
}

void ExtensionSessionsTest::CreateTestProfileSyncService() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.AppendASCII("test_profile");
  if (!base::PathExists(path))
    CHECK(base::CreateDirectory(path));
  Profile* profile =
      Profile::CreateProfile(path, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
  profile_manager->RegisterTestingProfile(profile, true, false);
  browser_sync::ProfileSyncServiceMock* service =
      static_cast<browser_sync::ProfileSyncServiceMock*>(
          ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              profile, &ExtensionSessionsTest::BuildProfileSyncService));

  syncer::ModelTypeSet preferred_types(syncer::SESSIONS, syncer::PROXY_TABS);
  GoogleServiceAuthError no_error(GoogleServiceAuthError::NONE);
  ON_CALL(*service, IsDataTypeControllerRunning(syncer::SESSIONS))
      .WillByDefault(testing::Return(true));
  ON_CALL(*service, IsDataTypeControllerRunning(syncer::PROXY_TABS))
      .WillByDefault(testing::Return(true));
  ON_CALL(*service, GetRegisteredDataTypes())
      .WillByDefault(testing::Return(syncer::UserTypes()));
  ON_CALL(*service, GetPreferredDataTypes()).WillByDefault(
      testing::Return(preferred_types));
  EXPECT_CALL(*service, GetAuthError()).WillRepeatedly(
      testing::ReturnRef(no_error));
  ON_CALL(*service, GetActiveDataTypes()).WillByDefault(
      testing::Return(preferred_types));

  EXPECT_CALL(*service, AddObserver(testing::_)).Times(testing::AnyNumber());
  EXPECT_CALL(*service, RemoveObserver(testing::_)).Times(testing::AnyNumber());

  browser_ = new Browser(Browser::CreateParams(profile, true));

  service->Initialize();
}

void ExtensionSessionsTest::CreateTestExtension() {
  std::unique_ptr<base::DictionaryValue> test_extension_value(
      api_test_utils::ParseDictionary(
          "{\"name\": \"Test\", \"version\": \"1.0\", "
          "\"permissions\": [\"sessions\", \"tabs\"]}"));
  extension_ = api_test_utils::CreateExtension(test_extension_value.get());
}

void ExtensionSessionsTest::CreateSessionModels() {
  syncer::SyncDataList initial_data;
  for (size_t index = 0; index < arraysize(kSessionTags); ++index) {
    // Fill an instance of session specifics with a foreign session's data.
    sync_pb::SessionSpecifics meta;
    BuildSessionSpecifics(kSessionTags[index], &meta);
    std::vector<SessionID::id_type> tab_list(kTabIDs,
                                             kTabIDs + arraysize(kTabIDs));
    BuildWindowSpecifics(index, tab_list, &meta);
    std::vector<sync_pb::SessionSpecifics> tabs(tab_list.size());
    for (size_t i = 0; i < tab_list.size(); ++i) {
      BuildTabSpecifics(kSessionTags[index], tab_list[i], &tabs[i]);
    }

    sync_pb::EntitySpecifics entity;
    entity.mutable_session()->CopyFrom(meta);
    initial_data.push_back(syncer::SyncData::CreateRemoteData(
        1, entity, base::Time(), syncer::AttachmentIdList(),
        syncer::AttachmentServiceProxyForTest::Create(),
        sync_sessions::SessionsSyncManager::TagHashFromSpecifics(
            entity.session())));
    for (size_t i = 0; i < tabs.size(); i++) {
      sync_pb::EntitySpecifics entity;
      entity.mutable_session()->CopyFrom(tabs[i]);
      initial_data.push_back(syncer::SyncData::CreateRemoteData(
          i + 2, entity, base::Time(), syncer::AttachmentIdList(),
          syncer::AttachmentServiceProxyForTest::Create(),
          sync_sessions::SessionsSyncManager::TagHashFromSpecifics(
              entity.session())));
    }
  }

  ProfileSyncServiceFactory::GetForProfile(browser_->profile())
      ->GetSessionsSyncableService()
      ->MergeDataAndStartSyncing(syncer::SESSIONS, initial_data,
                                 std::unique_ptr<syncer::SyncChangeProcessor>(
                                     new syncer::FakeSyncChangeProcessor()),
                                 std::unique_ptr<syncer::SyncErrorFactory>(
                                     new syncer::SyncErrorFactoryMock()));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetDevices) {
  CreateSessionModels();
  std::unique_ptr<base::ListValue> result(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetDevicesFunction>(true).get(),
          "[{\"maxResults\": 0}]", browser_)));
  ASSERT_TRUE(result);
  EXPECT_TRUE(CheckSessionModels(*result, 0u));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetDevicesMaxResults) {
  CreateSessionModels();
  std::unique_ptr<base::ListValue> result(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetDevicesFunction>(true).get(), "[]",
          browser_)));
  ASSERT_TRUE(result);
  EXPECT_TRUE(CheckSessionModels(*result, 1u));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetDevicesListEmpty) {
  std::unique_ptr<base::ListValue> result(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetDevicesFunction>(true).get(), "[]",
          browser_)));

  ASSERT_TRUE(result);
  base::ListValue* devices = result.get();
  EXPECT_EQ(0u, devices->GetSize());
}

// Flaky timeout: http://crbug.com/278372
IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest,
                       DISABLED_RestoreForeignSessionWindow) {
  CreateSessionModels();

  std::unique_ptr<base::DictionaryValue> restored_window_session(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsRestoreFunction>(true).get(), "[\"tag3.3\"]",
          browser_, utils::INCLUDE_INCOGNITO)));
  ASSERT_TRUE(restored_window_session);

  std::unique_ptr<base::ListValue> result(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<WindowsGetAllFunction>(true).get(), "[]", browser_)));
  ASSERT_TRUE(result);

  base::ListValue* windows = result.get();
  EXPECT_EQ(2u, windows->GetSize());
  base::DictionaryValue* restored_window = NULL;
  EXPECT_TRUE(restored_window_session->GetDictionary("window",
                                                     &restored_window));
  base::DictionaryValue* window = NULL;
  int restored_id = api_test_utils::GetInteger(restored_window, "id");
  for (size_t i = 0; i < windows->GetSize(); ++i) {
    EXPECT_TRUE(windows->GetDictionary(i, &window));
    if (api_test_utils::GetInteger(window, "id") == restored_id)
      break;
  }
  EXPECT_EQ(restored_id, api_test_utils::GetInteger(window, "id"));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreForeignSessionInvalidId) {
  CreateSessionModels();

  EXPECT_TRUE(base::MatchPattern(utils::RunFunctionAndReturnError(
      CreateFunction<SessionsRestoreFunction>(true).get(),
      "[\"tag3.0\"]",
      browser_), "Invalid session id: \"tag3.0\"."));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreInIncognito) {
  CreateSessionModels();

  EXPECT_TRUE(base::MatchPattern(utils::RunFunctionAndReturnError(
      CreateFunction<SessionsRestoreFunction>(true).get(),
      "[\"1\"]",
      CreateIncognitoBrowser()),
      "Can not restore sessions in incognito mode."));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetRecentlyClosedIncognito) {
  std::unique_ptr<base::ListValue> result(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetRecentlyClosedFunction>(true).get(), "[]",
          CreateIncognitoBrowser())));
  ASSERT_TRUE(result);
  base::ListValue* sessions = result.get();
  EXPECT_EQ(0u, sessions->GetSize());
}

// http://crbug.com/251199
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_SessionsApis) {
  ASSERT_TRUE(RunExtensionSubtest("sessions",
                                  "sessions.html")) << message_;
}

}  // namespace extensions
