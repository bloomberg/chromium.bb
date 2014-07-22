// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_host_mock.h"

#include "components/sync_driver/sync_frontend.h"

namespace browser_sync {

const char kTestCacheGuid[] = "test-guid";

SyncBackendHostMock::SyncBackendHostMock() : fail_initial_download_(false) {}
SyncBackendHostMock::~SyncBackendHostMock() {}

void SyncBackendHostMock::Initialize(
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
    syncer::NetworkResources* network_resources) {
  frontend->OnBackendInitialized(
      syncer::WeakHandle<syncer::JsBackend>(),
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(),
      kTestCacheGuid,
      !fail_initial_download_);
}

void SyncBackendHostMock::UpdateCredentials(
    const syncer::SyncCredentials& credentials) {}

void SyncBackendHostMock::StartSyncingWithServer() {}

void SyncBackendHostMock::SetEncryptionPassphrase(
    const std::string& passphrase,
    bool is_explicit) {}

bool SyncBackendHostMock::SetDecryptionPassphrase(
    const std::string& passphrase) {
  return false;
}

void SyncBackendHostMock::StopSyncingForShutdown() {}

scoped_ptr<base::Thread> SyncBackendHostMock::Shutdown(ShutdownOption option) {
  return scoped_ptr<base::Thread>();
}

void SyncBackendHostMock::UnregisterInvalidationIds() {}

void SyncBackendHostMock::ConfigureDataTypes(
    syncer::ConfigureReason reason,
    const DataTypeConfigStateMap& config_state_map,
    const base::Callback<void(syncer::ModelTypeSet,
                              syncer::ModelTypeSet)>& ready_task,
    const base::Callback<void()>& retry_callback) {}

void SyncBackendHostMock::EnableEncryptEverything() {}

void SyncBackendHostMock::ActivateDataType(
    syncer::ModelType type, syncer::ModelSafeGroup group,
    sync_driver::ChangeProcessor* change_processor) {}
void SyncBackendHostMock::DeactivateDataType(syncer::ModelType type) {}

syncer::UserShare* SyncBackendHostMock::GetUserShare() const {
  return NULL;
}

scoped_ptr<syncer::SyncContextProxy>
SyncBackendHostMock::GetSyncContextProxy() {
  return scoped_ptr<syncer::SyncContextProxy>();
}

SyncBackendHost::Status SyncBackendHostMock::GetDetailedStatus() {
  return SyncBackendHost::Status();
}

syncer::sessions::SyncSessionSnapshot
    SyncBackendHostMock::GetLastSessionSnapshot() const {
  return syncer::sessions::SyncSessionSnapshot();
}

bool SyncBackendHostMock::HasUnsyncedItems() const {
  return false;
}

bool SyncBackendHostMock::IsNigoriEnabled() const {
 return false;
}

syncer::PassphraseType SyncBackendHostMock::GetPassphraseType() const {
  return syncer::IMPLICIT_PASSPHRASE;
}

base::Time SyncBackendHostMock::GetExplicitPassphraseTime() const {
  return base::Time();
}

bool SyncBackendHostMock::IsCryptographerReady(
    const syncer::BaseTransaction* trans) const {
  return false;
}

void SyncBackendHostMock::GetModelSafeRoutingInfo(
    syncer::ModelSafeRoutingInfo* out) const {}

SyncedDeviceTracker* SyncBackendHostMock::GetSyncedDeviceTracker() const {
  return NULL;
}

base::MessageLoop* SyncBackendHostMock::GetSyncLoopForTesting() {
  return NULL;
}

void SyncBackendHostMock::RequestBufferedProtocolEventsAndEnableForwarding() {}

void SyncBackendHostMock::DisableProtocolEventForwarding() {}

void SyncBackendHostMock::EnableDirectoryTypeDebugInfoForwarding() {}

void SyncBackendHostMock::DisableDirectoryTypeDebugInfoForwarding() {}

void SyncBackendHostMock::GetAllNodesForTypes(
    syncer::ModelTypeSet types,
    base::Callback<void(const std::vector<syncer::ModelType>& type,
                        ScopedVector<base::ListValue>) > callback) {}

void SyncBackendHostMock::set_fail_initial_download(bool should_fail) {
  fail_initial_download_ = should_fail;
}

}  // namespace browser_sync

