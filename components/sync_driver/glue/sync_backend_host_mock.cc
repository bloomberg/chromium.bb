// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/glue/sync_backend_host_mock.h"

#include "components/sync_driver/sync_frontend.h"
#include "sync/internal_api/public/activation_context.h"

namespace browser_sync {

const char kTestCacheGuid[] = "test-guid";

SyncBackendHostMock::SyncBackendHostMock() : fail_initial_download_(false) {}
SyncBackendHostMock::~SyncBackendHostMock() {}

void SyncBackendHostMock::Initialize(
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
        saved_nigori_state) {
  frontend->OnBackendInitialized(
      syncer::WeakHandle<syncer::JsBackend>(),
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(),
      kTestCacheGuid,
      !fail_initial_download_);
}

void SyncBackendHostMock::TriggerRefresh(const syncer::ModelTypeSet& types) {}

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

std::unique_ptr<base::Thread> SyncBackendHostMock::Shutdown(
    syncer::ShutdownReason reason) {
  return std::unique_ptr<base::Thread>();
}

void SyncBackendHostMock::UnregisterInvalidationIds() {}

syncer::ModelTypeSet SyncBackendHostMock::ConfigureDataTypes(
    syncer::ConfigureReason reason,
    const DataTypeConfigStateMap& config_state_map,
    const base::Callback<void(syncer::ModelTypeSet, syncer::ModelTypeSet)>&
        ready_task,
    const base::Callback<void()>& retry_callback) {
  return syncer::ModelTypeSet();
}

void SyncBackendHostMock::EnableEncryptEverything() {}

void SyncBackendHostMock::ActivateDirectoryDataType(
    syncer::ModelType type,
    syncer::ModelSafeGroup group,
    sync_driver::ChangeProcessor* change_processor) {}
void SyncBackendHostMock::DeactivateDirectoryDataType(syncer::ModelType type) {}

void SyncBackendHostMock::ActivateNonBlockingDataType(
    syncer::ModelType type,
    std::unique_ptr<syncer_v2::ActivationContext> activation_context) {}

void SyncBackendHostMock::DeactivateNonBlockingDataType(
    syncer::ModelType type) {}

syncer::UserShare* SyncBackendHostMock::GetUserShare() const {
  return NULL;
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
  return true;
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

void SyncBackendHostMock::FlushDirectory() const {}

base::MessageLoop* SyncBackendHostMock::GetSyncLoopForTesting() {
  return NULL;
}

void SyncBackendHostMock::RefreshTypesForTest(syncer::ModelTypeSet types) {}

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

void SyncBackendHostMock::ClearServerData(
    const syncer::SyncManager::ClearServerDataCallback& callback) {
  callback.Run();
}

void SyncBackendHostMock::OnCookieJarChanged(bool account_mismatch) {}

}  // namespace browser_sync
