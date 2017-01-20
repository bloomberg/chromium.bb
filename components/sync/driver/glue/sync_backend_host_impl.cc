// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_backend_host_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/sync/base/experiments.h"
#include "components/sync/base/invalidation_helper.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/glue/sync_backend_host_core.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/activation_context.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/engine_components_factory_impl.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/net/http_bridge.h"
#include "components/sync/engine/sync_backend_registrar.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/engine/sync_manager_factory.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/syncable/base_transaction.h"

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncers involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

namespace syncer {

SyncBackendHostImpl::SyncBackendHostImpl(
    const std::string& name,
    SyncClient* sync_client,
    invalidation::InvalidationService* invalidator,
    const base::WeakPtr<SyncPrefs>& sync_prefs,
    const base::FilePath& sync_data_folder)
    : sync_client_(sync_client),
      name_(name),
      sync_prefs_(sync_prefs),
      invalidator_(invalidator),
      weak_ptr_factory_(this) {
  core_ = new SyncBackendHostCore(name_, sync_data_folder,
                                  weak_ptr_factory_.GetWeakPtr());
}

SyncBackendHostImpl::~SyncBackendHostImpl() {
  DCHECK(!core_.get() && !host_) << "Must call Shutdown before destructor.";
  DCHECK(!registrar_);
}

void SyncBackendHostImpl::Initialize(InitParams params) {
  CHECK(params.sync_task_runner);
  DCHECK(params.host);
  DCHECK(params.registrar);

  sync_task_runner_ = params.sync_task_runner;
  host_ = params.host;
  registrar_ = params.registrar.get();

  sync_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoInitialize, core_,
                            base::Passed(&params)));
}

void SyncBackendHostImpl::TriggerRefresh(const ModelTypeSet& types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoRefreshTypes, core_, types));
}

void SyncBackendHostImpl::UpdateCredentials(
    const SyncCredentials& credentials) {
  sync_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoUpdateCredentials, core_,
                            credentials));
}

void SyncBackendHostImpl::StartSyncingWithServer() {
  SDVLOG(1) << "SyncBackendHostImpl::StartSyncingWithServer called.";

  ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);

  sync_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoStartSyncing, core_,
                            routing_info, sync_prefs_->GetLastPollTime()));
}

void SyncBackendHostImpl::SetEncryptionPassphrase(const std::string& passphrase,
                                                  bool is_explicit) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!IsNigoriEnabled()) {
    NOTREACHED() << "SetEncryptionPassphrase must never be called when nigori"
                    " is disabled.";
    return;
  }

  // We should never be called with an empty passphrase.
  DCHECK(!passphrase.empty());

  // SetEncryptionPassphrase should never be called if we are currently
  // encrypted with an explicit passphrase.
  DCHECK(cached_passphrase_type_ == PassphraseType::KEYSTORE_PASSPHRASE ||
         cached_passphrase_type_ == PassphraseType::IMPLICIT_PASSPHRASE);

  // Post an encryption task on the syncer thread.
  sync_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoSetEncryptionPassphrase,
                            core_, passphrase, is_explicit));
}

bool SyncBackendHostImpl::SetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!IsNigoriEnabled()) {
    NOTREACHED() << "SetDecryptionPassphrase must never be called when nigori"
                    " is disabled.";
    return false;
  }

  // We should never be called with an empty passphrase.
  DCHECK(!passphrase.empty());

  // This should only be called when we have cached pending keys.
  DCHECK(cached_pending_keys_.has_blob());

  // Check the passphrase that was provided against our local cache of the
  // cryptographer's pending keys. If this was unsuccessful, the UI layer can
  // immediately call OnPassphraseRequired without showing the user a spinner.
  if (!CheckPassphraseAgainstCachedPendingKeys(passphrase))
    return false;

  // Post a decryption task on the syncer thread.
  sync_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoSetDecryptionPassphrase,
                            core_, passphrase));

  // Since we were able to decrypt the cached pending keys with the passphrase
  // provided, we immediately alert the UI layer that the passphrase was
  // accepted. This will avoid the situation where a user enters a passphrase,
  // clicks OK, immediately reopens the advanced settings dialog, and gets an
  // unnecessary prompt for a passphrase.
  // Note: It is not guaranteed that the passphrase will be accepted by the
  // syncer thread, since we could receive a new nigori node while the task is
  // pending. This scenario is a valid race, and SetDecryptionPassphrase can
  // trigger a new OnPassphraseRequired if it needs to.
  NotifyPassphraseAccepted();
  return true;
}

void SyncBackendHostImpl::StopSyncingForShutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Stop getting messages from the sync thread.
  weak_ptr_factory_.InvalidateWeakPtrs();
  // Immediately stop sending messages to the host.
  host_ = nullptr;

  registrar_->RequestWorkerStopOnUIThread();

  core_->ShutdownOnUIThread();
}

void SyncBackendHostImpl::Shutdown(ShutdownReason reason) {
  // StopSyncingForShutdown() (which nulls out |host_|) should be
  // called first.
  DCHECK(!host_);

  if (invalidation_handler_registered_) {
    CHECK(invalidator_->UpdateRegisteredInvalidationIds(this, ObjectIdSet()));
    invalidator_->UnregisterInvalidationHandler(this);
    invalidator_ = nullptr;
  }
  invalidation_handler_registered_ = false;

  model_type_connector_.reset();

  // Shut down and destroy SyncManager. SyncManager holds a pointer to
  // |registrar_| so its destruction must be sequenced before the destruction of
  // |registrar_|.
  sync_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoShutdown, core_, reason));
  core_ = nullptr;
  registrar_ = nullptr;
}

void SyncBackendHostImpl::ConfigureDataTypes(ConfigureParams params) {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoPurgeDisabledTypes, core_,
                 params.to_purge, params.to_journal, params.to_unapply));
  sync_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoConfigureSyncer, core_,
                            base::Passed(&params)));
}

void SyncBackendHostImpl::EnableEncryptEverything() {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoEnableEncryptEverything, core_));
}

void SyncBackendHostImpl::ActivateDirectoryDataType(
    ModelType type,
    ModelSafeGroup group,
    ChangeProcessor* change_processor) {
  registrar_->ActivateDataType(type, group, change_processor, GetUserShare());
}

void SyncBackendHostImpl::DeactivateDirectoryDataType(ModelType type) {
  registrar_->DeactivateDataType(type);
}

void SyncBackendHostImpl::ActivateNonBlockingDataType(
    ModelType type,
    std::unique_ptr<ActivationContext> activation_context) {
  registrar_->RegisterNonBlockingType(type);
  if (activation_context->model_type_state.initial_sync_done())
    registrar_->AddRestoredNonBlockingType(type);
  model_type_connector_->ConnectType(type, std::move(activation_context));
}

void SyncBackendHostImpl::DeactivateNonBlockingDataType(ModelType type) {
  model_type_connector_->DisconnectType(type);
}

UserShare* SyncBackendHostImpl::GetUserShare() const {
  return core_->sync_manager()->GetUserShare();
}

SyncBackendHostImpl::Status SyncBackendHostImpl::GetDetailedStatus() {
  DCHECK(initialized());
  return core_->sync_manager()->GetDetailedStatus();
}

bool SyncBackendHostImpl::HasUnsyncedItems() const {
  DCHECK(initialized());
  return core_->sync_manager()->HasUnsyncedItems();
}

bool SyncBackendHostImpl::IsNigoriEnabled() const {
  return registrar_ && registrar_->IsNigoriEnabled();
}

PassphraseType SyncBackendHostImpl::GetPassphraseType() const {
  return cached_passphrase_type_;
}

base::Time SyncBackendHostImpl::GetExplicitPassphraseTime() const {
  return cached_explicit_passphrase_time_;
}

bool SyncBackendHostImpl::IsCryptographerReady(
    const BaseTransaction* trans) const {
  return initialized() && trans->GetCryptographer() &&
         trans->GetCryptographer()->is_ready();
}

void SyncBackendHostImpl::GetModelSafeRoutingInfo(
    ModelSafeRoutingInfo* out) const {
  if (initialized()) {
    CHECK(registrar_);
    registrar_->GetModelSafeRoutingInfo(out);
  } else {
    NOTREACHED();
  }
}

void SyncBackendHostImpl::FlushDirectory() const {
  DCHECK(initialized());
  sync_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::SaveChanges, core_));
}

void SyncBackendHostImpl::RequestBufferedProtocolEventsAndEnableForwarding() {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &SyncBackendHostCore::SendBufferedProtocolEventsAndEnableForwarding,
          core_));
}

void SyncBackendHostImpl::DisableProtocolEventForwarding() {
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DisableProtocolEventForwarding, core_));
}

void SyncBackendHostImpl::EnableDirectoryTypeDebugInfoForwarding() {
  DCHECK(initialized());
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::EnableDirectoryTypeDebugInfoForwarding,
                 core_));
}

void SyncBackendHostImpl::DisableDirectoryTypeDebugInfoForwarding() {
  DCHECK(initialized());
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DisableDirectoryTypeDebugInfoForwarding,
                 core_));
}

void SyncBackendHostImpl::FinishConfigureDataTypesOnFrontendLoop(
    const ModelTypeSet enabled_types,
    const ModelTypeSet succeeded_configuration_types,
    const ModelTypeSet failed_configuration_types,
    const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task) {
  if (invalidator_) {
    CHECK(invalidator_->UpdateRegisteredInvalidationIds(
        this, ModelTypeSetToObjectIdSet(enabled_types)));
  }

  if (!ready_task.is_null())
    ready_task.Run(succeeded_configuration_types, failed_configuration_types);
}

void SyncBackendHostImpl::AddExperimentalTypes() {
  CHECK(initialized());
  Experiments experiments;
  if (core_->sync_manager()->ReceivedExperiment(&experiments))
    host_->OnExperimentsChanged(experiments);
}

void SyncBackendHostImpl::HandleInitializationSuccessOnFrontendLoop(
    ModelTypeSet initial_types,
    const WeakHandle<JsBackend> js_backend,
    const WeakHandle<DataTypeDebugInfoListener> debug_info_listener,
    std::unique_ptr<ModelTypeConnector> model_type_connector,
    const std::string& cache_guid) {
  DCHECK(thread_checker_.CalledOnValidThread());

  model_type_connector_ = std::move(model_type_connector);

  initialized_ = true;

  if (invalidator_) {
    invalidator_->RegisterInvalidationHandler(this);
    invalidation_handler_registered_ = true;

    // Fake a state change to initialize the SyncManager's cached invalidator
    // state.
    OnInvalidatorStateChange(invalidator_->GetInvalidatorState());
  }

  // Now that we've downloaded the control types, we can see if there are any
  // experimental types to enable. This should be done before we inform
  // the host to ensure they're visible in the customize screen.
  AddExperimentalTypes();
  host_->OnEngineInitialized(initial_types, js_backend, debug_info_listener,
                             cache_guid, true);
}

void SyncBackendHostImpl::HandleInitializationFailureOnFrontendLoop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_->OnEngineInitialized(ModelTypeSet(), WeakHandle<JsBackend>(),
                             WeakHandle<DataTypeDebugInfoListener>(), "",
                             false);
}

void SyncBackendHostImpl::HandleSyncCycleCompletedOnFrontendLoop(
    const SyncCycleSnapshot& snapshot) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Process any changes to the datatypes we're syncing.
  // TODO(sync): add support for removing types.
  if (initialized()) {
    AddExperimentalTypes();
    host_->OnSyncCycleCompleted(snapshot);
  }
}

void SyncBackendHostImpl::RetryConfigurationOnFrontendLoop(
    const base::Closure& retry_callback) {
  SDVLOG(1) << "Failed to complete configuration, informing of retry.";
  retry_callback.Run();
}

void SyncBackendHostImpl::PersistEncryptionBootstrapToken(
    const std::string& token,
    BootstrapTokenType token_type) {
  CHECK(sync_prefs_.get());
  if (token_type == PASSPHRASE_BOOTSTRAP_TOKEN)
    sync_prefs_->SetEncryptionBootstrapToken(token);
  else
    sync_prefs_->SetKeystoreEncryptionBootstrapToken(token);
}

void SyncBackendHostImpl::HandleActionableErrorEventOnFrontendLoop(
    const SyncProtocolError& sync_error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_->OnActionableError(sync_error);
}

void SyncBackendHostImpl::HandleMigrationRequestedOnFrontendLoop(
    ModelTypeSet types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_->OnMigrationNeededForTypes(types);
}

void SyncBackendHostImpl::OnInvalidatorStateChange(InvalidatorState state) {
  sync_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoOnInvalidatorStateChange,
                            core_, state));
}

void SyncBackendHostImpl::OnIncomingInvalidation(
    const ObjectIdInvalidationMap& invalidation_map) {
  sync_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoOnIncomingInvalidation,
                            core_, invalidation_map));
}

std::string SyncBackendHostImpl::GetOwnerName() const {
  return "SyncBackendHostImpl";
}

bool SyncBackendHostImpl::CheckPassphraseAgainstCachedPendingKeys(
    const std::string& passphrase) const {
  DCHECK(cached_pending_keys_.has_blob());
  DCHECK(!passphrase.empty());
  Nigori nigori;
  nigori.InitByDerivation("localhost", "dummy", passphrase);
  std::string plaintext;
  bool result = nigori.Decrypt(cached_pending_keys_.blob(), &plaintext);
  DVLOG_IF(1, result) << "Passphrase failed to decrypt pending keys.";
  return result;
}

void SyncBackendHostImpl::NotifyPassphraseRequired(
    PassphraseRequiredReason reason,
    sync_pb::EncryptedData pending_keys) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Update our cache of the cryptographer's pending keys.
  cached_pending_keys_ = pending_keys;

  host_->OnPassphraseRequired(reason, pending_keys);
}

void SyncBackendHostImpl::NotifyPassphraseAccepted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Clear our cache of the cryptographer's pending keys.
  cached_pending_keys_.clear_blob();
  host_->OnPassphraseAccepted();
}

void SyncBackendHostImpl::NotifyEncryptedTypesChanged(
    ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_->OnEncryptedTypesChanged(encrypted_types, encrypt_everything);
}

void SyncBackendHostImpl::NotifyEncryptionComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_->OnEncryptionComplete();
}

void SyncBackendHostImpl::HandlePassphraseTypeChangedOnFrontendLoop(
    PassphraseType type,
    base::Time explicit_passphrase_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Passphrase type changed to " << PassphraseTypeToString(type);
  cached_passphrase_type_ = type;
  cached_explicit_passphrase_time_ = explicit_passphrase_time;
}

void SyncBackendHostImpl::HandleLocalSetPassphraseEncryptionOnFrontendLoop(
    const SyncEncryptionHandler::NigoriState& nigori_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_->OnLocalSetPassphraseEncryption(nigori_state);
}

void SyncBackendHostImpl::HandleConnectionStatusChangeOnFrontendLoop(
    ConnectionStatus status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Connection status changed: " << ConnectionStatusToString(status);
  host_->OnConnectionStatusChange(status);
}

void SyncBackendHostImpl::HandleProtocolEventOnFrontendLoop(
    std::unique_ptr<ProtocolEvent> event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_->OnProtocolEvent(*event);
}

void SyncBackendHostImpl::HandleDirectoryCommitCountersUpdatedOnFrontendLoop(
    ModelType type,
    const CommitCounters& counters) {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_->OnDirectoryTypeCommitCounterUpdated(type, counters);
}

void SyncBackendHostImpl::HandleDirectoryUpdateCountersUpdatedOnFrontendLoop(
    ModelType type,
    const UpdateCounters& counters) {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_->OnDirectoryTypeUpdateCounterUpdated(type, counters);
}

void SyncBackendHostImpl::HandleDirectoryStatusCountersUpdatedOnFrontendLoop(
    ModelType type,
    const StatusCounters& counters) {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_->OnDatatypeStatusCounterUpdated(type, counters);
}

void SyncBackendHostImpl::UpdateInvalidationVersions(
    const std::map<ModelType, int64_t>& invalidation_versions) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_prefs_->UpdateInvalidationVersions(invalidation_versions);
}

void SyncBackendHostImpl::RefreshTypesForTest(ModelTypeSet types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoRefreshTypes, core_, types));
}

void SyncBackendHostImpl::ClearServerData(
    const SyncManager::ClearServerDataCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoClearServerData, core_, callback));
}

void SyncBackendHostImpl::OnCookieJarChanged(bool account_mismatch,
                                             bool empty_jar) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoOnCookieJarChanged, core_,
                            account_mismatch, empty_jar));
}

void SyncBackendHostImpl::ClearServerDataDoneOnFrontendLoop(
    const SyncManager::ClearServerDataCallback& frontend_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  frontend_callback.Run();
}

}  // namespace syncer

#undef SDVLOG

#undef SLOG
