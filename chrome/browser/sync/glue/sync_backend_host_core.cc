// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_host_core.h"

#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/browser/sync/glue/invalidation_adapter.h"
#include "chrome/browser/sync/glue/sync_backend_registrar.h"
#include "chrome/browser/sync/glue/synced_device_tracker.h"
#include "chrome/common/chrome_version_info.h"
#include "components/invalidation/invalidation_util.h"
#include "components/invalidation/object_id_invalidation_map.h"
#include "sync/internal_api/public/events/protocol_event.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/internal_components_factory.h"
#include "sync/internal_api/public/sessions/commit_counters.h"
#include "sync/internal_api/public/sessions/status_counters.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/internal_api/public/sessions/update_counters.h"
#include "sync/internal_api/public/sync_context_proxy.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/sync_manager_factory.h"
#include "url/gurl.h"

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncers involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

static const int kSaveChangesIntervalSeconds = 10;

namespace syncer {
class InternalComponentsFactory;
}  // namespace syncer

namespace {

// Enums for UMAs.
enum SyncBackendInitState {
    SETUP_COMPLETED_FOUND_RESTORED_TYPES = 0,
    SETUP_COMPLETED_NO_RESTORED_TYPES,
    FIRST_SETUP_NO_RESTORED_TYPES,
    FIRST_SETUP_RESTORED_TYPES,
    SYNC_BACKEND_INIT_STATE_COUNT
};

}  // namespace

namespace browser_sync {

DoInitializeOptions::DoInitializeOptions(
    base::MessageLoop* sync_loop,
    SyncBackendRegistrar* registrar,
    const syncer::ModelSafeRoutingInfo& routing_info,
    const std::vector<scoped_refptr<syncer::ModelSafeWorker> >& workers,
    const scoped_refptr<syncer::ExtensionsActivity>& extensions_activity,
    const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
    const GURL& service_url,
    scoped_ptr<syncer::HttpPostProviderFactory> http_bridge_factory,
    const syncer::SyncCredentials& credentials,
    const std::string& invalidator_client_id,
    scoped_ptr<syncer::SyncManagerFactory> sync_manager_factory,
    bool delete_sync_data_folder,
    const std::string& restored_key_for_bootstrapping,
    const std::string& restored_keystore_key_for_bootstrapping,
    scoped_ptr<syncer::InternalComponentsFactory> internal_components_factory,
    scoped_ptr<syncer::UnrecoverableErrorHandler> unrecoverable_error_handler,
    syncer::ReportUnrecoverableErrorFunction
        report_unrecoverable_error_function,
    const std::string& signin_scoped_device_id)
    : sync_loop(sync_loop),
      registrar(registrar),
      routing_info(routing_info),
      workers(workers),
      extensions_activity(extensions_activity),
      event_handler(event_handler),
      service_url(service_url),
      http_bridge_factory(http_bridge_factory.Pass()),
      credentials(credentials),
      invalidator_client_id(invalidator_client_id),
      sync_manager_factory(sync_manager_factory.Pass()),
      delete_sync_data_folder(delete_sync_data_folder),
      restored_key_for_bootstrapping(restored_key_for_bootstrapping),
      restored_keystore_key_for_bootstrapping(
          restored_keystore_key_for_bootstrapping),
      internal_components_factory(internal_components_factory.Pass()),
      unrecoverable_error_handler(unrecoverable_error_handler.Pass()),
      report_unrecoverable_error_function(report_unrecoverable_error_function),
      signin_scoped_device_id(signin_scoped_device_id) {
}

DoInitializeOptions::~DoInitializeOptions() {}

DoConfigureSyncerTypes::DoConfigureSyncerTypes() {}

DoConfigureSyncerTypes::~DoConfigureSyncerTypes() {}

SyncBackendHostCore::SyncBackendHostCore(
    const std::string& name,
    const base::FilePath& sync_data_folder_path,
    bool has_sync_setup_completed,
    const base::WeakPtr<SyncBackendHostImpl>& backend)
    : name_(name),
      sync_data_folder_path_(sync_data_folder_path),
      host_(backend),
      sync_loop_(NULL),
      registrar_(NULL),
      has_sync_setup_completed_(has_sync_setup_completed),
      forward_protocol_events_(false),
      forward_type_info_(false),
      weak_ptr_factory_(this) {
  DCHECK(backend.get());
}

SyncBackendHostCore::~SyncBackendHostCore() {
  DCHECK(!sync_manager_.get());
}

void SyncBackendHostCore::OnSyncCycleCompleted(
    const syncer::sessions::SyncSessionSnapshot& snapshot) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);

  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::HandleSyncCycleCompletedOnFrontendLoop,
      snapshot);
}

void SyncBackendHostCore::DoRefreshTypes(syncer::ModelTypeSet types) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->RefreshTypes(types);
}

void SyncBackendHostCore::OnControlTypesDownloadRetry() {
  host_.Call(FROM_HERE,
             &SyncBackendHostImpl::HandleControlTypesDownloadRetry);
}

void SyncBackendHostCore::OnInitializationComplete(
    const syncer::WeakHandle<syncer::JsBackend>& js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    bool success,
    const syncer::ModelTypeSet restored_types) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);

  if (!success) {
    DoDestroySyncManager(syncer::STOP_SYNC);
    host_.Call(FROM_HERE,
               &SyncBackendHostImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  // Register for encryption related changes now. We have to do this before
  // the initializing downloading control types or initializing the encryption
  // handler in order to receive notifications triggered during encryption
  // startup.
  sync_manager_->GetEncryptionHandler()->AddObserver(this);

  // Sync manager initialization is complete, so we can schedule recurring
  // SaveChanges.
  sync_loop_->PostTask(FROM_HERE,
                       base::Bind(&SyncBackendHostCore::StartSavingChanges,
                                  weak_ptr_factory_.GetWeakPtr()));

  // Hang on to these for a while longer.  We're not ready to hand them back to
  // the UI thread yet.
  js_backend_ = js_backend;
  debug_info_listener_ = debug_info_listener;

  // Track whether or not sync DB and preferences were in sync.
  SyncBackendInitState backend_init_state;
  if (has_sync_setup_completed_ && !restored_types.Empty()) {
    backend_init_state = SETUP_COMPLETED_FOUND_RESTORED_TYPES;
  } else if (has_sync_setup_completed_ && restored_types.Empty()) {
    backend_init_state = SETUP_COMPLETED_NO_RESTORED_TYPES;
  } else if (!has_sync_setup_completed_ && restored_types.Empty()) {
    backend_init_state = FIRST_SETUP_NO_RESTORED_TYPES;
  } else { // (!has_sync_setup_completed_ && !restored_types.Empty())
    backend_init_state = FIRST_SETUP_RESTORED_TYPES;
  }

  UMA_HISTOGRAM_ENUMERATION("Sync.BackendInitializeRestoreState",
                            backend_init_state,
                            SYNC_BACKEND_INIT_STATE_COUNT);

  // Before proceeding any further, we need to download the control types and
  // purge any partial data (ie. data downloaded for a type that was on its way
  // to being initially synced, but didn't quite make it.).  The following
  // configure cycle will take care of this.  It depends on the registrar state
  // which we initialize below to ensure that we don't perform any downloads if
  // all control types have already completed their initial sync.
  registrar_->SetInitialTypes(restored_types);

  syncer::ConfigureReason reason =
      restored_types.Empty() ?
       syncer::CONFIGURE_REASON_NEW_CLIENT :
       syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE;

  syncer::ModelTypeSet new_control_types = registrar_->ConfigureDataTypes(
      syncer::ControlTypes(), syncer::ModelTypeSet());
  syncer::ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);
  SDVLOG(1) << "Control Types "
            << syncer::ModelTypeSetToString(new_control_types)
            << " added; calling ConfigureSyncer";

  syncer::ModelTypeSet types_to_purge =
      syncer::Difference(syncer::ModelTypeSet::All(),
                         GetRoutingInfoTypes(routing_info));

  sync_manager_->ConfigureSyncer(
      reason,
      new_control_types,
      types_to_purge,
      syncer::ModelTypeSet(),
      syncer::ModelTypeSet(),
      routing_info,
      base::Bind(&SyncBackendHostCore::DoInitialProcessControlTypes,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&SyncBackendHostCore::OnControlTypesDownloadRetry,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SyncBackendHostCore::OnConnectionStatusChange(
    syncer::ConnectionStatus status) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::HandleConnectionStatusChangeOnFrontendLoop, status);
}

void SyncBackendHostCore::OnPassphraseRequired(
    syncer::PassphraseRequiredReason reason,
    const sync_pb::EncryptedData& pending_keys) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::NotifyPassphraseRequired, reason, pending_keys);
}

void SyncBackendHostCore::OnPassphraseAccepted() {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::NotifyPassphraseAccepted);
}

void SyncBackendHostCore::OnBootstrapTokenUpdated(
    const std::string& bootstrap_token,
    syncer::BootstrapTokenType type) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(FROM_HERE,
             &SyncBackendHostImpl::PersistEncryptionBootstrapToken,
             bootstrap_token,
             type);
}

void SyncBackendHostCore::OnEncryptedTypesChanged(
    syncer::ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  // NOTE: We're in a transaction.
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::NotifyEncryptedTypesChanged,
      encrypted_types, encrypt_everything);
}

void SyncBackendHostCore::OnEncryptionComplete() {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  // NOTE: We're in a transaction.
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::NotifyEncryptionComplete);
}

void SyncBackendHostCore::OnCryptographerStateChanged(
    syncer::Cryptographer* cryptographer) {
  // Do nothing.
}

void SyncBackendHostCore::OnPassphraseTypeChanged(
    syncer::PassphraseType type, base::Time passphrase_time) {
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::HandlePassphraseTypeChangedOnFrontendLoop,
      type, passphrase_time);
}

void SyncBackendHostCore::OnCommitCountersUpdated(
    syncer::ModelType type,
    const syncer::CommitCounters& counters) {
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::HandleDirectoryCommitCountersUpdatedOnFrontendLoop,
      type, counters);
}

void SyncBackendHostCore::OnUpdateCountersUpdated(
    syncer::ModelType type,
    const syncer::UpdateCounters& counters) {
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::HandleDirectoryUpdateCountersUpdatedOnFrontendLoop,
      type, counters);
}

void SyncBackendHostCore::OnStatusCountersUpdated(
    syncer::ModelType type,
    const syncer::StatusCounters& counters) {
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::HandleDirectoryStatusCountersUpdatedOnFrontendLoop,
      type, counters);
}

void SyncBackendHostCore::OnActionableError(
    const syncer::SyncProtocolError& sync_error) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::HandleActionableErrorEventOnFrontendLoop,
      sync_error);
}

void SyncBackendHostCore::OnMigrationRequested(syncer::ModelTypeSet types) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHostImpl::HandleMigrationRequestedOnFrontendLoop,
      types);
}

void SyncBackendHostCore::OnProtocolEvent(
    const syncer::ProtocolEvent& event) {
  // TODO(rlarocque): Find a way to pass event_clone as a scoped_ptr.
  if (forward_protocol_events_) {
    scoped_ptr<syncer::ProtocolEvent> event_clone(event.Clone());
    host_.Call(
        FROM_HERE,
        &SyncBackendHostImpl::HandleProtocolEventOnFrontendLoop,
        event_clone.release());
  }
}

void SyncBackendHostCore::DoOnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->SetInvalidatorEnabled(state == syncer::INVALIDATIONS_ENABLED);
}

void SyncBackendHostCore::DoOnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);

  syncer::ObjectIdSet ids = invalidation_map.GetObjectIds();
  for (syncer::ObjectIdSet::const_iterator ids_it = ids.begin();
       ids_it != ids.end();
       ++ids_it) {
    syncer::ModelType type;
    if (!NotificationTypeToRealModelType(ids_it->name(), &type)) {
      DLOG(WARNING) << "Notification has invalid id: "
                    << syncer::ObjectIdToString(*ids_it);
    } else {
      syncer::SingleObjectInvalidationSet invalidation_set =
          invalidation_map.ForObject(*ids_it);
      for (syncer::SingleObjectInvalidationSet::const_iterator inv_it =
               invalidation_set.begin();
           inv_it != invalidation_set.end();
           ++inv_it) {
        scoped_ptr<syncer::InvalidationInterface> inv_adapter(
            new InvalidationAdapter(*inv_it));
        sync_manager_->OnIncomingInvalidation(type, inv_adapter.Pass());
      }
    }
  }
}

void SyncBackendHostCore::DoInitialize(
    scoped_ptr<DoInitializeOptions> options) {
  DCHECK(!sync_loop_);
  sync_loop_ = options->sync_loop;
  DCHECK(sync_loop_);

  signin_scoped_device_id_ = options->signin_scoped_device_id;

  // Finish initializing the HttpBridgeFactory.  We do this here because
  // building the user agent may block on some platforms.
  chrome::VersionInfo version_info;
  options->http_bridge_factory->Init(
      DeviceInfo::MakeUserAgentForSyncApi(version_info));

  // Blow away the partial or corrupt sync data folder before doing any more
  // initialization, if necessary.
  if (options->delete_sync_data_folder) {
    DeleteSyncDataFolder();
  }

  // Make sure that the directory exists before initializing the backend.
  // If it already exists, this will do no harm.
  if (!base::CreateDirectory(sync_data_folder_path_)) {
    DLOG(FATAL) << "Sync Data directory creation failed.";
  }

  DCHECK(!registrar_);
  registrar_ = options->registrar;
  DCHECK(registrar_);

  sync_manager_ = options->sync_manager_factory->CreateSyncManager(name_);
  sync_manager_->AddObserver(this);
  sync_manager_->Init(sync_data_folder_path_,
                      options->event_handler,
                      options->service_url,
                      options->http_bridge_factory.Pass(),
                      options->workers,
                      options->extensions_activity,
                      options->registrar /* as SyncManager::ChangeDelegate */,
                      options->credentials,
                      options->invalidator_client_id,
                      options->restored_key_for_bootstrapping,
                      options->restored_keystore_key_for_bootstrapping,
                      options->internal_components_factory.get(),
                      &encryptor_,
                      options->unrecoverable_error_handler.Pass(),
                      options->report_unrecoverable_error_function,
                      &stop_syncing_signal_);
}

void SyncBackendHostCore::DoUpdateCredentials(
    const syncer::SyncCredentials& credentials) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  // UpdateCredentials can be called during backend initialization, possibly
  // when backend initialization has failed but hasn't notified the UI thread
  // yet. In that case, the sync manager may have been destroyed on the sync
  // thread before this task was executed, so we do nothing.
  if (sync_manager_) {
    sync_manager_->UpdateCredentials(credentials);
  }
}

void SyncBackendHostCore::DoStartSyncing(
    const syncer::ModelSafeRoutingInfo& routing_info) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->StartSyncingNormally(routing_info);
}

void SyncBackendHostCore::DoSetEncryptionPassphrase(
    const std::string& passphrase,
    bool is_explicit) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->GetEncryptionHandler()->SetEncryptionPassphrase(
      passphrase, is_explicit);
}

void SyncBackendHostCore::DoInitialProcessControlTypes() {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);

  DVLOG(1) << "Initilalizing Control Types";

  // Initialize encryption.
  sync_manager_->GetEncryptionHandler()->Init();

  // Note: experiments are currently handled via SBH::AddExperimentalTypes,
  // which is called at the end of every sync cycle.
  // TODO(zea): eventually add an experiment handler and initialize it here.

  if (!sync_manager_->GetUserShare()) {  // NULL in some tests.
    DVLOG(1) << "Skipping initialization of DeviceInfo";
    host_.Call(
        FROM_HERE,
        &SyncBackendHostImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  if (!sync_manager_->InitialSyncEndedTypes().HasAll(syncer::ControlTypes())) {
    LOG(ERROR) << "Failed to download control types";
    host_.Call(
        FROM_HERE,
        &SyncBackendHostImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  // Initialize device info. This is asynchronous on some platforms, so we
  // provide a callback for when it finishes.
  synced_device_tracker_.reset(
      new SyncedDeviceTracker(sync_manager_->GetUserShare(),
                              sync_manager_->cache_guid()));
  synced_device_tracker_->InitLocalDeviceInfo(
      signin_scoped_device_id_,
      base::Bind(&SyncBackendHostCore::DoFinishInitialProcessControlTypes,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SyncBackendHostCore::DoFinishInitialProcessControlTypes() {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);

  registrar_->ActivateDataType(syncer::DEVICE_INFO,
                               syncer::GROUP_PASSIVE,
                               synced_device_tracker_.get(),
                               sync_manager_->GetUserShare());

  host_.Call(FROM_HERE,
             &SyncBackendHostImpl::HandleInitializationSuccessOnFrontendLoop,
             js_backend_,
             debug_info_listener_,
             sync_manager_->GetSyncContextProxy(),
             sync_manager_->cache_guid());

  js_backend_.Reset();
  debug_info_listener_.Reset();
}

void SyncBackendHostCore::DoSetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->GetEncryptionHandler()->SetDecryptionPassphrase(
      passphrase);
}

void SyncBackendHostCore::DoEnableEncryptEverything() {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->GetEncryptionHandler()->EnableEncryptEverything();
}

void SyncBackendHostCore::ShutdownOnUIThread() {
  // This will cut short any blocking network tasks, cut short any in-progress
  // sync cycles, and prevent the creation of new blocking network tasks and new
  // sync cycles.  If there was an in-progress network request, it would have
  // had a reference to the RequestContextGetter.  This reference will be
  // dropped by the time this function returns.
  //
  // It is safe to call this even if Sync's backend classes have not been
  // initialized yet.  Those classes will receive the message when the sync
  // thread finally getes around to constructing them.
  stop_syncing_signal_.Signal();

  // This will drop the HttpBridgeFactory's reference to the
  // RequestContextGetter.  Once this has been called, the HttpBridgeFactory can
  // no longer be used to create new HttpBridge instances.  We can get away with
  // this because the stop_syncing_signal_ has already been signalled, which
  // guarantees that the ServerConnectionManager will no longer attempt to
  // create new connections.
  release_request_context_signal_.Signal();
}

void SyncBackendHostCore::DoShutdown(syncer::ShutdownReason reason) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);

  // It's safe to do this even if the type was never activated.
  registrar_->DeactivateDataType(syncer::DEVICE_INFO);
  synced_device_tracker_.reset();

  DoDestroySyncManager(reason);

  registrar_ = NULL;

  if (reason == syncer::DISABLE_SYNC)
    DeleteSyncDataFolder();

  host_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SyncBackendHostCore::DoDestroySyncManager(syncer::ShutdownReason reason) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  if (sync_manager_) {
    DisableDirectoryTypeDebugInfoForwarding();
    save_changes_timer_.reset();
    sync_manager_->RemoveObserver(this);
    sync_manager_->ShutdownOnSyncThread(reason);
    sync_manager_.reset();
  }
}

void SyncBackendHostCore::DoConfigureSyncer(
    syncer::ConfigureReason reason,
    const DoConfigureSyncerTypes& config_types,
    const syncer::ModelSafeRoutingInfo routing_info,
    const base::Callback<void(syncer::ModelTypeSet,
                              syncer::ModelTypeSet)>& ready_task,
    const base::Closure& retry_callback) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->ConfigureSyncer(
      reason,
      config_types.to_download,
      config_types.to_purge,
      config_types.to_journal,
      config_types.to_unapply,
      routing_info,
      base::Bind(&SyncBackendHostCore::DoFinishConfigureDataTypes,
                 weak_ptr_factory_.GetWeakPtr(),
                 config_types.to_download,
                 ready_task),
      base::Bind(&SyncBackendHostCore::DoRetryConfiguration,
                 weak_ptr_factory_.GetWeakPtr(),
                 retry_callback));
}

void SyncBackendHostCore::DoFinishConfigureDataTypes(
    syncer::ModelTypeSet types_to_config,
    const base::Callback<void(syncer::ModelTypeSet,
                              syncer::ModelTypeSet)>& ready_task) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);

  // Update the enabled types for the bridge and sync manager.
  syncer::ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);
  syncer::ModelTypeSet enabled_types = GetRoutingInfoTypes(routing_info);
  enabled_types.RemoveAll(syncer::ProxyTypes());

  const syncer::ModelTypeSet failed_configuration_types =
      Difference(types_to_config, sync_manager_->InitialSyncEndedTypes());
  const syncer::ModelTypeSet succeeded_configuration_types =
      Difference(types_to_config, failed_configuration_types);
  host_.Call(FROM_HERE,
             &SyncBackendHostImpl::FinishConfigureDataTypesOnFrontendLoop,
             enabled_types,
             succeeded_configuration_types,
             failed_configuration_types,
             ready_task);
}

void SyncBackendHostCore::DoRetryConfiguration(
    const base::Closure& retry_callback) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(FROM_HERE,
             &SyncBackendHostImpl::RetryConfigurationOnFrontendLoop,
             retry_callback);
}

void SyncBackendHostCore::SendBufferedProtocolEventsAndEnableForwarding() {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  forward_protocol_events_ = true;

  if (sync_manager_) {
    // Grab our own copy of the buffered events.
    // The buffer is not modified by this operation.
    std::vector<syncer::ProtocolEvent*> buffered_events;
    sync_manager_->GetBufferedProtocolEvents().release(&buffered_events);

    // Send them all over the fence to the host.
    for (std::vector<syncer::ProtocolEvent*>::iterator it =
         buffered_events.begin(); it != buffered_events.end(); ++it) {
      // TODO(rlarocque): Make it explicit that host_ takes ownership.
      host_.Call(
          FROM_HERE,
          &SyncBackendHostImpl::HandleProtocolEventOnFrontendLoop,
          *it);
    }
  }
}

void SyncBackendHostCore::DisableProtocolEventForwarding() {
  forward_protocol_events_ = false;
}

void SyncBackendHostCore::EnableDirectoryTypeDebugInfoForwarding() {
  DCHECK(sync_manager_);

  forward_type_info_ = true;

  if (!sync_manager_->HasDirectoryTypeDebugInfoObserver(this))
    sync_manager_->RegisterDirectoryTypeDebugInfoObserver(this);
  sync_manager_->RequestEmitDebugInfo();
}

void SyncBackendHostCore::DisableDirectoryTypeDebugInfoForwarding() {
  DCHECK(sync_manager_);

  if (!forward_type_info_)
    return;

  forward_type_info_ = false;

  if (sync_manager_->HasDirectoryTypeDebugInfoObserver(this))
    sync_manager_->UnregisterDirectoryTypeDebugInfoObserver(this);
}

void SyncBackendHostCore::DeleteSyncDataFolder() {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  if (base::DirectoryExists(sync_data_folder_path_)) {
    if (!base::DeleteFile(sync_data_folder_path_, true))
      SLOG(DFATAL) << "Could not delete the Sync Data folder.";
  }
}

void SyncBackendHostCore::GetAllNodesForTypes(
    syncer::ModelTypeSet types,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Callback<void(const std::vector<syncer::ModelType>& type,
                        ScopedVector<base::ListValue>)> callback) {
  std::vector<syncer::ModelType> types_vector;
  ScopedVector<base::ListValue> node_lists;

  syncer::ModelSafeRoutingInfo routes;
  registrar_->GetModelSafeRoutingInfo(&routes);
  syncer::ModelTypeSet enabled_types = GetRoutingInfoTypes(routes);

  for (syncer::ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    types_vector.push_back(it.Get());
    if (!enabled_types.Has(it.Get())) {
      node_lists.push_back(new base::ListValue());
    } else {
      node_lists.push_back(
          sync_manager_->GetAllNodesForType(it.Get()).release());
    }
  }

  task_runner->PostTask(
      FROM_HERE,
      base::Bind(callback, types_vector, base::Passed(&node_lists)));
}

void SyncBackendHostCore::StartSavingChanges() {
  // We may already be shut down.
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  DCHECK(!save_changes_timer_.get());
  save_changes_timer_.reset(new base::RepeatingTimer<SyncBackendHostCore>());
  save_changes_timer_->Start(FROM_HERE,
      base::TimeDelta::FromSeconds(kSaveChangesIntervalSeconds),
      this, &SyncBackendHostCore::SaveChanges);
}

void SyncBackendHostCore::SaveChanges() {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->SaveChanges();
}

}  // namespace browser_sync
