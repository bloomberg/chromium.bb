// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/invalidations/invalidator_storage.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/test/base/profile_mock.h"
#include "sync/internal_api/public/test/test_internal_components_factory.h"
#include "sync/test/engine/test_id_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;
class Task;
class TestProfileSyncService;

ACTION(ReturnNewDataTypeManager) {
  return new browser_sync::DataTypeManagerImpl(arg0, arg1, arg2);
}

namespace browser_sync {

class SyncBackendHostForProfileSyncTest : public SyncBackendHost {
 public:
  // |synchronous_init| causes initialization to block until the syncapi has
  //     completed setting itself up and called us back.
  // TOOD(akalin): Remove |synchronous_init| (http://crbug.com/140354).
  SyncBackendHostForProfileSyncTest(
      Profile* profile,
      const base::WeakPtr<SyncPrefs>& sync_prefs,
      const base::WeakPtr<InvalidatorStorage>& invalidator_storage,
      syncer::TestIdFactory& id_factory,
      base::Closure& callback,
      bool set_initial_sync_ended_on_init,
      bool synchronous_init,
      bool fail_initial_download,
      syncer::StorageOption storage_option);
  virtual ~SyncBackendHostForProfileSyncTest();

  MOCK_METHOD1(RequestNudge, void(const tracked_objects::Location&));

  virtual void RequestConfigureSyncer(
      syncer::ConfigureReason reason,
      syncer::ModelTypeSet types_to_config,
      const syncer::ModelSafeRoutingInfo& routing_info,
      const base::Callback<void(syncer::ModelTypeSet)>& ready_task,
      const base::Closure& retry_callback) OVERRIDE;

  virtual void HandleSyncManagerInitializationOnFrontendLoop(
      const syncer::WeakHandle<syncer::JsBackend>& js_backend,
      bool success,
      syncer::ModelTypeSet restored_types) OVERRIDE;

  static void SetHistoryServiceExpectations(ProfileMock* profile);

  void SetInitialSyncEndedForAllTypes();

  void EmitOnInvalidatorStateChange(syncer::InvalidatorState state);
  void EmitOnIncomingInvalidation(
      const syncer::ObjectIdStateMap& id_state_map,
      const syncer::IncomingInvalidationSource source);

 protected:
  virtual void InitCore(const DoInitializeOptions& options) OVERRIDE;

 private:
  syncer::TestIdFactory& id_factory_;
  base::Closure& callback_;

  bool set_initial_sync_ended_on_init_;
  bool synchronous_init_;
  bool fail_initial_download_;
  syncer::StorageOption storage_option_;
};

}  // namespace browser_sync

class TestProfileSyncService : public ProfileSyncService {
 public:
  // |callback| can be used to populate nodes before the OnBackendInitialized
  // callback fires.
  // TODO(tim): Remove |synchronous_backend_initialization|, and add ability to
  // inject TokenService alongside SigninManager.
  TestProfileSyncService(
      ProfileSyncComponentsFactory* factory,
      Profile* profile,
      SigninManager* signin,
      ProfileSyncService::StartBehavior behavior,
      bool synchronous_backend_initialization,
      const base::Closure& callback);

  virtual ~TestProfileSyncService();

  virtual void OnBackendInitialized(
      const syncer::WeakHandle<syncer::JsBackend>& backend,
      bool success) OVERRIDE;

  virtual void OnConfigureDone(
      const browser_sync::DataTypeManager::ConfigureResult& result) OVERRIDE;

  // We implement our own version to avoid some DCHECKs.
  virtual syncer::UserShare* GetUserShare() const OVERRIDE;

  // If this is called, configuring data types will require a syncer
  // nudge.
  void dont_set_initial_sync_ended_on_init();
  void set_synchronous_sync_configuration();

  void fail_initial_download();
  void set_storage_option(syncer::StorageOption option);

  syncer::TestIdFactory* id_factory();

  // Override of ProfileSyncService::GetBackendForTest() with a more
  // specific return type (since C++ supports covariant return types)
  // that is made public.
  virtual browser_sync::SyncBackendHostForProfileSyncTest*
      GetBackendForTest() OVERRIDE;

 protected:
  virtual void CreateBackend() OVERRIDE;

 private:
  syncer::TestIdFactory id_factory_;

  bool synchronous_backend_initialization_;

  // Set to true when a mock data type manager is being used and the configure
  // step is performed synchronously.
  bool synchronous_sync_configuration_;

  base::Closure callback_;
  bool set_initial_sync_ended_on_init_;

  bool fail_initial_download_;
  syncer::StorageOption storage_option_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
