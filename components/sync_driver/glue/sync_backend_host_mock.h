// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_MOCK_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_MOCK_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "components/sync_driver/glue/sync_backend_host.h"
#include "sync/internal_api/public/util/weak_handle.h"

namespace browser_sync {

// A mock of the SyncBackendHost.
//
// This class implements the bare minimum required for the ProfileSyncService to
// get through initialization.  It often returns NULL pointers or nonesense
// values; it is not intended to be used in tests that depend on SyncBackendHost
// behavior.
class SyncBackendHostMock : public SyncBackendHost {
 public:
  SyncBackendHostMock();
  ~SyncBackendHostMock() override;

  void Initialize(
      sync_driver::SyncFrontend* frontend,
      std::unique_ptr<base::Thread> sync_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_thread,
      const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
      const GURL& service_url,
      const std::string& sync_user_agent,
      const syncer::SyncCredentials& credentials,
      bool delete_sync_data_folder,
      std::unique_ptr<syncer::SyncManagerFactory> sync_manager_factory,
      const syncer::WeakHandle<syncer::UnrecoverableErrorHandler>&
          unrecoverable_error_handler,
      const base::Closure& report_unrecoverable_error_function,
      const HttpPostProviderFactoryGetter& http_post_provider_factory_getter,
      std::unique_ptr<syncer::SyncEncryptionHandler::NigoriState>
          saved_nigori_state) override;

  void TriggerRefresh(const syncer::ModelTypeSet& types) override;

  void UpdateCredentials(const syncer::SyncCredentials& credentials) override;

  void StartSyncingWithServer() override;

  void SetEncryptionPassphrase(const std::string& passphrase,
                               bool is_explicit) override;

  bool SetDecryptionPassphrase(const std::string& passphrase) override;

  void StopSyncingForShutdown() override;

  std::unique_ptr<base::Thread> Shutdown(
      syncer::ShutdownReason reason) override;

  void UnregisterInvalidationIds() override;

  syncer::ModelTypeSet ConfigureDataTypes(
      syncer::ConfigureReason reason,
      const DataTypeConfigStateMap& config_state_map,
      const base::Callback<void(syncer::ModelTypeSet, syncer::ModelTypeSet)>&
          ready_task,
      const base::Callback<void()>& retry_callback) override;

  void EnableEncryptEverything() override;

  void ActivateDirectoryDataType(
      syncer::ModelType type,
      syncer::ModelSafeGroup group,
      sync_driver::ChangeProcessor* change_processor) override;
  void DeactivateDirectoryDataType(syncer::ModelType type) override;

  void ActivateNonBlockingDataType(
      syncer::ModelType type,
      std::unique_ptr<syncer_v2::ActivationContext>) override;
  void DeactivateNonBlockingDataType(syncer::ModelType type) override;

  syncer::UserShare* GetUserShare() const override;

  Status GetDetailedStatus() override;

  syncer::sessions::SyncSessionSnapshot GetLastSessionSnapshot() const override;

  bool HasUnsyncedItems() const override;

  bool IsNigoriEnabled() const override;

  syncer::PassphraseType GetPassphraseType() const override;

  base::Time GetExplicitPassphraseTime() const override;

  bool IsCryptographerReady(
      const syncer::BaseTransaction* trans) const override;

  void GetModelSafeRoutingInfo(
      syncer::ModelSafeRoutingInfo* out) const override;

  void FlushDirectory() const override;

  void RequestBufferedProtocolEventsAndEnableForwarding() override;
  void DisableProtocolEventForwarding() override;

  void EnableDirectoryTypeDebugInfoForwarding() override;
  void DisableDirectoryTypeDebugInfoForwarding() override;

  void GetAllNodesForTypes(
      syncer::ModelTypeSet types,
      base::Callback<void(const std::vector<syncer::ModelType>& type,
                          ScopedVector<base::ListValue>)> callback) override;

  base::MessageLoop* GetSyncLoopForTesting() override;

  void RefreshTypesForTest(syncer::ModelTypeSet types) override;

  void ClearServerData(
      const syncer::SyncManager::ClearServerDataCallback& callback) override;

  void OnCookieJarChanged(bool account_mismatch) override;

  void set_fail_initial_download(bool should_fail);

 private:
  bool fail_initial_download_;
};

}  // namespace browser_sync

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_MOCK_H_
