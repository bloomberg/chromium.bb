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
#include "components/sync/driver/glue/sync_backend_registrar.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_frontend.h"
#include "components/sync/engine/activation_context.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/engine_components_factory_impl.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/net/http_bridge.h"
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
    const base::FilePath& sync_folder)
    : sync_client_(sync_client),
      name_(name),
      initialized_(false),
      sync_prefs_(sync_prefs),
      frontend_(nullptr),
      cached_passphrase_type_(PassphraseType::IMPLICIT_PASSPHRASE),
      invalidator_(invalidator),
      invalidation_handler_registered_(false),
      weak_ptr_factory_(this) {
  core_ = new SyncBackendHostCore(name_, sync_folder,
                                  weak_ptr_factory_.GetWeakPtr());
}

SyncBackendHostImpl::~SyncBackendHostImpl() {
  DCHECK(!core_.get() && !frontend_) << "Must call Shutdown before destructor.";
  DCHECK(!registrar_.get());
}

void SyncBackendHostImpl::Initialize(
    SyncFrontend* frontend,
    scoped_refptr<base::SingleThreadTaskRunner> sync_task_runner,
    const WeakHandle<JsEventHandler>& event_handler,
    const GURL& sync_service_url,
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
  CHECK(sync_task_runner);
  sync_task_runner_ = sync_task_runner;

  registrar_ = base::MakeUnique<SyncBackendRegistrar>(
      name_, base::Bind(&SyncClient::CreateModelWorkerForGroup,
                        base::Unretained(sync_client_)));

  DCHECK(frontend);
  frontend_ = frontend;

  std::vector<scoped_refptr<ModelSafeWorker>> workers;
  registrar_->GetWorkers(&workers);

  EngineComponentsFactory::Switches factory_switches = {
      EngineComponentsFactory::ENCRYPTION_KEYSTORE,
      EngineComponentsFactory::BACKOFF_NORMAL};

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kSyncShortInitialRetryOverride)) {
    factory_switches.backoff_override =
        EngineComponentsFactory::BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE;
  }
  if (cl->HasSwitch(switches::kSyncEnableGetUpdateAvoidance)) {
    factory_switches.pre_commit_updates_policy =
        EngineComponentsFactory::FORCE_ENABLE_PRE_COMMIT_UPDATE_AVOIDANCE;
  }
  if (cl->HasSwitch(switches::kSyncShortNudgeDelayForTest)) {
    factory_switches.nudge_delay =
        EngineComponentsFactory::NudgeDelay::SHORT_NUDGE_DELAY;
  }

  std::map<ModelType, int64_t> invalidation_versions;
  sync_prefs_->GetInvalidationVersions(&invalidation_versions);

  std::unique_ptr<DoInitializeOptions> init_opts(new DoInitializeOptions(
      sync_task_runner_, registrar_.get(), workers,
      sync_client_->GetExtensionsActivity(), event_handler, sync_service_url,
      sync_user_agent, http_post_provider_factory_getter.Run(
                           core_->GetRequestContextCancelationSignal()),
      credentials, invalidator_ ? invalidator_->GetInvalidatorClientId() : "",
      std::move(sync_manager_factory), delete_sync_data_folder,
      enable_local_sync_backend, local_sync_backend_folder,
      sync_prefs_->GetEncryptionBootstrapToken(),
      sync_prefs_->GetKeystoreEncryptionBootstrapToken(),
      std::unique_ptr<EngineComponentsFactory>(
          new EngineComponentsFactoryImpl(factory_switches)),
      unrecoverable_error_handler, report_unrecoverable_error_function,
      std::move(saved_nigori_state), invalidation_versions));
  InitCore(std::move(init_opts));
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
  if (!IsNigoriEnabled()) {
    NOTREACHED() << "SetEncryptionPassphrase must never be called when nigori"
                    " is disabled.";
    return;
  }

  // We should never be called with an empty passphrase.
  DCHECK(!passphrase.empty());

  // This should only be called by the frontend.
  DCHECK(thread_checker_.CalledOnValidThread());

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
  if (!IsNigoriEnabled()) {
    NOTREACHED() << "SetDecryptionPassphrase must never be called when nigori"
                    " is disabled.";
    return false;
  }

  // We should never be called with an empty passphrase.
  DCHECK(!passphrase.empty());

  // This should only be called by the frontend.
  DCHECK(thread_checker_.CalledOnValidThread());

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
  // Immediately stop sending messages to the frontend.
  frontend_ = nullptr;

  registrar_->RequestWorkerStopOnUIThread();

  core_->ShutdownOnUIThread();
}

void SyncBackendHostImpl::Shutdown(ShutdownReason reason) {
  // StopSyncingForShutdown() (which nulls out |frontend_|) should be
  // called first.
  DCHECK(!frontend_);

  if (invalidation_handler_registered_) {
    if (reason == DISABLE_SYNC) {
      UnregisterInvalidationIds();
    }
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

  // Destroy |registrar_|.
  sync_task_runner_->DeleteSoon(FROM_HERE, registrar_.release());
}

void SyncBackendHostImpl::UnregisterInvalidationIds() {
  if (invalidation_handler_registered_) {
    CHECK(invalidator_->UpdateRegisteredInvalidationIds(this, ObjectIdSet()));
  }
}

ModelTypeSet SyncBackendHostImpl::ConfigureDataTypes(
    ConfigureReason reason,
    const DataTypeConfigStateMap& config_state_map,
    const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task,
    const base::Callback<void()>& retry_callback) {
  // Only one configure is allowed at a time.  This is guaranteed by our
  // callers.  The SyncBackendHostImpl requests one configure as the backend is
  // initializing and waits for it to complete.  After initialization, all
  // configurations will pass through the DataTypeManager, which is careful to
  // never send a new configure request until the current request succeeds.

  // The SyncBackendRegistrar's routing info will be updated by adding the
  // types_to_add to the list then removing types_to_remove.  Any types which
  // are not in either of those sets will remain untouched.
  //
  // Types which were not in the list previously are not fully downloaded, so we
  // must ask the syncer to download them.  Any newly supported datatypes will
  // not have been in that routing info list, so they will be among the types
  // downloaded if they are enabled.
  //
  // The SyncBackendRegistrar's state was initially derived from the types
  // detected to have been downloaded in the database.  Afterwards it is
  // modified only by this function.  We expect it to remain in sync with the
  // backend because configuration requests are never aborted; they are retried
  // until they succeed or the backend is shut down.

  ModelTypeSet disabled_types = GetDataTypesInState(DISABLED, config_state_map);
  ModelTypeSet fatal_types = GetDataTypesInState(FATAL, config_state_map);
  ModelTypeSet crypto_types = GetDataTypesInState(CRYPTO, config_state_map);
  ModelTypeSet unready_types = GetDataTypesInState(UNREADY, config_state_map);

  disabled_types.PutAll(fatal_types);
  disabled_types.PutAll(crypto_types);
  disabled_types.PutAll(unready_types);

  ModelTypeSet active_types =
      GetDataTypesInState(CONFIGURE_ACTIVE, config_state_map);
  ModelTypeSet clean_first_types =
      GetDataTypesInState(CONFIGURE_CLEAN, config_state_map);
  ModelTypeSet types_to_download = registrar_->ConfigureDataTypes(
      Union(active_types, clean_first_types), disabled_types);
  types_to_download.PutAll(clean_first_types);
  types_to_download.RemoveAll(ProxyTypes());
  if (!types_to_download.Empty())
    types_to_download.Put(NIGORI);

  // TODO(sync): crbug.com/137550.
  // It's dangerous to configure types that have progress markers.  Types with
  // progress markers can trigger a MIGRATION_DONE response.  We are not
  // prepared to handle a migration during a configure, so we must ensure that
  // all our types_to_download actually contain no data before we sync them.
  //
  // One common way to end up in this situation used to be types which
  // downloaded some or all of their data but have not applied it yet.  We avoid
  // problems with those types by purging the data of any such partially synced
  // types soon after we load the directory.
  //
  // Another possible scenario is that we have newly supported or newly enabled
  // data types being downloaded here but the nigori type, which is always
  // included in any GetUpdates request, requires migration.  The server has
  // code to detect this scenario based on the configure reason, the fact that
  // the nigori type is the only requested type which requires migration, and
  // that the requested types list includes at least one non-nigori type.  It
  // will not send a MIGRATION_DONE response in that case.  We still need to be
  // careful to not send progress markers for non-nigori types, though.  If a
  // non-nigori type in the request requires migration, a MIGRATION_DONE
  // response will be sent.

  ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);

  ModelTypeSet current_types = registrar_->GetLastConfiguredTypes();
  ModelTypeSet types_to_purge = Difference(ModelTypeSet::All(), current_types);
  ModelTypeSet inactive_types =
      GetDataTypesInState(CONFIGURE_INACTIVE, config_state_map);
  types_to_purge.RemoveAll(inactive_types);
  types_to_purge.RemoveAll(unready_types);

  // If a type has already been disabled and unapplied or journaled, it will
  // not be part of the |types_to_purge| set, and therefore does not need
  // to be acted on again.
  fatal_types.RetainAll(types_to_purge);
  ModelTypeSet unapply_types = Union(crypto_types, clean_first_types);
  unapply_types.RetainAll(types_to_purge);

  DCHECK(Intersection(current_types, fatal_types).Empty());
  DCHECK(Intersection(current_types, crypto_types).Empty());
  DCHECK(current_types.HasAll(types_to_download));

  SDVLOG(1) << "Types " << ModelTypeSetToString(types_to_download)
            << " added; calling DoConfigureSyncer";
  // Divide up the types into their corresponding actions (each is mutually
  // exclusive):
  // - Types which have just been added to the routing info (types_to_download):
  //   are downloaded.
  // - Types which have encountered a fatal error (fatal_types) are deleted
  //   from the directory and journaled in the delete journal.
  // - Types which have encountered a cryptographer error (crypto_types) are
  //   unapplied (local state is purged but sync state is not).
  // - All other types not in the routing info (types just disabled) are deleted
  //   from the directory.
  // - Everything else (enabled types and already disabled types) is not
  //   touched.
  RequestConfigureSyncer(reason, types_to_download, types_to_purge, fatal_types,
                         unapply_types, inactive_types, routing_info,
                         ready_task, retry_callback);

  DCHECK(Intersection(active_types, types_to_purge).Empty());
  DCHECK(Intersection(active_types, fatal_types).Empty());
  DCHECK(Intersection(active_types, unapply_types).Empty());
  DCHECK(Intersection(active_types, inactive_types).Empty());
  return Difference(active_types, types_to_download);
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

SyncCycleSnapshot SyncBackendHostImpl::GetLastCycleSnapshot() const {
  return last_snapshot_;
}

bool SyncBackendHostImpl::HasUnsyncedItems() const {
  DCHECK(initialized());
  return core_->sync_manager()->HasUnsyncedItems();
}

bool SyncBackendHostImpl::IsNigoriEnabled() const {
  return registrar_.get() && registrar_->IsNigoriEnabled();
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
    CHECK(registrar_.get());
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

void SyncBackendHostImpl::InitCore(
    std::unique_ptr<DoInitializeOptions> options) {
  sync_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoInitialize, core_,
                            base::Passed(&options)));
}

void SyncBackendHostImpl::RequestConfigureSyncer(
    ConfigureReason reason,
    ModelTypeSet to_download,
    ModelTypeSet to_purge,
    ModelTypeSet to_journal,
    ModelTypeSet to_unapply,
    ModelTypeSet to_ignore,
    const ModelSafeRoutingInfo& routing_info,
    const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task,
    const base::Closure& retry_callback) {
  DoConfigureSyncerTypes config_types;
  config_types.to_download = to_download;
  config_types.to_purge = to_purge;
  config_types.to_journal = to_journal;
  config_types.to_unapply = to_unapply;
  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoConfigureSyncer, core_, reason,
                 config_types, routing_info, ready_task, retry_callback));
}

void SyncBackendHostImpl::FinishConfigureDataTypesOnFrontendLoop(
    const ModelTypeSet enabled_types,
    const ModelTypeSet succeeded_configuration_types,
    const ModelTypeSet failed_configuration_types,
    const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task) {
  if (!frontend_)
    return;

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
    frontend_->OnExperimentsChanged(experiments);
}

void SyncBackendHostImpl::HandleInitializationSuccessOnFrontendLoop(
    const WeakHandle<JsBackend> js_backend,
    const WeakHandle<DataTypeDebugInfoListener> debug_info_listener,
    std::unique_ptr<ModelTypeConnector> model_type_connector,
    const std::string& cache_guid) {
  DCHECK(thread_checker_.CalledOnValidThread());

  model_type_connector_ = std::move(model_type_connector);

  if (!frontend_)
    return;

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
  // the frontend to ensure they're visible in the customize screen.
  AddExperimentalTypes();
  frontend_->OnBackendInitialized(js_backend, debug_info_listener, cache_guid,
                                  true);
}

void SyncBackendHostImpl::HandleInitializationFailureOnFrontendLoop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!frontend_)
    return;

  frontend_->OnBackendInitialized(WeakHandle<JsBackend>(),
                                  WeakHandle<DataTypeDebugInfoListener>(), "",
                                  false);
}

void SyncBackendHostImpl::HandleSyncCycleCompletedOnFrontendLoop(
    const SyncCycleSnapshot& snapshot) {
  if (!frontend_)
    return;
  DCHECK(thread_checker_.CalledOnValidThread());

  last_snapshot_ = snapshot;

  SDVLOG(1) << "Got snapshot " << snapshot.ToString();

  if (!snapshot.poll_finish_time().is_null())
    sync_prefs_->SetLastPollTime(snapshot.poll_finish_time());

  // Process any changes to the datatypes we're syncing.
  // TODO(sync): add support for removing types.
  if (initialized())
    AddExperimentalTypes();

  if (initialized())
    frontend_->OnSyncCycleCompleted();
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
  if (!frontend_)
    return;
  DCHECK(thread_checker_.CalledOnValidThread());
  frontend_->OnActionableError(sync_error);
}

void SyncBackendHostImpl::HandleMigrationRequestedOnFrontendLoop(
    ModelTypeSet types) {
  if (!frontend_)
    return;
  DCHECK(thread_checker_.CalledOnValidThread());
  frontend_->OnMigrationNeededForTypes(types);
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
  if (!frontend_)
    return;

  DCHECK(thread_checker_.CalledOnValidThread());

  // Update our cache of the cryptographer's pending keys.
  cached_pending_keys_ = pending_keys;

  frontend_->OnPassphraseRequired(reason, pending_keys);
}

void SyncBackendHostImpl::NotifyPassphraseAccepted() {
  if (!frontend_)
    return;

  DCHECK(thread_checker_.CalledOnValidThread());

  // Clear our cache of the cryptographer's pending keys.
  cached_pending_keys_.clear_blob();
  frontend_->OnPassphraseAccepted();
}

void SyncBackendHostImpl::NotifyEncryptedTypesChanged(
    ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  if (!frontend_)
    return;

  DCHECK(thread_checker_.CalledOnValidThread());
  frontend_->OnEncryptedTypesChanged(encrypted_types, encrypt_everything);
}

void SyncBackendHostImpl::NotifyEncryptionComplete() {
  if (!frontend_)
    return;

  DCHECK(thread_checker_.CalledOnValidThread());
  frontend_->OnEncryptionComplete();
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
  frontend_->OnLocalSetPassphraseEncryption(nigori_state);
}

void SyncBackendHostImpl::HandleConnectionStatusChangeOnFrontendLoop(
    ConnectionStatus status) {
  if (!frontend_)
    return;

  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG(1) << "Connection status changed: " << ConnectionStatusToString(status);
  frontend_->OnConnectionStatusChange(status);
}

void SyncBackendHostImpl::HandleProtocolEventOnFrontendLoop(
    std::unique_ptr<ProtocolEvent> event) {
  if (!frontend_)
    return;
  frontend_->OnProtocolEvent(*event);
}

void SyncBackendHostImpl::HandleDirectoryCommitCountersUpdatedOnFrontendLoop(
    ModelType type,
    const CommitCounters& counters) {
  if (!frontend_)
    return;
  frontend_->OnDirectoryTypeCommitCounterUpdated(type, counters);
}

void SyncBackendHostImpl::HandleDirectoryUpdateCountersUpdatedOnFrontendLoop(
    ModelType type,
    const UpdateCounters& counters) {
  if (!frontend_)
    return;
  frontend_->OnDirectoryTypeUpdateCounterUpdated(type, counters);
}

void SyncBackendHostImpl::HandleDirectoryStatusCountersUpdatedOnFrontendLoop(
    ModelType type,
    const StatusCounters& counters) {
  if (!frontend_)
    return;
  frontend_->OnDatatypeStatusCounterUpdated(type, counters);
}

void SyncBackendHostImpl::UpdateInvalidationVersions(
    const std::map<ModelType, int64_t>& invalidation_versions) {
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
