// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/glue/sync_backend_host_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/sync_driver/data_type_manager_impl.h"
#include "components/sync_driver/sync_prefs.h"
#include "sync/test/engine/test_id_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;
class ProfileOAuth2TokenService;
class ProfileSyncComponentsFactory;
class ProfileSyncComponentsFactoryMock;

ACTION(ReturnNewDataTypeManager) {
  return new sync_driver::DataTypeManagerImpl(base::Closure(),
                                              arg0,
                                              arg1,
                                              arg2,
                                              arg3,
                                              arg4,
                                              arg5);
}

namespace browser_sync {

class SyncBackendHostForProfileSyncTest : public SyncBackendHostImpl {
 public:
  SyncBackendHostForProfileSyncTest(
      Profile* profile,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
      base::Closure callback);
  virtual ~SyncBackendHostForProfileSyncTest();

  virtual void RequestConfigureSyncer(
      syncer::ConfigureReason reason,
      syncer::ModelTypeSet to_download,
      syncer::ModelTypeSet to_purge,
      syncer::ModelTypeSet to_journal,
      syncer::ModelTypeSet to_unapply,
      syncer::ModelTypeSet to_ignore,
      const syncer::ModelSafeRoutingInfo& routing_info,
      const base::Callback<void(syncer::ModelTypeSet,
                                syncer::ModelTypeSet)>& ready_task,
      const base::Closure& retry_callback) OVERRIDE;

 protected:
  virtual void InitCore(scoped_ptr<DoInitializeOptions> options) OVERRIDE;

 private:
  // Invoked at the start of HandleSyncManagerInitializationOnFrontendLoop.
  // Allows extra initialization work to be performed before the backend comes
  // up.
  base::Closure callback_;
};

}  // namespace browser_sync

class TestProfileSyncService : public ProfileSyncService {
 public:
  // TODO(tim): Add ability to inject TokenService alongside SigninManager.
  // TODO(rogerta): what does above comment mean?
  TestProfileSyncService(
      scoped_ptr<ProfileSyncComponentsFactory> factory,
      Profile* profile,
      SigninManagerBase* signin,
      ProfileOAuth2TokenService* oauth2_token_service,
      browser_sync::ProfileSyncServiceStartBehavior behavior);

  virtual ~TestProfileSyncService();

  virtual void OnConfigureDone(
      const sync_driver::DataTypeManager::ConfigureResult& result) OVERRIDE;

  // We implement our own version to avoid some DCHECKs.
  virtual syncer::UserShare* GetUserShare() const OVERRIDE;

  static TestProfileSyncService* BuildAutoStartAsyncInit(
      Profile* profile, base::Closure callback);

  ProfileSyncComponentsFactoryMock* components_factory_mock();

  syncer::TestIdFactory* id_factory();

  // Raise visibility to ease testing.
  using ProfileSyncService::NotifyObservers;

 protected:
  static KeyedService* TestFactoryFunction(content::BrowserContext* profile);

  // Return NULL handle to use in backend initialization to avoid receiving
  // js messages on UI loop when it's being destroyed, which are not deleted
  // and cause memory leak in test.
  virtual syncer::WeakHandle<syncer::JsEventHandler> GetJsEventHandler()
      OVERRIDE;

  virtual bool NeedBackup() const OVERRIDE;

 private:
  syncer::TestIdFactory id_factory_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
