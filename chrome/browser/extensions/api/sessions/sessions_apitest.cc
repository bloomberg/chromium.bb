// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sessions/sessions_api.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/glue/local_device_info_provider_mock.h"
#include "chrome/browser/sync/open_tabs_ui_delegate.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/attachments/attachment_service_proxy_for_test.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_error_factory_mock.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

namespace utils = extension_function_test_utils;

namespace extensions {

namespace {

// If more sessions are added to session tags, num sessions should be updated.
const char* kSessionTags[] = {"tag0", "tag1", "tag2", "tag3", "tag4"};
const size_t kNumSessions = 5;

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

void BuildTabSpecifics(const std::string& tag, int window_id, int tab_id,
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
  navigation->set_referrer("referrer");
  navigation->set_title("title");
  navigation->set_page_transition(sync_pb::SyncEnums_PageTransition_TYPED);
}

} // namespace

class ExtensionSessionsTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;
 protected:
  static KeyedService* BuildProfileSyncService(
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
  };

  Browser* browser_;
  scoped_refptr<extensions::Extension> extension_;
};

void ExtensionSessionsTest::SetUpCommandLine(CommandLine* command_line) {
#if defined(OS_CHROMEOS)
  command_line->AppendSwitch(
      chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
}

void ExtensionSessionsTest::SetUpOnMainThread() {
  CreateTestProfileSyncService();
  CreateTestExtension();
}

KeyedService* ExtensionSessionsTest::BuildProfileSyncService(
    content::BrowserContext* profile) {

  ProfileSyncComponentsFactoryMock* factory =
      new ProfileSyncComponentsFactoryMock();

  factory->SetLocalDeviceInfoProvider(
      scoped_ptr<LocalDeviceInfoProvider>(
          new browser_sync::LocalDeviceInfoProviderMock(
              kSessionTags[0],
              "machine name",
              "Chromium 10k",
              "Chrome 10k",
              sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
              "device_id")));

  return new ProfileSyncServiceMock(
      scoped_ptr<ProfileSyncComponentsFactory>(factory),
      static_cast<Profile*>(profile));
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
  ProfileSyncServiceMock* service = static_cast<ProfileSyncServiceMock*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile, &ExtensionSessionsTest::BuildProfileSyncService));
  browser_ = new Browser(Browser::CreateParams(
      profile, chrome::HOST_DESKTOP_TYPE_NATIVE));

  syncer::ModelTypeSet preferred_types;
  preferred_types.Put(syncer::SESSIONS);
  GoogleServiceAuthError no_error(GoogleServiceAuthError::NONE);
  ON_CALL(*service, IsSessionsDataTypeControllerRunning())
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

  service->Initialize();
}

void ExtensionSessionsTest::CreateTestExtension() {
  scoped_ptr<base::DictionaryValue> test_extension_value(
      utils::ParseDictionary(
      "{\"name\": \"Test\", \"version\": \"1.0\", "
      "\"permissions\": [\"sessions\", \"tabs\"]}"));
  extension_ = utils::CreateExtension(test_extension_value.get());
}

void ExtensionSessionsTest::CreateSessionModels() {
  syncer::SyncDataList initial_data;
  for (size_t index = 0; index < kNumSessions; ++index) {
    // Fill an instance of session specifics with a foreign session's data.
    sync_pb::SessionSpecifics meta;
    BuildSessionSpecifics(kSessionTags[index], &meta);
    SessionID::id_type tab_nums1[] = {5, 10, 13, 17};
    std::vector<SessionID::id_type> tab_list1(
        tab_nums1, tab_nums1 + arraysize(tab_nums1));
    BuildWindowSpecifics(index, tab_list1, &meta);
    std::vector<sync_pb::SessionSpecifics> tabs1;
    tabs1.resize(tab_list1.size());
    for (size_t i = 0; i < tab_list1.size(); ++i) {
      BuildTabSpecifics(kSessionTags[index], 0, tab_list1[i], &tabs1[i]);
    }

    sync_pb::EntitySpecifics entity;
    entity.mutable_session()->CopyFrom(meta);
    initial_data.push_back(syncer::SyncData::CreateRemoteData(
        1,
        entity,
        base::Time(),
        syncer::AttachmentIdList(),
        syncer::AttachmentServiceProxyForTest::Create()));
    for (size_t i = 0; i < tabs1.size(); i++) {
      sync_pb::EntitySpecifics entity;
      entity.mutable_session()->CopyFrom(tabs1[i]);
      initial_data.push_back(syncer::SyncData::CreateRemoteData(
          i + 2,
          entity,
          base::Time(),
          syncer::AttachmentIdList(),
          syncer::AttachmentServiceProxyForTest::Create()));
    }
  }

  ProfileSyncServiceFactory::GetForProfile(browser_->profile())->
      GetSessionsSyncableService()->
          MergeDataAndStartSyncing(syncer::SESSIONS, initial_data,
      scoped_ptr<syncer::SyncChangeProcessor>(
          new syncer::FakeSyncChangeProcessor()),
      scoped_ptr<syncer::SyncErrorFactory>(
          new syncer::SyncErrorFactoryMock()));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetDevices) {
  CreateSessionModels();

  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetDevicesFunction>(true).get(),
          "[{\"maxResults\": 0}]",
          browser_)));
  ASSERT_TRUE(result);
  base::ListValue* devices = result.get();
  EXPECT_EQ(5u, devices->GetSize());
  base::DictionaryValue* device = NULL;
  base::ListValue* sessions = NULL;
  for (size_t i = 0; i < devices->GetSize(); ++i) {
    EXPECT_TRUE(devices->GetDictionary(i, &device));
    EXPECT_EQ(kSessionTags[i], utils::GetString(device, "info"));
    EXPECT_EQ(kSessionTags[i], utils::GetString(device, "deviceName"));
    EXPECT_TRUE(device->GetList("sessions", &sessions));
    EXPECT_EQ(0u, sessions->GetSize());
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetDevicesMaxResults) {
  CreateSessionModels();

  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetDevicesFunction>(true).get(),
          "[]",
          browser_)));
  ASSERT_TRUE(result);
  base::ListValue* devices = result.get();
  EXPECT_EQ(5u, devices->GetSize());
  base::DictionaryValue* device = NULL;
  base::ListValue* sessions = NULL;
  for (size_t i = 0; i < devices->GetSize(); ++i) {
    EXPECT_TRUE(devices->GetDictionary(i, &device));
    EXPECT_EQ(kSessionTags[i], utils::GetString(device, "info"));
    EXPECT_EQ(kSessionTags[i], utils::GetString(device, "deviceName"));
    EXPECT_TRUE(device->GetList("sessions", &sessions));
    EXPECT_EQ(1u, sessions->GetSize());
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetDevicesListEmpty) {
  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetDevicesFunction>(true).get(),
          "[]",
          browser_)));

  ASSERT_TRUE(result);
  base::ListValue* devices = result.get();
  EXPECT_EQ(0u, devices->GetSize());
}

// Flaky timeout: http://crbug.com/278372
IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest,
                       DISABLED_RestoreForeignSessionWindow) {
  CreateSessionModels();

  scoped_ptr<base::DictionaryValue> restored_window_session(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsRestoreFunction>(true).get(),
          "[\"tag3.3\"]",
          browser_,
          utils::INCLUDE_INCOGNITO)));
  ASSERT_TRUE(restored_window_session);

  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(
          CreateFunction<WindowsGetAllFunction>(true).get(),
          "[]",
          browser_)));
  ASSERT_TRUE(result);

  base::ListValue* windows = result.get();
  EXPECT_EQ(2u, windows->GetSize());
  base::DictionaryValue* restored_window = NULL;
  EXPECT_TRUE(restored_window_session->GetDictionary("window",
                                                     &restored_window));
  base::DictionaryValue* window = NULL;
  int restored_id = utils::GetInteger(restored_window, "id");
  for (size_t i = 0; i < windows->GetSize(); ++i) {
    EXPECT_TRUE(windows->GetDictionary(i, &window));
    if (utils::GetInteger(window, "id") == restored_id)
      break;
  }
  EXPECT_EQ(restored_id, utils::GetInteger(window, "id"));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreForeignSessionInvalidId) {
  CreateSessionModels();

  EXPECT_TRUE(MatchPattern(utils::RunFunctionAndReturnError(
      CreateFunction<SessionsRestoreFunction>(true).get(),
      "[\"tag3.0\"]",
      browser_), "Invalid session id: \"tag3.0\"."));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreInIncognito) {
  CreateSessionModels();

  EXPECT_TRUE(MatchPattern(utils::RunFunctionAndReturnError(
      CreateFunction<SessionsRestoreFunction>(true).get(),
      "[\"1\"]",
      CreateIncognitoBrowser()),
      "Can not restore sessions in incognito mode."));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetRecentlyClosedIncognito) {
  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetRecentlyClosedFunction>(true).get(),
          "[]",
          CreateIncognitoBrowser())));
  ASSERT_TRUE(result);
  base::ListValue* sessions = result.get();
  EXPECT_EQ(0u, sessions->GetSize());
}

// Flaky on ChromeOS, times out on OSX Debug http://crbug.com/251199
#if defined(OS_CHROMEOS) || (defined(OS_MACOSX) && !defined(NDEBUG))
#define MAYBE_SessionsApis DISABLED_SessionsApis
#else
#define MAYBE_SessionsApis SessionsApis
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_SessionsApis) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  ASSERT_TRUE(RunExtensionSubtest("sessions",
                                  "sessions.html")) << message_;
}

}  // namespace extensions
