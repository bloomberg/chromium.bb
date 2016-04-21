// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/glue/sync_backend_host_impl.h"

#include <map>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/sync_driver/glue/sync_backend_host_core.h"
#include "components/sync_driver/glue/sync_backend_registrar.h"
#include "components/sync_driver/invalidation_helper.h"
#include "components/sync_driver/sync_client.h"
#include "components/sync_driver/sync_driver_switches.h"
#include "components/sync_driver/sync_frontend.h"
#include "components/sync_driver/sync_prefs.h"
#include "sync/internal_api/public/activation_context.h"
#include "sync/internal_api/public/base_transaction.h"
#include "sync/internal_api/public/events/protocol_event.h"
#include "sync/internal_api/public/http_bridge.h"
#include "sync/internal_api/public/internal_components_factory.h"
#include "sync/internal_api/public/internal_components_factory_impl.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/sync_manager_factory.h"
#include "sync/internal_api/public/util/experiments.h"
#include "sync/internal_api/public/util/sync_string_conversions.h"

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncers involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

using syncer::InternalComponentsFactory;

namespace browser_sync {

SyncBackendHostImpl::SyncBackendHostImpl(
    const std::string& name,
    sync_driver::SyncClient* sync_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    invalidation::InvalidationService* invalidator,
    const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
    const base::FilePath& sync_folder)
    : frontend_loop_(base::MessageLoop::current()),
      sync_client_(sync_client),
      ui_thread_(ui_thread),
      name_(name),
      initialized_(false),
      sync_prefs_(sync_prefs),
      frontend_(NULL),
      cached_passphrase_type_(syncer::IMPLICIT_PASSPHRASE),
      invalidator_(invalidator),
      invalidation_handler_registered_(false),
      weak_ptr_factory_(this) {
  core_ = new SyncBackendHostCore(name_, sync_folder,
                                  sync_prefs_->IsFirstSetupComplete(),
                                  weak_ptr_factory_.GetWeakPtr());
}

SyncBackendHostImpl::~SyncBackendHostImpl() {
  DCHECK(!core_.get() && !frontend_) << "Must call Shutdown before destructor.";
  DCHECK(!registrar_.get());
}

void SyncBackendHostImpl::Initialize(
    sync_driver::SyncFrontend* frontend,
    std::unique_ptr<base::Thread> sync_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& file_thread,
    const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
    const GURL& sync_service_url,
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
  registrar_.reset(new browser_sync::SyncBackendRegistrar(
      name_, sync_client_, std::move(sync_thread), ui_thread_, db_thread,
      file_thread));
  CHECK(registrar_->sync_thread());

  frontend_ = frontend;
  DCHECK(frontend);

  syncer::ModelSafeRoutingInfo routing_info;
  std::vector<scoped_refptr<syncer::ModelSafeWorker> > workers;
  registrar_->GetModelSafeRoutingInfo(&routing_info);
  registrar_->GetWorkers(&workers);

  InternalComponentsFactory::Switches factory_switches = {
    InternalComponentsFactory::ENCRYPTION_KEYSTORE,
    InternalComponentsFactory::BACKOFF_NORMAL
  };

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kSyncShortInitialRetryOverride)) {
    factory_switches.backoff_override =
        InternalComponentsFactory::BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE;
  }
  if (cl->HasSwitch(switches::kSyncEnableGetUpdateAvoidance)) {
    factory_switches.pre_commit_updates_policy =
        InternalComponentsFactory::FORCE_ENABLE_PRE_COMMIT_UPDATE_AVOIDANCE;
  }
  syncer::PassphraseTransitionClearDataOption clear_data_option =
      syncer::PASSPHRASE_TRANSITION_DO_NOT_CLEAR_DATA;
  if (cl->HasSwitch(switches::kSyncEnableClearDataOnPassphraseEncryption))
    clear_data_option = syncer::PASSPHRASE_TRANSITION_CLEAR_DATA;

  std::map<syncer::ModelType, int64_t> invalidation_versions;
  sync_prefs_->GetInvalidationVersions(&invalidation_versions);

  std::unique_ptr<DoInitializeOptions> init_opts(new DoInitializeOptions(
      registrar_->sync_thread()->message_loop(), registrar_.get(), routing_info,
      workers, sync_client_->GetExtensionsActivity(), event_handler,
      sync_service_url, sync_user_agent,
      http_post_provider_factory_getter.Run(
          core_->GetRequestContextCancelationSignal()),
      credentials, invalidator_ ? invalidator_->GetInvalidatorClientId() : "",
      std::move(sync_manager_factory), delete_sync_data_folder,
      sync_prefs_->GetEncryptionBootstrapToken(),
      sync_prefs_->GetKeystoreEncryptionBootstrapToken(),
      std::unique_ptr<InternalComponentsFactory>(
          new syncer::InternalComponentsFactoryImpl(factory_switches)),
      unrecoverable_error_handler, report_unrecoverable_error_function,
      std::move(saved_nigori_state), clear_data_option, invalidation_versions));
  InitCore(std::move(init_opts));
}

void SyncBackendHostImpl::TriggerRefresh(const syncer::ModelTypeSet& types) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoRefreshTypes, core_.get(), types));
}

void SyncBackendHostImpl::UpdateCredentials(
    const syncer::SyncCredentials& credentials) {
  DCHECK(registrar_->sync_thread()->IsRunning());
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoUpdateCredentials,
                            core_.get(), credentials));
}

void SyncBackendHostImpl::StartSyncingWithServer() {
  SDVLOG(1) << "SyncBackendHostImpl::StartSyncingWithServer called.";

  syncer::ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);

  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoStartSyncing, core_.get(),
                            routing_info, sync_prefs_->GetLastPollTime()));
}

void SyncBackendHostImpl::SetEncryptionPassphrase(const std::string& passphrase,
                                                  bool is_explicit) {
  DCHECK(registrar_->sync_thread()->IsRunning());
  if (!IsNigoriEnabled()) {
    NOTREACHED() << "SetEncryptionPassphrase must never be called when nigori"
                    " is disabled.";
    return;
  }

  // We should never be called with an empty passphrase.
  DCHECK(!passphrase.empty());

  // This should only be called by the frontend.
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

  // SetEncryptionPassphrase should never be called if we are currently
  // encrypted with an explicit passphrase.
  DCHECK(cached_passphrase_type_ == syncer::KEYSTORE_PASSPHRASE ||
         cached_passphrase_type_ == syncer::IMPLICIT_PASSPHRASE);

  // Post an encryption task on the syncer thread.
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoSetEncryptionPassphrase,
                            core_.get(), passphrase, is_explicit));
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
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

  // This should only be called when we have cached pending keys.
  DCHECK(cached_pending_keys_.has_blob());

  // Check the passphrase that was provided against our local cache of the
  // cryptographer's pending keys. If this was unsuccessful, the UI layer can
  // immediately call OnPassphraseRequired without showing the user a spinner.
  if (!CheckPassphraseAgainstCachedPendingKeys(passphrase))
    return false;

  // Post a decryption task on the syncer thread.
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoSetDecryptionPassphrase,
                            core_.get(), passphrase));

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
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

  // Immediately stop sending messages to the frontend.
  frontend_ = NULL;

  // Stop non-blocking sync types from sending any more requests to the syncer.
  model_type_connector_.reset();

  DCHECK(registrar_->sync_thread()->IsRunning());

  registrar_->RequestWorkerStopOnUIThread();

  core_->ShutdownOnUIThread();
}

std::unique_ptr<base::Thread> SyncBackendHostImpl::Shutdown(
    syncer::ShutdownReason reason) {
  // StopSyncingForShutdown() (which nulls out |frontend_|) should be
  // called first.
  DCHECK(!frontend_);
  DCHECK(registrar_->sync_thread()->IsRunning());

  bool sync_thread_claimed = (reason != syncer::BROWSER_SHUTDOWN);

  if (invalidation_handler_registered_) {
    if (reason == syncer::DISABLE_SYNC) {
      UnregisterInvalidationIds();
    }
    invalidator_->UnregisterInvalidationHandler(this);
    invalidator_ = NULL;
  }
  invalidation_handler_registered_ = false;

  // Shut down and destroy sync manager.
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoShutdown, core_.get(), reason));
  core_ = NULL;

  // Worker cleanup.
  SyncBackendRegistrar* detached_registrar = registrar_.release();
  detached_registrar->sync_thread()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendRegistrar::Shutdown,
                            base::Unretained(detached_registrar)));

  if (sync_thread_claimed)
    return detached_registrar->ReleaseSyncThread();
  else
    return std::unique_ptr<base::Thread>();
}

void SyncBackendHostImpl::UnregisterInvalidationIds() {
  if (invalidation_handler_registered_) {
    CHECK(invalidator_->UpdateRegisteredInvalidationIds(this,
                                                        syncer::ObjectIdSet()));
  }
}

syncer::ModelTypeSet SyncBackendHostImpl::ConfigureDataTypes(
    syncer::ConfigureReason reason,
    const DataTypeConfigStateMap& config_state_map,
    const base::Callback<void(syncer::ModelTypeSet, syncer::ModelTypeSet)>&
        ready_task,
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

  syncer::ModelTypeSet disabled_types =
      GetDataTypesInState(DISABLED, config_state_map);
  syncer::ModelTypeSet fatal_types =
      GetDataTypesInState(FATAL, config_state_map);
  syncer::ModelTypeSet crypto_types =
      GetDataTypesInState(CRYPTO, config_state_map);
  syncer::ModelTypeSet unready_types =
      GetDataTypesInState(UNREADY, config_state_map);

  disabled_types.PutAll(fatal_types);
  disabled_types.PutAll(crypto_types);
  disabled_types.PutAll(unready_types);

  syncer::ModelTypeSet active_types =
      GetDataTypesInState(CONFIGURE_ACTIVE, config_state_map);
  syncer::ModelTypeSet clean_first_types =
      GetDataTypesInState(CONFIGURE_CLEAN, config_state_map);
  syncer::ModelTypeSet types_to_download = registrar_->ConfigureDataTypes(
      syncer::Union(active_types, clean_first_types),
      disabled_types);
  types_to_download.PutAll(clean_first_types);
  types_to_download.RemoveAll(syncer::ProxyTypes());
  if (!types_to_download.Empty())
    types_to_download.Put(syncer::NIGORI);

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

  syncer::ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);

  syncer::ModelTypeSet current_types = registrar_->GetLastConfiguredTypes();
  syncer::ModelTypeSet types_to_purge =
      syncer::Difference(syncer::ModelTypeSet::All(), current_types);
  syncer::ModelTypeSet inactive_types =
      GetDataTypesInState(CONFIGURE_INACTIVE, config_state_map);
  types_to_purge.RemoveAll(inactive_types);
  types_to_purge.RemoveAll(unready_types);

  // If a type has already been disabled and unapplied or journaled, it will
  // not be part of the |types_to_purge| set, and therefore does not need
  // to be acted on again.
  fatal_types.RetainAll(types_to_purge);
  syncer::ModelTypeSet unapply_types =
      syncer::Union(crypto_types, clean_first_types);
  unapply_types.RetainAll(types_to_purge);

  DCHECK(syncer::Intersection(current_types, fatal_types).Empty());
  DCHECK(syncer::Intersection(current_types, crypto_types).Empty());
  DCHECK(current_types.HasAll(types_to_download));

  SDVLOG(1) << "Types "
            << syncer::ModelTypeSetToString(types_to_download)
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
  RequestConfigureSyncer(reason,
                         types_to_download,
                         types_to_purge,
                         fatal_types,
                         unapply_types,
                         inactive_types,
                         routing_info,
                         ready_task,
                         retry_callback);

  DCHECK(syncer::Intersection(active_types, types_to_purge).Empty());
  DCHECK(syncer::Intersection(active_types, fatal_types).Empty());
  DCHECK(syncer::Intersection(active_types, unapply_types).Empty());
  DCHECK(syncer::Intersection(active_types, inactive_types).Empty());
  return syncer::Difference(active_types, types_to_download);
}

void SyncBackendHostImpl::EnableEncryptEverything() {
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoEnableEncryptEverything, core_.get()));
}

void SyncBackendHostImpl::ActivateDirectoryDataType(
    syncer::ModelType type,
    syncer::ModelSafeGroup group,
    sync_driver::ChangeProcessor* change_processor) {
  registrar_->ActivateDataType(type, group, change_processor, GetUserShare());
}

void SyncBackendHostImpl::DeactivateDirectoryDataType(syncer::ModelType type) {
  registrar_->DeactivateDataType(type);
}

void SyncBackendHostImpl::ActivateNonBlockingDataType(
    syncer::ModelType type,
    std::unique_ptr<syncer_v2::ActivationContext> activation_context) {
  registrar_->RegisterNonBlockingType(type);
  model_type_connector_->ConnectType(type, std::move(activation_context));
}

void SyncBackendHostImpl::DeactivateNonBlockingDataType(
    syncer::ModelType type) {
  model_type_connector_->DisconnectType(type);
}

syncer::UserShare* SyncBackendHostImpl::GetUserShare() const {
  return core_->sync_manager()->GetUserShare();
}

SyncBackendHostImpl::Status SyncBackendHostImpl::GetDetailedStatus() {
  DCHECK(initialized());
  return core_->sync_manager()->GetDetailedStatus();
}

syncer::sessions::SyncSessionSnapshot
SyncBackendHostImpl::GetLastSessionSnapshot() const {
  return last_snapshot_;
}

bool SyncBackendHostImpl::HasUnsyncedItems() const {
  DCHECK(initialized());
  return core_->sync_manager()->HasUnsyncedItems();
}

bool SyncBackendHostImpl::IsNigoriEnabled() const {
  return registrar_.get() && registrar_->IsNigoriEnabled();
}

syncer::PassphraseType SyncBackendHostImpl::GetPassphraseType() const {
  return cached_passphrase_type_;
}

base::Time SyncBackendHostImpl::GetExplicitPassphraseTime() const {
  return cached_explicit_passphrase_time_;
}

bool SyncBackendHostImpl::IsCryptographerReady(
    const syncer::BaseTransaction* trans) const {
  return initialized() && trans->GetCryptographer() &&
      trans->GetCryptographer()->is_ready();
}

void SyncBackendHostImpl::GetModelSafeRoutingInfo(
    syncer::ModelSafeRoutingInfo* out) const {
  if (initialized()) {
    CHECK(registrar_.get());
    registrar_->GetModelSafeRoutingInfo(out);
  } else {
    NOTREACHED();
  }
}

void SyncBackendHostImpl::FlushDirectory() const {
  DCHECK(initialized());
  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHostCore::SaveChanges, core_));
}

void SyncBackendHostImpl::RequestBufferedProtocolEventsAndEnableForwarding() {
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          &SyncBackendHostCore::SendBufferedProtocolEventsAndEnableForwarding,
          core_));
}

void SyncBackendHostImpl::DisableProtocolEventForwarding() {
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DisableProtocolEventForwarding, core_));
}

void SyncBackendHostImpl::EnableDirectoryTypeDebugInfoForwarding() {
  DCHECK(initialized());
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::EnableDirectoryTypeDebugInfoForwarding,
                 core_));
}

void SyncBackendHostImpl::DisableDirectoryTypeDebugInfoForwarding() {
  DCHECK(initialized());
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DisableDirectoryTypeDebugInfoForwarding,
                 core_));
}

void SyncBackendHostImpl::GetAllNodesForTypes(
    syncer::ModelTypeSet types,
    base::Callback<void(const std::vector<syncer::ModelType>&,
                        ScopedVector<base::ListValue>)> callback) {
  DCHECK(initialized());
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::GetAllNodesForTypes, core_,
                            types, frontend_loop_->task_runner(), callback));
}

void SyncBackendHostImpl::InitCore(
    std::unique_ptr<DoInitializeOptions> options) {
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoInitialize, core_.get(),
                            base::Passed(&options)));
}

void SyncBackendHostImpl::RequestConfigureSyncer(
    syncer::ConfigureReason reason,
    syncer::ModelTypeSet to_download,
    syncer::ModelTypeSet to_purge,
    syncer::ModelTypeSet to_journal,
    syncer::ModelTypeSet to_unapply,
    syncer::ModelTypeSet to_ignore,
    const syncer::ModelSafeRoutingInfo& routing_info,
    const base::Callback<void(syncer::ModelTypeSet,
                              syncer::ModelTypeSet)>& ready_task,
    const base::Closure& retry_callback) {
  DoConfigureSyncerTypes config_types;
  config_types.to_download = to_download;
  config_types.to_purge = to_purge;
  config_types.to_journal = to_journal;
  config_types.to_unapply = to_unapply;
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoConfigureSyncer, core_.get(), reason,
                 config_types, routing_info, ready_task, retry_callback));
}

void SyncBackendHostImpl::FinishConfigureDataTypesOnFrontendLoop(
    const syncer::ModelTypeSet enabled_types,
    const syncer::ModelTypeSet succeeded_configuration_types,
    const syncer::ModelTypeSet failed_configuration_types,
    const base::Callback<void(syncer::ModelTypeSet,
                              syncer::ModelTypeSet)>& ready_task) {
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
  syncer::Experiments experiments;
  if (core_->sync_manager()->ReceivedExperiment(&experiments))
    frontend_->OnExperimentsChanged(experiments);
}

void SyncBackendHostImpl::HandleInitializationSuccessOnFrontendLoop(
    const syncer::WeakHandle<syncer::JsBackend> js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>
        debug_info_listener,
    std::unique_ptr<syncer_v2::ModelTypeConnector> model_type_connector,
    const std::string& cache_guid) {
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

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
  frontend_->OnBackendInitialized(js_backend,
                                  debug_info_listener,
                                  cache_guid,
                                  true);
}

void SyncBackendHostImpl::HandleInitializationFailureOnFrontendLoop() {
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  if (!frontend_)
    return;

  frontend_->OnBackendInitialized(
      syncer::WeakHandle<syncer::JsBackend>(),
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(),
      "",
      false);
}

void SyncBackendHostImpl::HandleSyncCycleCompletedOnFrontendLoop(
    const syncer::sessions::SyncSessionSnapshot& snapshot) {
  if (!frontend_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

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
    syncer::BootstrapTokenType token_type) {
  CHECK(sync_prefs_.get());
  if (token_type == syncer::PASSPHRASE_BOOTSTRAP_TOKEN)
    sync_prefs_->SetEncryptionBootstrapToken(token);
  else
    sync_prefs_->SetKeystoreEncryptionBootstrapToken(token);
}

void SyncBackendHostImpl::HandleActionableErrorEventOnFrontendLoop(
    const syncer::SyncProtocolError& sync_error) {
  if (!frontend_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  frontend_->OnActionableError(sync_error);
}

void SyncBackendHostImpl::HandleMigrationRequestedOnFrontendLoop(
    syncer::ModelTypeSet types) {
  if (!frontend_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  frontend_->OnMigrationNeededForTypes(types);
}

void SyncBackendHostImpl::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoOnInvalidatorStateChange,
                            core_.get(), state));
}

void SyncBackendHostImpl::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoOnIncomingInvalidation,
                            core_.get(), invalidation_map));
}

std::string SyncBackendHostImpl::GetOwnerName() const {
  return "SyncBackendHostImpl";
}

bool SyncBackendHostImpl::CheckPassphraseAgainstCachedPendingKeys(
    const std::string& passphrase) const {
  DCHECK(cached_pending_keys_.has_blob());
  DCHECK(!passphrase.empty());
  syncer::Nigori nigori;
  nigori.InitByDerivation("localhost", "dummy", passphrase);
  std::string plaintext;
  bool result = nigori.Decrypt(cached_pending_keys_.blob(), &plaintext);
  DVLOG_IF(1, result) << "Passphrase failed to decrypt pending keys.";
  return result;
}

void SyncBackendHostImpl::NotifyPassphraseRequired(
    syncer::PassphraseRequiredReason reason,
    sync_pb::EncryptedData pending_keys) {
  if (!frontend_)
    return;

  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

  // Update our cache of the cryptographer's pending keys.
  cached_pending_keys_ = pending_keys;

  frontend_->OnPassphraseRequired(reason, pending_keys);
}

void SyncBackendHostImpl::NotifyPassphraseAccepted() {
  if (!frontend_)
    return;

  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

  // Clear our cache of the cryptographer's pending keys.
  cached_pending_keys_.clear_blob();
  frontend_->OnPassphraseAccepted();
}

void SyncBackendHostImpl::NotifyEncryptedTypesChanged(
    syncer::ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  if (!frontend_)
    return;

  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  frontend_->OnEncryptedTypesChanged(
      encrypted_types, encrypt_everything);
}

void SyncBackendHostImpl::NotifyEncryptionComplete() {
  if (!frontend_)
    return;

  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  frontend_->OnEncryptionComplete();
}

void SyncBackendHostImpl::HandlePassphraseTypeChangedOnFrontendLoop(
    syncer::PassphraseType type,
    base::Time explicit_passphrase_time) {
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  DVLOG(1) << "Passphrase type changed to "
           << syncer::PassphraseTypeToString(type);
  cached_passphrase_type_ = type;
  cached_explicit_passphrase_time_ = explicit_passphrase_time;
}

void SyncBackendHostImpl::HandleLocalSetPassphraseEncryptionOnFrontendLoop(
    const syncer::SyncEncryptionHandler::NigoriState& nigori_state) {
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  frontend_->OnLocalSetPassphraseEncryption(nigori_state);
}

void SyncBackendHostImpl::HandleConnectionStatusChangeOnFrontendLoop(
    syncer::ConnectionStatus status) {
  if (!frontend_)
    return;

  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

  DVLOG(1) << "Connection status changed: "
           << syncer::ConnectionStatusToString(status);
  frontend_->OnConnectionStatusChange(status);
}

void SyncBackendHostImpl::HandleProtocolEventOnFrontendLoop(
    syncer::ProtocolEvent* event) {
  std::unique_ptr<syncer::ProtocolEvent> scoped_event(event);
  if (!frontend_)
    return;
  frontend_->OnProtocolEvent(*scoped_event);
}

void SyncBackendHostImpl::HandleDirectoryCommitCountersUpdatedOnFrontendLoop(
    syncer::ModelType type,
    const syncer::CommitCounters& counters) {
  if (!frontend_)
    return;
  frontend_->OnDirectoryTypeCommitCounterUpdated(type, counters);
}

void SyncBackendHostImpl::HandleDirectoryUpdateCountersUpdatedOnFrontendLoop(
    syncer::ModelType type,
    const syncer::UpdateCounters& counters) {
  if (!frontend_)
    return;
  frontend_->OnDirectoryTypeUpdateCounterUpdated(type, counters);
}

void SyncBackendHostImpl::HandleDirectoryStatusCountersUpdatedOnFrontendLoop(
    syncer::ModelType type,
    const syncer::StatusCounters& counters) {
  if (!frontend_)
    return;
  frontend_->OnDirectoryTypeStatusCounterUpdated(type, counters);
}

void SyncBackendHostImpl::UpdateInvalidationVersions(
    const std::map<syncer::ModelType, int64_t>& invalidation_versions) {
  sync_prefs_->UpdateInvalidationVersions(invalidation_versions);
}

base::MessageLoop* SyncBackendHostImpl::GetSyncLoopForTesting() {
  return registrar_->sync_thread()->message_loop();
}

void SyncBackendHostImpl::RefreshTypesForTest(syncer::ModelTypeSet types) {
  DCHECK(ui_thread_->BelongsToCurrentThread());

  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoRefreshTypes, core_.get(), types));
}

void SyncBackendHostImpl::ClearServerData(
    const syncer::SyncManager::ClearServerDataCallback& callback) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoClearServerData,
                            core_.get(), callback));
}

void SyncBackendHostImpl::OnCookieJarChanged(bool account_mismatch) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  registrar_->sync_thread()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendHostCore::DoOnCookieJarChanged,
                            core_.get(), account_mismatch));
}

void SyncBackendHostImpl::ClearServerDataDoneOnFrontendLoop(
    const syncer::SyncManager::ClearServerDataCallback& frontend_callback) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  frontend_callback.Run();
}

}  // namespace browser_sync

#undef SDVLOG

#undef SLOG
