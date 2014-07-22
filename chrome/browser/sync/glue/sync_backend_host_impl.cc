// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_host_impl.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/sync/glue/sync_backend_host_core.h"
#include "chrome/browser/sync/glue/sync_backend_registrar.h"
#include "chrome/common/chrome_switches.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/object_id_invalidation_map.h"
#include "components/network_time/network_time_tracker.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/sync_driver/sync_frontend.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "sync/internal_api/public/base_transaction.h"
#include "sync/internal_api/public/events/protocol_event.h"
#include "sync/internal_api/public/http_bridge.h"
#include "sync/internal_api/public/internal_components_factory.h"
#include "sync/internal_api/public/internal_components_factory_impl.h"
#include "sync/internal_api/public/network_resources.h"
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

namespace {

void UpdateNetworkTimeOnUIThread(base::Time network_time,
                                 base::TimeDelta resolution,
                                 base::TimeDelta latency,
                                 base::TimeTicks post_time) {
  g_browser_process->network_time_tracker()->UpdateNetworkTime(
      network_time, resolution, latency, post_time);
}

void UpdateNetworkTime(const base::Time& network_time,
                       const base::TimeDelta& resolution,
                       const base::TimeDelta& latency) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&UpdateNetworkTimeOnUIThread,
                 network_time, resolution, latency, base::TimeTicks::Now()));
}

}  // namespace

SyncBackendHostImpl::SyncBackendHostImpl(
    const std::string& name,
    Profile* profile,
    invalidation::InvalidationService* invalidator,
    const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
    const base::FilePath& sync_folder)
    : frontend_loop_(base::MessageLoop::current()),
      profile_(profile),
      name_(name),
      initialized_(false),
      sync_prefs_(sync_prefs),
      frontend_(NULL),
      cached_passphrase_type_(syncer::IMPLICIT_PASSPHRASE),
      invalidator_(invalidator),
      invalidation_handler_registered_(false),
      weak_ptr_factory_(this) {
  core_ = new SyncBackendHostCore(
      name_,
      profile_->GetPath().Append(sync_folder),
      sync_prefs_->HasSyncSetupCompleted(),
      weak_ptr_factory_.GetWeakPtr());
}

SyncBackendHostImpl::~SyncBackendHostImpl() {
  DCHECK(!core_.get() && !frontend_) << "Must call Shutdown before destructor.";
  DCHECK(!registrar_.get());
}

void SyncBackendHostImpl::Initialize(
    sync_driver::SyncFrontend* frontend,
    scoped_ptr<base::Thread> sync_thread,
    const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
    const GURL& sync_service_url,
    const syncer::SyncCredentials& credentials,
    bool delete_sync_data_folder,
    scoped_ptr<syncer::SyncManagerFactory> sync_manager_factory,
    scoped_ptr<syncer::UnrecoverableErrorHandler> unrecoverable_error_handler,
    syncer::ReportUnrecoverableErrorFunction
        report_unrecoverable_error_function,
    syncer::NetworkResources* network_resources) {
  registrar_.reset(new browser_sync::SyncBackendRegistrar(name_,
                                            profile_,
                                            sync_thread.Pass()));
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

  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kSyncShortInitialRetryOverride)) {
    factory_switches.backoff_override =
        InternalComponentsFactory::BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE;
  }
  if (cl->HasSwitch(switches::kSyncEnableGetUpdateAvoidance)) {
    factory_switches.pre_commit_updates_policy =
        InternalComponentsFactory::FORCE_ENABLE_PRE_COMMIT_UPDATE_AVOIDANCE;
  }

  SigninClient* signin_client =
      ChromeSigninClientFactory::GetForProfile(profile_);
  DCHECK(signin_client);
  std::string signin_scoped_device_id =
      signin_client->GetSigninScopedDeviceId();

  scoped_ptr<DoInitializeOptions> init_opts(new DoInitializeOptions(
      registrar_->sync_thread()->message_loop(),
      registrar_.get(),
      routing_info,
      workers,
      extensions_activity_monitor_.GetExtensionsActivity(),
      event_handler,
      sync_service_url,
      network_resources->GetHttpPostProviderFactory(
          make_scoped_refptr(profile_->GetRequestContext()),
          base::Bind(&UpdateNetworkTime),
          core_->GetRequestContextCancelationSignal()),
      credentials,
      invalidator_ ? invalidator_->GetInvalidatorClientId() : "",
      sync_manager_factory.Pass(),
      delete_sync_data_folder,
      sync_prefs_->GetEncryptionBootstrapToken(),
      sync_prefs_->GetKeystoreEncryptionBootstrapToken(),
      scoped_ptr<InternalComponentsFactory>(
          new syncer::InternalComponentsFactoryImpl(factory_switches)).Pass(),
      unrecoverable_error_handler.Pass(),
      report_unrecoverable_error_function,
      signin_scoped_device_id));
  InitCore(init_opts.Pass());
}

void SyncBackendHostImpl::UpdateCredentials(
    const syncer::SyncCredentials& credentials) {
  DCHECK(registrar_->sync_thread()->IsRunning());
  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoUpdateCredentials,
                 core_.get(),
                 credentials));
}

void SyncBackendHostImpl::StartSyncingWithServer() {
  SDVLOG(1) << "SyncBackendHostImpl::StartSyncingWithServer called.";

  syncer::ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);

  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoStartSyncing,
                 core_.get(), routing_info));
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
  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoSetEncryptionPassphrase,
                 core_.get(),
                 passphrase, is_explicit));
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
  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoSetDecryptionPassphrase,
                 core_.get(),
                 passphrase));

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

  // Stop listening for and forwarding locally-triggered sync refresh requests.
  notification_registrar_.RemoveAll();

  // Stop non-blocking sync types from sending any more requests to the syncer.
  sync_context_proxy_.reset();

  DCHECK(registrar_->sync_thread()->IsRunning());

  registrar_->RequestWorkerStopOnUIThread();

  core_->ShutdownOnUIThread();
}

scoped_ptr<base::Thread> SyncBackendHostImpl::Shutdown(ShutdownOption option) {
  // StopSyncingForShutdown() (which nulls out |frontend_|) should be
  // called first.
  DCHECK(!frontend_);
  DCHECK(registrar_->sync_thread()->IsRunning());

  bool sync_disabled = (option == DISABLE_AND_CLAIM_THREAD);
  bool sync_thread_claimed =
      (option == DISABLE_AND_CLAIM_THREAD || option == STOP_AND_CLAIM_THREAD);

  if (invalidation_handler_registered_) {
    if (sync_disabled) {
      UnregisterInvalidationIds();
    }
    invalidator_->UnregisterInvalidationHandler(this);
    invalidator_ = NULL;
  }
  invalidation_handler_registered_ = false;

  // Shut down and destroy sync manager.
  registrar_->sync_thread()->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoShutdown,
                 core_.get(), sync_disabled));
  core_ = NULL;

  // Worker cleanup.
  SyncBackendRegistrar* detached_registrar = registrar_.release();
  detached_registrar->sync_thread()->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendRegistrar::Shutdown,
                 base::Unretained(detached_registrar)));

  if (sync_thread_claimed)
    return detached_registrar->ReleaseSyncThread();
  else
    return scoped_ptr<base::Thread>();
}

void SyncBackendHostImpl::UnregisterInvalidationIds() {
  if (invalidation_handler_registered_) {
    invalidator_->UpdateRegisteredInvalidationIds(
        this,
        syncer::ObjectIdSet());
  }
}

void SyncBackendHostImpl::ConfigureDataTypes(
    syncer::ConfigureReason reason,
    const DataTypeConfigStateMap& config_state_map,
    const base::Callback<void(syncer::ModelTypeSet,
                              syncer::ModelTypeSet)>& ready_task,
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
}

void SyncBackendHostImpl::EnableEncryptEverything() {
  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
     base::Bind(&SyncBackendHostCore::DoEnableEncryptEverything,
                core_.get()));
}

void SyncBackendHostImpl::ActivateDataType(
    syncer::ModelType type, syncer::ModelSafeGroup group,
    sync_driver::ChangeProcessor* change_processor) {
  registrar_->ActivateDataType(type, group, change_processor, GetUserShare());
}

void SyncBackendHostImpl::DeactivateDataType(syncer::ModelType type) {
  registrar_->DeactivateDataType(type);
}

syncer::UserShare* SyncBackendHostImpl::GetUserShare() const {
  return core_->sync_manager()->GetUserShare();
}

scoped_ptr<syncer::SyncContextProxy>
SyncBackendHostImpl::GetSyncContextProxy() {
  return sync_context_proxy_.get() ? scoped_ptr<syncer::SyncContextProxy>(
                                         sync_context_proxy_->Clone())
                                   : scoped_ptr<syncer::SyncContextProxy>();
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

SyncedDeviceTracker* SyncBackendHostImpl::GetSyncedDeviceTracker() const {
  if (!initialized())
    return NULL;
  return core_->synced_device_tracker();
}

void SyncBackendHostImpl::RequestBufferedProtocolEventsAndEnableForwarding() {
  registrar_->sync_thread()->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(
          &SyncBackendHostCore::SendBufferedProtocolEventsAndEnableForwarding,
          core_));
}

void SyncBackendHostImpl::DisableProtocolEventForwarding() {
  registrar_->sync_thread()->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(
          &SyncBackendHostCore::DisableProtocolEventForwarding,
          core_));
}

void SyncBackendHostImpl::EnableDirectoryTypeDebugInfoForwarding() {
  DCHECK(initialized());
  registrar_->sync_thread()->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(
          &SyncBackendHostCore::EnableDirectoryTypeDebugInfoForwarding,
          core_));
}

void SyncBackendHostImpl::DisableDirectoryTypeDebugInfoForwarding() {
  DCHECK(initialized());
  registrar_->sync_thread()->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(
          &SyncBackendHostCore::DisableDirectoryTypeDebugInfoForwarding,
          core_));
}

void SyncBackendHostImpl::GetAllNodesForTypes(
    syncer::ModelTypeSet types,
    base::Callback<void(const std::vector<syncer::ModelType>&,
                        ScopedVector<base::ListValue>)> callback) {
  DCHECK(initialized());
  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
       base::Bind(
           &SyncBackendHostCore::GetAllNodesForTypes,
           core_,
           types,
           frontend_loop_->message_loop_proxy(),
           callback));
}

void SyncBackendHostImpl::InitCore(scoped_ptr<DoInitializeOptions> options) {
  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoInitialize,
                 core_.get(), base::Passed(&options)));
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
  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
       base::Bind(&SyncBackendHostCore::DoConfigureSyncer,
                  core_.get(),
                  reason,
                  config_types,
                  routing_info,
                  ready_task,
                  retry_callback));
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
    invalidator_->UpdateRegisteredInvalidationIds(
        this,
        ModelTypeSetToObjectIdSet(enabled_types));
  }

  if (!ready_task.is_null())
    ready_task.Run(succeeded_configuration_types, failed_configuration_types);
}

void SyncBackendHostImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(type, chrome::NOTIFICATION_SYNC_REFRESH_LOCAL);

  content::Details<const syncer::ModelTypeSet> state_details(details);
  const syncer::ModelTypeSet& types = *(state_details.ptr());
  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoRefreshTypes, core_.get(), types));
}

void SyncBackendHostImpl::AddExperimentalTypes() {
  CHECK(initialized());
  syncer::Experiments experiments;
  if (core_->sync_manager()->ReceivedExperiment(&experiments))
    frontend_->OnExperimentsChanged(experiments);
}

void SyncBackendHostImpl::HandleControlTypesDownloadRetry() {
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  if (!frontend_)
    return;

  frontend_->OnSyncConfigureRetry();
}

void SyncBackendHostImpl::HandleInitializationSuccessOnFrontendLoop(
    const syncer::WeakHandle<syncer::JsBackend> js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>
        debug_info_listener,
    syncer::SyncContextProxy* sync_context_proxy,
    const std::string& cache_guid) {
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

  if (sync_context_proxy)
    sync_context_proxy_ = sync_context_proxy->Clone();

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

  // Start forwarding refresh requests to the SyncManager
  notification_registrar_.Add(this, chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                              content::Source<Profile>(profile_));

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
  DCHECK(!token.empty());
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
  registrar_->sync_thread()->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoOnInvalidatorStateChange,
                 core_.get(),
                 state));
}

void SyncBackendHostImpl::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  registrar_->sync_thread()->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostCore::DoOnIncomingInvalidation,
                 core_.get(),
                 invalidation_map));
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
  scoped_ptr<syncer::ProtocolEvent> scoped_event(event);
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

base::MessageLoop* SyncBackendHostImpl::GetSyncLoopForTesting() {
  return registrar_->sync_thread()->message_loop();
}

}  // namespace browser_sync

#undef SDVLOG

#undef SLOG
