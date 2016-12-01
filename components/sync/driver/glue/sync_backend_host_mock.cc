// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_backend_host_mock.h"

#include "components/sync/driver/sync_frontend.h"
#include "components/sync/engine/activation_context.h"

namespace syncer {

const char kTestCacheGuid[] = "test-guid";

SyncBackendHostMock::SyncBackendHostMock() : fail_initial_download_(false) {}
SyncBackendHostMock::~SyncBackendHostMock() {}

void SyncBackendHostMock::Initialize(
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
    std::unique_ptr<SyncEncryptionHandler::NigoriState> saved_nigori_state) {
  frontend->OnBackendInitialized(WeakHandle<JsBackend>(),
                                 WeakHandle<DataTypeDebugInfoListener>(),
                                 kTestCacheGuid, !fail_initial_download_);
}

void SyncBackendHostMock::TriggerRefresh(const ModelTypeSet& types) {}

void SyncBackendHostMock::UpdateCredentials(
    const SyncCredentials& credentials) {}

void SyncBackendHostMock::StartSyncingWithServer() {}

void SyncBackendHostMock::SetEncryptionPassphrase(const std::string& passphrase,
                                                  bool is_explicit) {}

bool SyncBackendHostMock::SetDecryptionPassphrase(
    const std::string& passphrase) {
  return false;
}

void SyncBackendHostMock::StopSyncingForShutdown() {}

void SyncBackendHostMock::Shutdown(ShutdownReason reason) {}

void SyncBackendHostMock::UnregisterInvalidationIds() {}

ModelTypeSet SyncBackendHostMock::ConfigureDataTypes(
    ConfigureReason reason,
    const DataTypeConfigStateMap& config_state_map,
    const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task,
    const base::Callback<void()>& retry_callback) {
  return ModelTypeSet();
}

void SyncBackendHostMock::EnableEncryptEverything() {}

void SyncBackendHostMock::ActivateDirectoryDataType(
    ModelType type,
    ModelSafeGroup group,
    ChangeProcessor* change_processor) {}
void SyncBackendHostMock::DeactivateDirectoryDataType(ModelType type) {}

void SyncBackendHostMock::ActivateNonBlockingDataType(
    ModelType type,
    std::unique_ptr<ActivationContext> activation_context) {}

void SyncBackendHostMock::DeactivateNonBlockingDataType(ModelType type) {}

UserShare* SyncBackendHostMock::GetUserShare() const {
  return nullptr;
}

SyncBackendHost::Status SyncBackendHostMock::GetDetailedStatus() {
  return SyncBackendHost::Status();
}

SyncCycleSnapshot SyncBackendHostMock::GetLastCycleSnapshot() const {
  return SyncCycleSnapshot();
}

bool SyncBackendHostMock::HasUnsyncedItems() const {
  return false;
}

bool SyncBackendHostMock::IsNigoriEnabled() const {
  return true;
}

PassphraseType SyncBackendHostMock::GetPassphraseType() const {
  return PassphraseType::IMPLICIT_PASSPHRASE;
}

base::Time SyncBackendHostMock::GetExplicitPassphraseTime() const {
  return base::Time();
}

bool SyncBackendHostMock::IsCryptographerReady(
    const BaseTransaction* trans) const {
  return false;
}

void SyncBackendHostMock::GetModelSafeRoutingInfo(
    ModelSafeRoutingInfo* out) const {}

void SyncBackendHostMock::FlushDirectory() const {}

void SyncBackendHostMock::RefreshTypesForTest(ModelTypeSet types) {}

void SyncBackendHostMock::RequestBufferedProtocolEventsAndEnableForwarding() {}

void SyncBackendHostMock::DisableProtocolEventForwarding() {}

void SyncBackendHostMock::EnableDirectoryTypeDebugInfoForwarding() {}

void SyncBackendHostMock::DisableDirectoryTypeDebugInfoForwarding() {}

void SyncBackendHostMock::set_fail_initial_download(bool should_fail) {
  fail_initial_download_ = should_fail;
}

void SyncBackendHostMock::ClearServerData(
    const SyncManager::ClearServerDataCallback& callback) {
  callback.Run();
}

void SyncBackendHostMock::OnCookieJarChanged(bool account_mismatch,
                                             bool empty_jar) {}

}  // namespace syncer
