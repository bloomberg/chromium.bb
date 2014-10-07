// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_MOCK_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_MOCK_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
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
  virtual ~SyncBackendHostMock();

  virtual void Initialize(
      sync_driver::SyncFrontend* frontend,
      scoped_ptr<base::Thread> sync_thread,
      const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
      const GURL& service_url,
      const syncer::SyncCredentials& credentials,
      bool delete_sync_data_folder,
      scoped_ptr<syncer::SyncManagerFactory> sync_manager_factory,
      scoped_ptr<syncer::UnrecoverableErrorHandler> unrecoverable_error_handler,
      syncer::ReportUnrecoverableErrorFunction
          report_unrecoverable_error_function,
      syncer::NetworkResources* network_resources) override;

  virtual void UpdateCredentials(
      const syncer::SyncCredentials& credentials) override;

  virtual void StartSyncingWithServer() override;

  virtual void SetEncryptionPassphrase(
      const std::string& passphrase,
      bool is_explicit) override;

  virtual bool SetDecryptionPassphrase(
      const std::string& passphrase) override;

  virtual void StopSyncingForShutdown() override;

  virtual scoped_ptr<base::Thread> Shutdown(syncer::ShutdownReason reason)
      override;

  virtual void UnregisterInvalidationIds() override;

  virtual void ConfigureDataTypes(
      syncer::ConfigureReason reason,
      const DataTypeConfigStateMap& config_state_map,
      const base::Callback<void(syncer::ModelTypeSet,
                                syncer::ModelTypeSet)>& ready_task,
      const base::Callback<void()>& retry_callback) override;

  virtual void EnableEncryptEverything() override;

  virtual void ActivateDataType(
      syncer::ModelType type, syncer::ModelSafeGroup group,
      sync_driver::ChangeProcessor* change_processor) override;
  virtual void DeactivateDataType(syncer::ModelType type) override;

  virtual syncer::UserShare* GetUserShare() const override;

  virtual scoped_ptr<syncer::SyncContextProxy> GetSyncContextProxy() override;

  virtual Status GetDetailedStatus() override;

  virtual syncer::sessions::SyncSessionSnapshot
      GetLastSessionSnapshot() const override;

  virtual bool HasUnsyncedItems() const override;

  virtual bool IsNigoriEnabled() const override;

  virtual syncer::PassphraseType GetPassphraseType() const override;

  virtual base::Time GetExplicitPassphraseTime() const override;

  virtual bool IsCryptographerReady(
      const syncer::BaseTransaction* trans) const override;

  virtual void GetModelSafeRoutingInfo(
      syncer::ModelSafeRoutingInfo* out) const override;

  virtual void FlushDirectory() const override;

  virtual void RequestBufferedProtocolEventsAndEnableForwarding() override;
  virtual void DisableProtocolEventForwarding() override;

  virtual void EnableDirectoryTypeDebugInfoForwarding() override;
  virtual void DisableDirectoryTypeDebugInfoForwarding() override;

  virtual void GetAllNodesForTypes(
      syncer::ModelTypeSet types,
      base::Callback<void(const std::vector<syncer::ModelType>& type,
                          ScopedVector<base::ListValue>) > callback) override;

  virtual base::MessageLoop* GetSyncLoopForTesting() override;

  void set_fail_initial_download(bool should_fail);

 private:
  bool fail_initial_download_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_MOCK_H_
