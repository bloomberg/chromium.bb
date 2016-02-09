// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/preferences_private/preferences_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/sync_driver/signin_manager_wrapper.h"
#include "components/sync_driver/sync_api_component_factory_mock.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/browser/browser_context.h"
#include "extensions/test/extension_test_message_listener.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

using extensions::PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction;

namespace {

class FakeProfileSyncService : public ProfileSyncService {
 public:
  explicit FakeProfileSyncService(Profile* profile)
      : ProfileSyncService(CreateProfileSyncServiceParamsForTest(profile)),
        sync_initialized_(true),
        initialized_state_violation_(false) {}

  ~FakeProfileSyncService() override {}

  static scoped_ptr<KeyedService> BuildFakeProfileSyncService(
      content::BrowserContext* context) {
    return make_scoped_ptr(
        new FakeProfileSyncService(static_cast<Profile*>(context)));
  }

  void set_sync_initialized(bool sync_initialized) {
    sync_initialized_ = sync_initialized;
  }

  bool initialized_state_violation() { return initialized_state_violation_; }

  // ProfileSyncService:
  bool IsSyncActive() const override { return sync_initialized_; }

  void AddObserver(sync_driver::SyncServiceObserver* observer) override {
    if (sync_initialized_)
      initialized_state_violation_ = true;
    // Set sync initialized state to true so the function will run after
    // OnStateChanged is called.
    sync_initialized_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&sync_driver::SyncServiceObserver::OnStateChanged,
                              base::Unretained(observer)));
  }

  syncer::ModelTypeSet GetEncryptedDataTypes() const override {
    if (!sync_initialized_)
      initialized_state_violation_ = true;
    syncer::ModelTypeSet type_set;
    type_set.Put(syncer::AUTOFILL);
    return type_set;
  }

  syncer::ModelTypeSet GetPreferredDataTypes() const override {
    if (!sync_initialized_)
      initialized_state_violation_ = true;
    syncer::ModelTypeSet preferred_types =
        syncer::UserSelectableTypes();
    preferred_types.Remove(syncer::TYPED_URLS);
    return preferred_types;
  }

 private:
  bool sync_initialized_;
  // Set to true if a function is called when sync_initialized is in an
  // unexpected state.
  mutable bool initialized_state_violation_;

  DISALLOW_COPY_AND_ASSIGN(FakeProfileSyncService);
};

class PreferencesPrivateApiTest : public ExtensionApiTest {
 public:
  PreferencesPrivateApiTest() : browser_(NULL), service_(NULL) {}
  ~PreferencesPrivateApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    base::FilePath path;
    PathService::Get(chrome::DIR_USER_DATA, &path);
    path = path.AppendASCII("test_profile");
    if (!base::PathExists(path))
      CHECK(base::CreateDirectory(path));

    Profile* profile =
        Profile::CreateProfile(path, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
    sync_driver::SyncPrefs sync_prefs(profile->GetPrefs());
    sync_prefs.SetKeepEverythingSynced(false);

    ProfileManager* profile_manager = g_browser_process->profile_manager();
    profile_manager->RegisterTestingProfile(profile, true, false);

    service_ = static_cast<FakeProfileSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile, &FakeProfileSyncService::BuildFakeProfileSyncService));

    browser_ = new Browser(Browser::CreateParams(profile));
  }

  // Calls GetSyncCategoriesWithoutPassphraseFunction and verifies that the
  // results returned are the expected ones.
  void TestGetSyncCategoriesWithoutPassphraseFunction();

 protected:
  Browser* browser_;
  FakeProfileSyncService* service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PreferencesPrivateApiTest);
};

void
PreferencesPrivateApiTest::TestGetSyncCategoriesWithoutPassphraseFunction() {
  scoped_refptr<PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction>
      function(
          new PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction);
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), "[]", browser_, extension_function_test_utils::NONE));
  EXPECT_FALSE(service_->initialized_state_violation());

  const base::ListValue* result = function->GetResultList();
  EXPECT_EQ(1u, result->GetSize());

  const base::ListValue* categories = NULL;
  ASSERT_TRUE(result->GetList(0, &categories));
  EXPECT_NE(categories->end(),
            categories->Find(base::StringValue(bookmarks::kBookmarksFileName)));
  EXPECT_NE(categories->end(),
            categories->Find(base::StringValue(chrome::kPreferencesFilename)));
  EXPECT_EQ(categories->end(),
           categories->Find(base::StringValue("Autofill"))) <<
               "Encrypted categories should not be present";
  EXPECT_EQ(categories->end(),
           categories->Find(base::StringValue("Typed URLs"))) <<
               "Unsynced categories should not be present";
}

IN_PROC_BROWSER_TEST_F(PreferencesPrivateApiTest,
                       GetSyncCategoriesWithoutPassphrase) {
  TestGetSyncCategoriesWithoutPassphraseFunction();
}

// Verifies that we wait for the sync service to be ready before checking
// encryption status.
IN_PROC_BROWSER_TEST_F(PreferencesPrivateApiTest,
                       GetSyncCategoriesWithoutPassphraseAsynchronous) {
  service_->set_sync_initialized(false);
  TestGetSyncCategoriesWithoutPassphraseFunction();
}

}  // namespace
