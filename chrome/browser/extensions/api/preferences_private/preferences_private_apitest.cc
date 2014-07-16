// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/preferences_private/preferences_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

using extensions::PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction;

namespace {

class FakeProfileSyncService : public ProfileSyncService {
 public:
  explicit FakeProfileSyncService(Profile* profile)
      : ProfileSyncService(
            scoped_ptr<ProfileSyncComponentsFactory>(
                new ProfileSyncComponentsFactoryMock()),
            profile,
            make_scoped_ptr<SupervisedUserSigninManagerWrapper>(NULL),
            ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
            browser_sync::MANUAL_START),
        sync_initialized_(true),
        initialized_state_violation_(false) {}

  virtual ~FakeProfileSyncService() {}

  static KeyedService* BuildFakeProfileSyncService(
      content::BrowserContext* context) {
    return new FakeProfileSyncService(static_cast<Profile*>(context));
  }

  void set_sync_initialized(bool sync_initialized) {
    sync_initialized_ = sync_initialized;
  }

  bool initialized_state_violation() { return initialized_state_violation_; }

  // ProfileSyncService:
  virtual bool sync_initialized() const OVERRIDE {
    return sync_initialized_;
  }

  virtual void AddObserver(
      ProfileSyncServiceBase::Observer* observer) OVERRIDE {
    if (sync_initialized_)
      initialized_state_violation_ = true;
    // Set sync initialized state to true so the function will run after
    // OnStateChanged is called.
    sync_initialized_ = true;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ProfileSyncServiceBase::Observer::OnStateChanged,
                   base::Unretained(observer)));
  }

  virtual syncer::ModelTypeSet GetEncryptedDataTypes() const OVERRIDE {
    if (!sync_initialized_)
      initialized_state_violation_ = true;
    syncer::ModelTypeSet type_set;
    type_set.Put(syncer::AUTOFILL);
    return type_set;
  }

  virtual syncer::ModelTypeSet GetPreferredDataTypes() const OVERRIDE {
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
  virtual ~PreferencesPrivateApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  virtual void SetUpOnMainThread() OVERRIDE {
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

    browser_ = new Browser(Browser::CreateParams(
        profile, chrome::HOST_DESKTOP_TYPE_NATIVE));
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
      function,
      "[]",
      browser_,
      extension_function_test_utils::NONE));
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
