// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_host.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/timer.h"
#include "base/tracked_objects.h"
#include "build/build_config.h"
#include "chrome/browser/invalidation/invalidation_service.h"
#include "chrome/browser/invalidation/invalidation_service_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/network_time/network_time_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/chrome_encryptor.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/browser/sync/glue/sync_backend_registrar.h"
#include "chrome/browser/sync/glue/synced_device_tracker.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/content_client.h"
#include "google_apis/gaia/gaia_constants.h"
#include "jingle/notifier/base/notification_method.h"
#include "jingle/notifier/base/notifier_options.h"
#include "net/base/host_port_pair.h"
#include "net/url_request/url_request_context_getter.h"
#include "sync/internal_api/public/base/cancelation_signal.h"
#include "sync/internal_api/public/base_transaction.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/http_bridge.h"
#include "sync/internal_api/public/internal_components_factory_impl.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/sync_manager_factory.h"
#include "sync/internal_api/public/util/experiments.h"
#include "sync/internal_api/public/util/sync_string_conversions.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/util/nigori.h"

static const int kSaveChangesIntervalSeconds = 10;
static const base::FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

typedef TokenService::TokenAvailableDetails TokenAvailableDetails;

typedef GoogleServiceAuthError AuthError;

namespace browser_sync {

using content::BrowserThread;
using syncer::InternalComponentsFactory;
using syncer::InternalComponentsFactoryImpl;
using syncer::sessions::SyncSessionSnapshot;
using syncer::SyncCredentials;

namespace {

// Enums for UMAs.
enum SyncBackendInitState {
    SETUP_COMPLETED_FOUND_RESTORED_TYPES = 0,
    SETUP_COMPLETED_NO_RESTORED_TYPES,
    FIRST_SETUP_NO_RESTORED_TYPES,
    FIRST_SETUP_RESTORED_TYPES,
    SYNC_BACKEND_INIT_STATE_COUNT
};

// Helper struct to handle currying params to
// SyncBackendHost::Core::DoConfigureSyncer.
struct DoConfigureSyncerTypes {
  DoConfigureSyncerTypes() {}
  ~DoConfigureSyncerTypes() {}
  syncer::ModelTypeSet to_download;
  syncer::ModelTypeSet to_purge;
  syncer::ModelTypeSet to_journal;
  syncer::ModelTypeSet to_unapply;
};

}  // namespace

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncers involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

class SyncBackendHost::Core
    : public base::RefCountedThreadSafe<SyncBackendHost::Core>,
      public syncer::SyncEncryptionHandler::Observer,
      public syncer::SyncManager::Observer {
 public:
  Core(const std::string& name,
       const base::FilePath& sync_data_folder_path,
       bool has_sync_setup_completed,
       const base::WeakPtr<SyncBackendHost>& backend);

  // SyncManager::Observer implementation.  The Core just acts like an air
  // traffic controller here, forwarding incoming messages to appropriate
  // landing threads.
  virtual void OnSyncCycleCompleted(
      const syncer::sessions::SyncSessionSnapshot& snapshot) OVERRIDE;
  virtual void OnInitializationComplete(
      const syncer::WeakHandle<syncer::JsBackend>& js_backend,
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      bool success,
      syncer::ModelTypeSet restored_types) OVERRIDE;
  virtual void OnConnectionStatusChange(
      syncer::ConnectionStatus status) OVERRIDE;
  virtual void OnStopSyncingPermanently() OVERRIDE;
  virtual void OnActionableError(
      const syncer::SyncProtocolError& sync_error) OVERRIDE;

  // SyncEncryptionHandler::Observer implementation.
  virtual void OnPassphraseRequired(
      syncer::PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) OVERRIDE;
  virtual void OnPassphraseAccepted() OVERRIDE;
  virtual void OnBootstrapTokenUpdated(
      const std::string& bootstrap_token,
      syncer::BootstrapTokenType type) OVERRIDE;
  virtual void OnEncryptedTypesChanged(
      syncer::ModelTypeSet encrypted_types,
      bool encrypt_everything) OVERRIDE;
  virtual void OnEncryptionComplete() OVERRIDE;
  virtual void OnCryptographerStateChanged(
      syncer::Cryptographer* cryptographer) OVERRIDE;
  virtual void OnPassphraseTypeChanged(syncer::PassphraseType type,
                                       base::Time passphrase_time) OVERRIDE;

  // Forwards an invalidation state change to the sync manager.
  void DoOnInvalidatorStateChange(syncer::InvalidatorState state);

  // Forwards an invalidation to the sync manager.
  void DoOnIncomingInvalidation(
      syncer::ObjectIdInvalidationMap invalidation_map);

  // Note:
  //
  // The Do* methods are the various entry points from our
  // SyncBackendHost.  They are all called on the sync thread to
  // actually perform synchronous (and potentially blocking) syncapi
  // operations.
  //
  // Called to perform initialization of the syncapi on behalf of
  // SyncBackendHost::Initialize.
  void DoInitialize(scoped_ptr<DoInitializeOptions> options);

  // Called to perform credential update on behalf of
  // SyncBackendHost::UpdateCredentials.
  void DoUpdateCredentials(const syncer::SyncCredentials& credentials);

  // Called to tell the syncapi to start syncing (generally after
  // initialization and authentication).
  void DoStartSyncing(const syncer::ModelSafeRoutingInfo& routing_info);

  // Called to set the passphrase for encryption.
  void DoSetEncryptionPassphrase(const std::string& passphrase,
                                 bool is_explicit);

  // Called to decrypt the pending keys.
  void DoSetDecryptionPassphrase(const std::string& passphrase);

  // Called to turn on encryption of all sync data as well as
  // reencrypt everything.
  void DoEnableEncryptEverything();

  // Ask the syncer to check for updates for the specified types.
  void DoRefreshTypes(syncer::ModelTypeSet types);

  // Invoked if we failed to download the necessary control types at startup.
  // Invokes SyncBackendHost::HandleControlTypesDownloadRetry.
  void OnControlTypesDownloadRetry();

  // Called to perform tasks which require the control data to be downloaded.
  // This includes refreshing encryption, setting up the device info change
  // processor, etc.
  void DoInitialProcessControlTypes();

  // Some parts of DoInitialProcessControlTypes() may be executed on a different
  // thread.  This function asynchronously continues the work started in
  // DoInitialProcessControlTypes() once that other thread gets back to us.
  void DoFinishInitialProcessControlTypes();

  // The shutdown order is a bit complicated:
  // 1) Call ShutdownOnUIThread() from |frontend_loop_| to request sync manager
  //    to stop as soon as possible.
  // 2) Post DoShutdown() to sync loop to clean up backend state, save
  //    directory and destroy sync manager.
  void ShutdownOnUIThread();
  void DoShutdown(bool sync_disabled);
  void DoDestroySyncManager();

  // Configuration methods that must execute on sync loop.
  void DoConfigureSyncer(
      syncer::ConfigureReason reason,
      const DoConfigureSyncerTypes& config_types,
      const syncer::ModelSafeRoutingInfo routing_info,
      const base::Callback<void(syncer::ModelTypeSet,
                                syncer::ModelTypeSet)>& ready_task,
      const base::Closure& retry_callback);
  void DoFinishConfigureDataTypes(
      syncer::ModelTypeSet types_to_config,
      const base::Callback<void(syncer::ModelTypeSet,
                                syncer::ModelTypeSet)>& ready_task);
  void DoRetryConfiguration(
      const base::Closure& retry_callback);

  // Set the base request context to use when making HTTP calls.
  // This method will add a reference to the context to persist it
  // on the IO thread. Must be removed from IO thread.

  syncer::SyncManager* sync_manager() { return sync_manager_.get(); }

  SyncedDeviceTracker* synced_device_tracker() {
    return synced_device_tracker_.get();
  }

  // Delete the sync data folder to cleanup backend data.  Happens the first
  // time sync is enabled for a user (to prevent accidentally reusing old
  // sync databases), as well as shutdown when you're no longer syncing.
  void DeleteSyncDataFolder();

  // We expose this member because it's required in the construction of the
  // HttpBridgeFactory.
  syncer::CancelationSignal* GetRequestContextCancelationSignal() {
    return &release_request_context_signal_;
  }

 private:
  friend class base::RefCountedThreadSafe<SyncBackendHost::Core>;
  friend class SyncBackendHostForProfileSyncTest;

  virtual ~Core();

  // Invoked when initialization of syncapi is complete and we can start
  // our timer.
  // This must be called from the thread on which SaveChanges is intended to
  // be run on; the host's |registrar_->sync_thread()|.
  void StartSavingChanges();

  // Invoked periodically to tell the syncapi to persist its state
  // by writing to disk.
  // This is called from the thread we were created on (which is sync thread),
  // using a repeating timer that is kicked off as soon as the SyncManager
  // tells us it completed initialization.
  void SaveChanges();

  // Name used for debugging.
  const std::string name_;

  // Path of the folder that stores the sync data files.
  const base::FilePath sync_data_folder_path_;

  // Our parent SyncBackendHost.
  syncer::WeakHandle<SyncBackendHost> host_;

  // The loop where all the sync backend operations happen.
  // Non-NULL only between calls to DoInitialize() and ~Core().
  base::MessageLoop* sync_loop_;

  // Our parent's registrar (not owned).  Non-NULL only between
  // calls to DoInitialize() and DoShutdown().
  SyncBackendRegistrar* registrar_;

  // The timer used to periodically call SaveChanges.
  scoped_ptr<base::RepeatingTimer<Core> > save_changes_timer_;

  // Our encryptor, which uses Chrome's encryption functions.
  ChromeEncryptor encryptor_;

  // A special ChangeProcessor that tracks the DEVICE_INFO type for us.
  scoped_ptr<SyncedDeviceTracker> synced_device_tracker_;

  // The top-level syncapi entry point.  Lives on the sync thread.
  scoped_ptr<syncer::SyncManager> sync_manager_;

  // Temporary holder of sync manager's initialization results. Set by
  // OnInitializeComplete, and consumed when we pass it via OnBackendInitialized
  // in the final state of HandleInitializationSuccessOnFrontendLoop.
  syncer::WeakHandle<syncer::JsBackend> js_backend_;
  syncer::WeakHandle<syncer::DataTypeDebugInfoListener> debug_info_listener_;

  // These signals allow us to send requests to shut down the HttpBridgeFactory
  // and ServerConnectionManager without having to wait for those classes to
  // finish initializing first.
  //
  // See comments in Core::ShutdownOnUIThread() for more details.
  syncer::CancelationSignal release_request_context_signal_;
  syncer::CancelationSignal stop_syncing_signal_;

  // Matches the value of SyncPref's HasSyncSetupCompleted() flag at init time.
  // Should not be used for anything except for UMAs and logging.
  const bool has_sync_setup_completed_;

  base::WeakPtrFactory<Core> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

SyncBackendHost::SyncBackendHost(
    const std::string& name,
    Profile* profile,
    const base::WeakPtr<SyncPrefs>& sync_prefs)
    : frontend_loop_(base::MessageLoop::current()),
      profile_(profile),
      name_(name),
      initialized_(false),
      sync_prefs_(sync_prefs),
      frontend_(NULL),
      cached_passphrase_type_(syncer::IMPLICIT_PASSPHRASE),
      invalidator_(
          invalidation::InvalidationServiceFactory::GetForProfile(profile)),
      invalidation_handler_registered_(false),
      weak_ptr_factory_(this) {
  core_ = new Core(name_, profile_->GetPath().Append(kSyncDataFolderName),
                   sync_prefs_->HasSyncSetupCompleted(),
                   weak_ptr_factory_.GetWeakPtr());
}

SyncBackendHost::SyncBackendHost(Profile* profile)
    : frontend_loop_(base::MessageLoop::current()),
      profile_(profile),
      name_("Unknown"),
      initialized_(false),
      frontend_(NULL),
      cached_passphrase_type_(syncer::IMPLICIT_PASSPHRASE),
      invalidation_handler_registered_(false),
      weak_ptr_factory_(this) {
}

SyncBackendHost::~SyncBackendHost() {
  DCHECK(!core_.get() && !frontend_) << "Must call Shutdown before destructor.";
  DCHECK(!registrar_.get());
}

void SyncBackendHost::Initialize(
    SyncFrontend* frontend,
    scoped_ptr<base::Thread> sync_thread,
    const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
    const GURL& sync_service_url,
    const SyncCredentials& credentials,
    bool delete_sync_data_folder,
    scoped_ptr<syncer::SyncManagerFactory> sync_manager_factory,
    scoped_ptr<syncer::UnrecoverableErrorHandler> unrecoverable_error_handler,
    syncer::ReportUnrecoverableErrorFunction
        report_unrecoverable_error_function) {
  registrar_.reset(new SyncBackendRegistrar(name_,
                                            profile_,
                                            sync_thread.Pass()));
  CHECK(registrar_->sync_thread());

  frontend_ = frontend;
  DCHECK(frontend);

  syncer::ModelSafeRoutingInfo routing_info;
  std::vector<syncer::ModelSafeWorker*> workers;
  registrar_->GetModelSafeRoutingInfo(&routing_info);
  registrar_->GetWorkers(&workers);

  InternalComponentsFactory::Switches factory_switches = {
      InternalComponentsFactory::ENCRYPTION_KEYSTORE,
      InternalComponentsFactory::BACKOFF_NORMAL
  };

  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kSyncShortInitialRetryOverride)) {
    factory_switches.backoff_override =
        InternalComponentsFactoryImpl::BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE;
  }
  if (cl->HasSwitch(switches::kSyncEnableGetUpdateAvoidance)) {
    factory_switches.pre_commit_updates_policy =
        InternalComponentsFactoryImpl::FORCE_ENABLE_PRE_COMMIT_UPDATE_AVOIDANCE;
  }

  scoped_ptr<DoInitializeOptions> init_opts(new DoInitializeOptions(
      registrar_->sync_thread()->message_loop(),
      registrar_.get(),
      routing_info,
      workers,
      extensions_activity_monitor_.GetExtensionsActivity(),
      event_handler,
      sync_service_url,
      scoped_ptr<syncer::HttpPostProviderFactory>(
          new syncer::HttpBridgeFactory(
              make_scoped_refptr(profile_->GetRequestContext()),
              NetworkTimeTracker::BuildNotifierUpdateCallback(),
              core_->GetRequestContextCancelationSignal())),
      credentials,
      invalidator_->GetInvalidatorClientId(),
      sync_manager_factory.Pass(),
      delete_sync_data_folder,
      sync_prefs_->GetEncryptionBootstrapToken(),
      sync_prefs_->GetKeystoreEncryptionBootstrapToken(),
      scoped_ptr<InternalComponentsFactory>(
          new InternalComponentsFactoryImpl(factory_switches)).Pass(),
      unrecoverable_error_handler.Pass(),
      report_unrecoverable_error_function));
  InitCore(init_opts.Pass());
}

void SyncBackendHost::UpdateCredentials(const SyncCredentials& credentials) {
  DCHECK(registrar_->sync_thread()->IsRunning());
  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoUpdateCredentials,
                 core_.get(),
                 credentials));
}

void SyncBackendHost::StartSyncingWithServer() {
  SDVLOG(1) << "SyncBackendHost::StartSyncingWithServer called.";

  syncer::ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);

  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoStartSyncing,
                 core_.get(), routing_info));
}

void SyncBackendHost::SetEncryptionPassphrase(const std::string& passphrase,
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
      base::Bind(&SyncBackendHost::Core::DoSetEncryptionPassphrase,
                 core_.get(),
                 passphrase, is_explicit));
}

bool SyncBackendHost::SetDecryptionPassphrase(const std::string& passphrase) {
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
      base::Bind(&SyncBackendHost::Core::DoSetDecryptionPassphrase,
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

void SyncBackendHost::StopSyncingForShutdown() {
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

  // Immediately stop sending messages to the frontend.
  frontend_ = NULL;

  // Stop listening for and forwarding locally-triggered sync refresh requests.
  notification_registrar_.RemoveAll();

  DCHECK(registrar_->sync_thread()->IsRunning());

  registrar_->RequestWorkerStopOnUIThread();

  core_->ShutdownOnUIThread();
}

scoped_ptr<base::Thread> SyncBackendHost::Shutdown(ShutdownOption option) {
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
      base::Bind(&SyncBackendHost::Core::DoShutdown,
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

void SyncBackendHost::UnregisterInvalidationIds() {
  if (invalidation_handler_registered_) {
    invalidator_->UpdateRegisteredInvalidationIds(
        this,
        syncer::ObjectIdSet());
  }
}

void SyncBackendHost::ConfigureDataTypes(
    syncer::ConfigureReason reason,
    const DataTypeConfigStateMap& config_state_map,
    const base::Callback<void(syncer::ModelTypeSet,
                              syncer::ModelTypeSet)>& ready_task,
    const base::Callback<void()>& retry_callback) {
  // Only one configure is allowed at a time.  This is guaranteed by our
  // callers.  The SyncBackendHost requests one configure as the backend is
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

  syncer::ModelTypeSet previous_types = registrar_->GetLastConfiguredTypes();

  syncer::ModelTypeSet disabled_types =
      GetDataTypesInState(DISABLED, config_state_map);
  syncer::ModelTypeSet fatal_types =
      GetDataTypesInState(FATAL, config_state_map);
  syncer::ModelTypeSet crypto_types =
      GetDataTypesInState(CRYPTO, config_state_map);
  disabled_types.PutAll(fatal_types);
  disabled_types.PutAll(crypto_types);
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
      syncer::Difference(previous_types, current_types);
  syncer::ModelTypeSet inactive_types =
      GetDataTypesInState(CONFIGURE_INACTIVE, config_state_map);
  types_to_purge.RemoveAll(inactive_types);

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

void SyncBackendHost::EnableEncryptEverything() {
  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
     base::Bind(&SyncBackendHost::Core::DoEnableEncryptEverything,
                core_.get()));
}

void SyncBackendHost::ActivateDataType(
    syncer::ModelType type, syncer::ModelSafeGroup group,
    ChangeProcessor* change_processor) {
  registrar_->ActivateDataType(type, group, change_processor, GetUserShare());
}

void SyncBackendHost::DeactivateDataType(syncer::ModelType type) {
  registrar_->DeactivateDataType(type);
}

syncer::UserShare* SyncBackendHost::GetUserShare() const {
  return core_->sync_manager()->GetUserShare();
}

SyncBackendHost::Status SyncBackendHost::GetDetailedStatus() {
  DCHECK(initialized());
  return core_->sync_manager()->GetDetailedStatus();
}

SyncSessionSnapshot SyncBackendHost::GetLastSessionSnapshot() const {
  return last_snapshot_;
}

bool SyncBackendHost::HasUnsyncedItems() const {
  DCHECK(initialized());
  return core_->sync_manager()->HasUnsyncedItems();
}

bool SyncBackendHost::IsNigoriEnabled() const {
  return registrar_.get() && registrar_->IsNigoriEnabled();
}

syncer::PassphraseType SyncBackendHost::GetPassphraseType() const {
  return cached_passphrase_type_;
}

base::Time SyncBackendHost::GetExplicitPassphraseTime() const {
  return cached_explicit_passphrase_time_;
}

bool SyncBackendHost::IsCryptographerReady(
    const syncer::BaseTransaction* trans) const {
  return initialized() && trans->GetCryptographer()->is_ready();
}

void SyncBackendHost::GetModelSafeRoutingInfo(
    syncer::ModelSafeRoutingInfo* out) const {
  if (initialized()) {
    CHECK(registrar_.get());
    registrar_->GetModelSafeRoutingInfo(out);
  } else {
    NOTREACHED();
  }
}

SyncedDeviceTracker* SyncBackendHost::GetSyncedDeviceTracker() const {
  if (!initialized())
    return NULL;
  return core_->synced_device_tracker();
}

void SyncBackendHost::InitCore(scoped_ptr<DoInitializeOptions> options) {
  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoInitialize,
                 core_.get(), base::Passed(&options)));
}

void SyncBackendHost::RequestConfigureSyncer(
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
       base::Bind(&SyncBackendHost::Core::DoConfigureSyncer,
                  core_.get(),
                  reason,
                  config_types,
                  routing_info,
                  ready_task,
                  retry_callback));
}

void SyncBackendHost::FinishConfigureDataTypesOnFrontendLoop(
    const syncer::ModelTypeSet enabled_types,
    const syncer::ModelTypeSet succeeded_configuration_types,
    const syncer::ModelTypeSet failed_configuration_types,
    const base::Callback<void(syncer::ModelTypeSet,
                              syncer::ModelTypeSet)>& ready_task) {
  if (!frontend_)
    return;

  invalidator_->UpdateRegisteredInvalidationIds(
      this,
      ModelTypeSetToObjectIdSet(enabled_types));

  if (!ready_task.is_null())
    ready_task.Run(succeeded_configuration_types, failed_configuration_types);
}

void SyncBackendHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(type, chrome::NOTIFICATION_SYNC_REFRESH_LOCAL);

  content::Details<const syncer::ModelTypeSet> state_details(details);
  const syncer::ModelTypeSet& types = *(state_details.ptr());
  registrar_->sync_thread()->message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoRefreshTypes, core_.get(), types));
}

SyncBackendHost::DoInitializeOptions::DoInitializeOptions(
    base::MessageLoop* sync_loop,
    SyncBackendRegistrar* registrar,
    const syncer::ModelSafeRoutingInfo& routing_info,
    const std::vector<syncer::ModelSafeWorker*>& workers,
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
    scoped_ptr<InternalComponentsFactory> internal_components_factory,
    scoped_ptr<syncer::UnrecoverableErrorHandler> unrecoverable_error_handler,
    syncer::ReportUnrecoverableErrorFunction
        report_unrecoverable_error_function)
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
      report_unrecoverable_error_function(
          report_unrecoverable_error_function) {
}

SyncBackendHost::DoInitializeOptions::~DoInitializeOptions() {}

SyncBackendHost::Core::Core(const std::string& name,
                            const base::FilePath& sync_data_folder_path,
                            bool has_sync_setup_completed,
                            const base::WeakPtr<SyncBackendHost>& backend)
    : name_(name),
      sync_data_folder_path_(sync_data_folder_path),
      host_(backend),
      sync_loop_(NULL),
      registrar_(NULL),
      has_sync_setup_completed_(has_sync_setup_completed),
      weak_ptr_factory_(this) {
  DCHECK(backend.get());
}

SyncBackendHost::Core::~Core() {
  DCHECK(!sync_manager_.get());
}

void SyncBackendHost::Core::OnSyncCycleCompleted(
    const SyncSessionSnapshot& snapshot) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);

  host_.Call(
      FROM_HERE,
      &SyncBackendHost::HandleSyncCycleCompletedOnFrontendLoop,
      snapshot);
}

void SyncBackendHost::Core::DoRefreshTypes(syncer::ModelTypeSet types) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->RefreshTypes(types);
}

void SyncBackendHost::Core::OnControlTypesDownloadRetry() {
  host_.Call(FROM_HERE,
             &SyncBackendHost::HandleControlTypesDownloadRetry);
}

void SyncBackendHost::Core::OnInitializationComplete(
    const syncer::WeakHandle<syncer::JsBackend>& js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    bool success,
    const syncer::ModelTypeSet restored_types) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);

  if (!success) {
    DoDestroySyncManager();
    host_.Call(FROM_HERE,
               &SyncBackendHost::HandleInitializationFailureOnFrontendLoop);
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
                       base::Bind(&Core::StartSavingChanges,
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
      base::Bind(&SyncBackendHost::Core::DoInitialProcessControlTypes,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&SyncBackendHost::Core::OnControlTypesDownloadRetry,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SyncBackendHost::Core::OnConnectionStatusChange(
    syncer::ConnectionStatus status) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::HandleConnectionStatusChangeOnFrontendLoop, status);
}

void SyncBackendHost::Core::OnPassphraseRequired(
    syncer::PassphraseRequiredReason reason,
    const sync_pb::EncryptedData& pending_keys) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::NotifyPassphraseRequired, reason, pending_keys);
}

void SyncBackendHost::Core::OnPassphraseAccepted() {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::NotifyPassphraseAccepted);
}

void SyncBackendHost::Core::OnBootstrapTokenUpdated(
    const std::string& bootstrap_token,
    syncer::BootstrapTokenType type) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(FROM_HERE,
             &SyncBackendHost::PersistEncryptionBootstrapToken,
             bootstrap_token,
             type);
}

void SyncBackendHost::Core::OnStopSyncingPermanently() {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::HandleStopSyncingPermanentlyOnFrontendLoop);
}

void SyncBackendHost::Core::OnEncryptedTypesChanged(
    syncer::ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  // NOTE: We're in a transaction.
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::NotifyEncryptedTypesChanged,
      encrypted_types, encrypt_everything);
}

void SyncBackendHost::Core::OnEncryptionComplete() {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  // NOTE: We're in a transaction.
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::NotifyEncryptionComplete);
}

void SyncBackendHost::Core::OnCryptographerStateChanged(
    syncer::Cryptographer* cryptographer) {
  // Do nothing.
}

void SyncBackendHost::Core::OnPassphraseTypeChanged(
    syncer::PassphraseType type, base::Time passphrase_time) {
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::HandlePassphraseTypeChangedOnFrontendLoop,
      type, passphrase_time);
}

void SyncBackendHost::Core::OnActionableError(
    const syncer::SyncProtocolError& sync_error) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::HandleActionableErrorEventOnFrontendLoop,
      sync_error);
}

void SyncBackendHost::Core::DoOnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->OnInvalidatorStateChange(state);
}

void SyncBackendHost::Core::DoOnIncomingInvalidation(
    syncer::ObjectIdInvalidationMap invalidation_map) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->OnIncomingInvalidation(invalidation_map);
}

void SyncBackendHost::Core::DoInitialize(
    scoped_ptr<DoInitializeOptions> options) {
  DCHECK(!sync_loop_);
  sync_loop_ = options->sync_loop;
  DCHECK(sync_loop_);

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
  if (!file_util::CreateDirectory(sync_data_folder_path_)) {
    DLOG(FATAL) << "Sync Data directory creation failed.";
  }

  DCHECK(!registrar_);
  registrar_ = options->registrar;
  DCHECK(registrar_);

  sync_manager_ = options->sync_manager_factory->CreateSyncManager(name_);
  sync_manager_->AddObserver(this);
  sync_manager_->Init(sync_data_folder_path_,
                      options->event_handler,
                      options->service_url.host() + options->service_url.path(),
                      options->service_url.EffectiveIntPort(),
                      options->service_url.SchemeIsSecure(),
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

  // |sync_manager_| may end up being NULL here in tests (in
  // synchronous initialization mode).
  //
  // TODO(akalin): Fix this behavior (see http://crbug.com/140354).
  if (sync_manager_) {
    // Now check the command line to see if we need to simulate an
    // unrecoverable error for testing purpose. Note the error is thrown
    // only if the initialization succeeded. Also it makes sense to use this
    // flag only when restarting the browser with an account already setup. If
    // you use this before setting up the setup would not succeed as an error
    // would be encountered.
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSyncThrowUnrecoverableError)) {
      sync_manager_->ThrowUnrecoverableError();
    }
  }
}

void SyncBackendHost::Core::DoUpdateCredentials(
    const SyncCredentials& credentials) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  // UpdateCredentials can be called during backend initialization, possibly
  // when backend initialization has failed but hasn't notified the UI thread
  // yet. In that case, the sync manager may have been destroyed on the sync
  // thread before this task was executed, so we do nothing.
  if (sync_manager_) {
    sync_manager_->UpdateCredentials(credentials);
  }
}

void SyncBackendHost::Core::DoStartSyncing(
    const syncer::ModelSafeRoutingInfo& routing_info) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->StartSyncingNormally(routing_info);
}

void SyncBackendHost::Core::DoSetEncryptionPassphrase(
    const std::string& passphrase,
    bool is_explicit) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->GetEncryptionHandler()->SetEncryptionPassphrase(
      passphrase, is_explicit);
}

void SyncBackendHost::Core::DoInitialProcessControlTypes() {
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
        &SyncBackendHost::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  if (!sync_manager_->InitialSyncEndedTypes().HasAll(syncer::ControlTypes())) {
    LOG(ERROR) << "Failed to download control types";
    host_.Call(
        FROM_HERE,
        &SyncBackendHost::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  // Initialize device info. This is asynchronous on some platforms, so we
  // provide a callback for when it finishes.
  synced_device_tracker_.reset(
      new SyncedDeviceTracker(sync_manager_->GetUserShare(),
                              sync_manager_->cache_guid()));
  synced_device_tracker_->InitLocalDeviceInfo(
      base::Bind(&SyncBackendHost::Core::DoFinishInitialProcessControlTypes,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SyncBackendHost::Core::DoFinishInitialProcessControlTypes() {
  registrar_->ActivateDataType(syncer::DEVICE_INFO,
                               syncer::GROUP_PASSIVE,
                               synced_device_tracker_.get(),
                               sync_manager_->GetUserShare());

  host_.Call(
      FROM_HERE,
      &SyncBackendHost::HandleInitializationSuccessOnFrontendLoop,
      js_backend_,
      debug_info_listener_);

  js_backend_.Reset();
  debug_info_listener_.Reset();
}

void SyncBackendHost::Core::DoSetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->GetEncryptionHandler()->SetDecryptionPassphrase(
      passphrase);
}

void SyncBackendHost::Core::DoEnableEncryptEverything() {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->GetEncryptionHandler()->EnableEncryptEverything();
}

void SyncBackendHost::Core::ShutdownOnUIThread() {
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

void SyncBackendHost::Core::DoShutdown(bool sync_disabled) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);

  // It's safe to do this even if the type was never activated.
  registrar_->DeactivateDataType(syncer::DEVICE_INFO);
  synced_device_tracker_.reset();

  DoDestroySyncManager();

  registrar_ = NULL;

  if (sync_disabled)
    DeleteSyncDataFolder();

  host_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SyncBackendHost::Core::DoDestroySyncManager() {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  if (sync_manager_) {
    save_changes_timer_.reset();
    sync_manager_->RemoveObserver(this);
    sync_manager_->ShutdownOnSyncThread();
    sync_manager_.reset();
  }
}

void SyncBackendHost::Core::DoConfigureSyncer(
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
      base::Bind(&SyncBackendHost::Core::DoFinishConfigureDataTypes,
                 weak_ptr_factory_.GetWeakPtr(),
                 config_types.to_download,
                 ready_task),
      base::Bind(&SyncBackendHost::Core::DoRetryConfiguration,
                 weak_ptr_factory_.GetWeakPtr(),
                 retry_callback));
}

void SyncBackendHost::Core::DoFinishConfigureDataTypes(
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
             &SyncBackendHost::FinishConfigureDataTypesOnFrontendLoop,
             enabled_types,
             succeeded_configuration_types,
             failed_configuration_types,
             ready_task);
}

void SyncBackendHost::Core::DoRetryConfiguration(
    const base::Closure& retry_callback) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  host_.Call(FROM_HERE,
             &SyncBackendHost::RetryConfigurationOnFrontendLoop,
             retry_callback);
}

void SyncBackendHost::Core::DeleteSyncDataFolder() {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  if (base::DirectoryExists(sync_data_folder_path_)) {
    if (!base::DeleteFile(sync_data_folder_path_, true))
      SLOG(DFATAL) << "Could not delete the Sync Data folder.";
  }
}

void SyncBackendHost::Core::StartSavingChanges() {
  // We may already be shut down.
  if (!sync_loop_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  DCHECK(!save_changes_timer_.get());
  save_changes_timer_.reset(new base::RepeatingTimer<Core>());
  save_changes_timer_->Start(FROM_HERE,
      base::TimeDelta::FromSeconds(kSaveChangesIntervalSeconds),
      this, &Core::SaveChanges);
}

void SyncBackendHost::Core::SaveChanges() {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  sync_manager_->SaveChanges();
}

void SyncBackendHost::AddExperimentalTypes() {
  CHECK(initialized());
  syncer::Experiments experiments;
  if (core_->sync_manager()->ReceivedExperiment(&experiments))
    frontend_->OnExperimentsChanged(experiments);
}

void SyncBackendHost::HandleControlTypesDownloadRetry() {
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  if (!frontend_)
    return;

  frontend_->OnSyncConfigureRetry();
}

void SyncBackendHost::HandleInitializationSuccessOnFrontendLoop(
    const syncer::WeakHandle<syncer::JsBackend> js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>
        debug_info_listener) {
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  if (!frontend_)
    return;

  initialized_ = true;

  invalidator_->RegisterInvalidationHandler(this);
  invalidation_handler_registered_ = true;

  // Fake a state change to initialize the SyncManager's cached invalidator
  // state.
  OnInvalidatorStateChange(invalidator_->GetInvalidatorState());

  // Start forwarding refresh requests to the SyncManager
  notification_registrar_.Add(this, chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                              content::Source<Profile>(profile_));

  // Now that we've downloaded the control types, we can see if there are any
  // experimental types to enable. This should be done before we inform
  // the frontend to ensure they're visible in the customize screen.
  AddExperimentalTypes();
  frontend_->OnBackendInitialized(js_backend,
                                  debug_info_listener,
                                  true);
}

void SyncBackendHost::HandleInitializationFailureOnFrontendLoop() {
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  if (!frontend_)
    return;

  frontend_->OnBackendInitialized(
      syncer::WeakHandle<syncer::JsBackend>(),
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(),
      false);
}

void SyncBackendHost::HandleSyncCycleCompletedOnFrontendLoop(
    const SyncSessionSnapshot& snapshot) {
  if (!frontend_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

  last_snapshot_ = snapshot;

  SDVLOG(1) << "Got snapshot " << snapshot.ToString();

  const syncer::ModelTypeSet to_migrate =
      snapshot.model_neutral_state().types_needing_local_migration;
  if (!to_migrate.Empty())
    frontend_->OnMigrationNeededForTypes(to_migrate);

  // Process any changes to the datatypes we're syncing.
  // TODO(sync): add support for removing types.
  if (initialized())
    AddExperimentalTypes();

  if (initialized())
    frontend_->OnSyncCycleCompleted();
}

void SyncBackendHost::RetryConfigurationOnFrontendLoop(
    const base::Closure& retry_callback) {
  SDVLOG(1) << "Failed to complete configuration, informing of retry.";
  retry_callback.Run();
}

void SyncBackendHost::PersistEncryptionBootstrapToken(
    const std::string& token,
    syncer::BootstrapTokenType token_type) {
  CHECK(sync_prefs_.get());
  DCHECK(!token.empty());
  if (token_type == syncer::PASSPHRASE_BOOTSTRAP_TOKEN)
    sync_prefs_->SetEncryptionBootstrapToken(token);
  else
    sync_prefs_->SetKeystoreEncryptionBootstrapToken(token);
}

void SyncBackendHost::HandleActionableErrorEventOnFrontendLoop(
    const syncer::SyncProtocolError& sync_error) {
  if (!frontend_)
    return;
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  frontend_->OnActionableError(sync_error);
}

void SyncBackendHost::OnInvalidatorStateChange(syncer::InvalidatorState state) {
  registrar_->sync_thread()->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoOnInvalidatorStateChange,
                 core_.get(),
                 state));
}

void SyncBackendHost::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  // TODO(rlarocque): Acknowledge these invalidations only after the syncer has
  // acted on them and saved the results to disk.
  syncer::ObjectIdSet ids = invalidation_map.GetObjectIds();
  for (syncer::ObjectIdSet::const_iterator it = ids.begin();
       it != ids.end(); ++it) {
    const syncer::AckHandle& handle =
        invalidation_map.ForObject(*it).back().ack_handle();
    invalidator_->AcknowledgeInvalidation(*it, handle);
  }

  registrar_->sync_thread()->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoOnIncomingInvalidation,
                 core_.get(),
                 invalidation_map));
}

bool SyncBackendHost::CheckPassphraseAgainstCachedPendingKeys(
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

void SyncBackendHost::NotifyPassphraseRequired(
    syncer::PassphraseRequiredReason reason,
    sync_pb::EncryptedData pending_keys) {
  if (!frontend_)
    return;

  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

  // Update our cache of the cryptographer's pending keys.
  cached_pending_keys_ = pending_keys;

  frontend_->OnPassphraseRequired(reason, pending_keys);
}

void SyncBackendHost::NotifyPassphraseAccepted() {
  if (!frontend_)
    return;

  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

  // Clear our cache of the cryptographer's pending keys.
  cached_pending_keys_.clear_blob();
  frontend_->OnPassphraseAccepted();
}

void SyncBackendHost::NotifyEncryptedTypesChanged(
    syncer::ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  if (!frontend_)
    return;

  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  frontend_->OnEncryptedTypesChanged(
      encrypted_types, encrypt_everything);
}

void SyncBackendHost::NotifyEncryptionComplete() {
  if (!frontend_)
    return;

  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  frontend_->OnEncryptionComplete();
}

void SyncBackendHost::HandlePassphraseTypeChangedOnFrontendLoop(
    syncer::PassphraseType type,
    base::Time explicit_passphrase_time) {
  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);
  DVLOG(1) << "Passphrase type changed to "
           << syncer::PassphraseTypeToString(type);
  cached_passphrase_type_ = type;
  cached_explicit_passphrase_time_ = explicit_passphrase_time;
}

void SyncBackendHost::HandleStopSyncingPermanentlyOnFrontendLoop() {
  if (!frontend_)
    return;
  frontend_->OnStopSyncingPermanently();
}

void SyncBackendHost::HandleConnectionStatusChangeOnFrontendLoop(
    syncer::ConnectionStatus status) {
  if (!frontend_)
    return;

  DCHECK_EQ(base::MessageLoop::current(), frontend_loop_);

  DVLOG(1) << "Connection status changed: "
           << syncer::ConnectionStatusToString(status);
  frontend_->OnConnectionStatusChange(status);
}

base::MessageLoop* SyncBackendHost::GetSyncLoopForTesting() {
  return registrar_->sync_thread()->message_loop();
}

#undef SDVLOG

#undef SLOG

}  // namespace browser_sync
