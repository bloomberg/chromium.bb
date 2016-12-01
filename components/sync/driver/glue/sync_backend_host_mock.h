// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_MOCK_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_MOCK_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/driver/glue/sync_backend_host.h"

namespace syncer {

// A mock of the SyncBackendHost.
//
// This class implements the bare minimum required for the ProfileSyncService to
// get through initialization.  It often returns null pointers or nonesense
// values; it is not intended to be used in tests that depend on SyncBackendHost
// behavior.
class SyncBackendHostMock : public SyncBackendHost {
 public:
  SyncBackendHostMock();
  ~SyncBackendHostMock() override;

  void Initialize(
      SyncFrontend* frontend,
      scoped_refptr<base::SingleThreadTaskRunner> sync_task_runner,
      const WeakHandle<JsEventHandler>& event_handler,
      const GURL& service_url,
      const std::string& sync_user_agent,
      const SyncCredentials& credentials,
      bool delete_sync_data_folder,
      bool enable_local_sync_backend,
      const base::FilePath& local_sync_backend_folder,
      std::unique_ptr<SyncManagerFactory> sync_manager_factory,
      const WeakHandle<UnrecoverableErrorHandler>& unrecoverable_error_handler,
      const base::Closure& report_unrecoverable_error_function,
      const HttpPostProviderFactoryGetter& http_post_provider_factory_getter,
      std::unique_ptr<SyncEncryptionHandler::NigoriState> saved_nigori_state)
      override;

  void TriggerRefresh(const ModelTypeSet& types) override;

  void UpdateCredentials(const SyncCredentials& credentials) override;

  void StartSyncingWithServer() override;

  void SetEncryptionPassphrase(const std::string& passphrase,
                               bool is_explicit) override;

  bool SetDecryptionPassphrase(const std::string& passphrase) override;

  void StopSyncingForShutdown() override;

  void Shutdown(ShutdownReason reason) override;

  void UnregisterInvalidationIds() override;

  ModelTypeSet ConfigureDataTypes(
      ConfigureReason reason,
      const DataTypeConfigStateMap& config_state_map,
      const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task,
      const base::Callback<void()>& retry_callback) override;

  void EnableEncryptEverything() override;

  void ActivateDirectoryDataType(ModelType type,
                                 ModelSafeGroup group,
                                 ChangeProcessor* change_processor) override;
  void DeactivateDirectoryDataType(ModelType type) override;

  void ActivateNonBlockingDataType(ModelType type,
                                   std::unique_ptr<ActivationContext>) override;
  void DeactivateNonBlockingDataType(ModelType type) override;

  UserShare* GetUserShare() const override;

  Status GetDetailedStatus() override;

  SyncCycleSnapshot GetLastCycleSnapshot() const override;

  bool HasUnsyncedItems() const override;

  bool IsNigoriEnabled() const override;

  PassphraseType GetPassphraseType() const override;

  base::Time GetExplicitPassphraseTime() const override;

  bool IsCryptographerReady(const BaseTransaction* trans) const override;

  void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) const override;

  void FlushDirectory() const override;

  void RequestBufferedProtocolEventsAndEnableForwarding() override;
  void DisableProtocolEventForwarding() override;

  void EnableDirectoryTypeDebugInfoForwarding() override;
  void DisableDirectoryTypeDebugInfoForwarding() override;

  void RefreshTypesForTest(ModelTypeSet types) override;

  void ClearServerData(
      const SyncManager::ClearServerDataCallback& callback) override;

  void OnCookieJarChanged(bool account_mismatch, bool empty_jar) override;

  void set_fail_initial_download(bool should_fail);

 private:
  bool fail_initial_download_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_MOCK_H_
