// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace {

class FakeProfileSyncService : public ProfileSyncService {
 public:
  explicit FakeProfileSyncService(Profile* profile)
      : ProfileSyncService(
            NULL,
            profile,
            NULL,
            ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
            ProfileSyncService::MANUAL_START),
        sync_initialized_(true),
        initialized_state_violation_(false) {}

  virtual ~FakeProfileSyncService() {}

  static BrowserContextKeyedService* BuildFakeProfileSyncService(
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
  PreferencesPrivateApiTest() : service_(NULL) {}
  virtual ~PreferencesPrivateApiTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    service_ =  static_cast<FakeProfileSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), &FakeProfileSyncService::BuildFakeProfileSyncService));
    browser_sync::SyncPrefs sync_prefs(profile()->GetPrefs());
    sync_prefs.SetKeepEverythingSynced(false);
  }

  FakeProfileSyncService* service() { return service_; }

 private:
  FakeProfileSyncService* service_;
  DISALLOW_COPY_AND_ASSIGN(PreferencesPrivateApiTest);
};

IN_PROC_BROWSER_TEST_F(PreferencesPrivateApiTest,
                       GetSyncCategoriesWithoutPassphraseSynchronous) {
  ASSERT_TRUE(RunComponentExtensionTest("preferences_private")) << message_;
  EXPECT_FALSE(service()->initialized_state_violation());
}

// Verifies that we wait for the sync service to be ready before checking
// encryption status.
IN_PROC_BROWSER_TEST_F(PreferencesPrivateApiTest,
                       GetSyncCategoriesWithoutPassphraseAsynchronous) {
  service()->set_sync_initialized(false);
  ASSERT_TRUE(RunComponentExtensionTest("preferences_private")) << message_;
  EXPECT_FALSE(service()->initialized_state_violation());
}

}  // namespace
