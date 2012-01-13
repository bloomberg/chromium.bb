// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/sync/glue/sync_backend_host.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/http_bridge.h"
#include "chrome/browser/sync/glue/sync_backend_registrar.h"
#include "chrome/browser/sync/internal_api/base_transaction.h"
#include "chrome/browser/sync/internal_api/read_transaction.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/sync_prefs.h"
// TODO(tim): Remove this! We should have a syncapi pass-thru instead.
#include "chrome/browser/sync/syncable/directory_manager.h"  // Cryptographer.
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_client.h"

static const int kSaveChangesIntervalSeconds = 10;
static const FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

typedef TokenService::TokenAvailableDetails TokenAvailableDetails;

typedef GoogleServiceAuthError AuthError;

namespace browser_sync {

using content::BrowserThread;
using sessions::SyncSessionSnapshot;
using sync_api::SyncCredentials;

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncers involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

class SyncBackendHost::Core
    : public base::RefCountedThreadSafe<SyncBackendHost::Core>,
      public sync_api::SyncManager::Observer {
 public:
  Core(const std::string& name,
       const FilePath& sync_data_folder_path,
       const base::WeakPtr<SyncBackendHost>& backend);

  // SyncManager::Observer implementation.  The Core just acts like an air
  // traffic controller here, forwarding incoming messages to appropriate
  // landing threads.
  virtual void OnSyncCycleCompleted(
      const sessions::SyncSessionSnapshot* snapshot) OVERRIDE;
  virtual void OnInitializationComplete(
      const WeakHandle<JsBackend>& js_backend,
      bool success) OVERRIDE;
  virtual void OnAuthError(
      const GoogleServiceAuthError& auth_error) OVERRIDE;
  virtual void OnPassphraseRequired(
      sync_api::PassphraseRequiredReason reason) OVERRIDE;
  virtual void OnPassphraseAccepted(
      const std::string& bootstrap_token) OVERRIDE;
  virtual void OnStopSyncingPermanently() OVERRIDE;
  virtual void OnUpdatedToken(const std::string& token) OVERRIDE;
  virtual void OnClearServerDataFailed() OVERRIDE;
  virtual void OnClearServerDataSucceeded() OVERRIDE;
  virtual void OnEncryptedTypesChanged(
      syncable::ModelTypeSet encrypted_types,
      bool encrypt_everything) OVERRIDE;
  virtual void OnEncryptionComplete() OVERRIDE;
  virtual void OnActionableError(
      const browser_sync::SyncProtocolError& sync_error) OVERRIDE;

  // Note:
  //
  // The Do* methods are the various entry points from our
  // SyncBackendHost.  They are all called on the sync thread to
  // actually perform synchronous (and potentially blocking) syncapi
  // operations.
  //
  // Called to perform initialization of the syncapi on behalf of
  // SyncBackendHost::Initialize.
  void DoInitialize(const DoInitializeOptions& options);

  // Called to check server reachability after initialization is
  // fully completed.
  void DoCheckServerReachable();

  // Called to perform credential update on behalf of
  // SyncBackendHost::UpdateCredentials
  void DoUpdateCredentials(const sync_api::SyncCredentials& credentials);

  // Called when the user disables or enables a sync type.
  void DoUpdateEnabledTypes();

  // Called to tell the syncapi to start syncing (generally after
  // initialization and authentication).
  void DoStartSyncing();

  // Called to clear server data.
  void DoRequestClearServerData();

  // Called to cleanup disabled types.
  void DoRequestCleanupDisabledTypes();

  // Called to set the passphrase on behalf of
  // SyncBackendHost::SupplyPassphrase.
  void DoSetPassphrase(const std::string& passphrase, bool is_explicit);

  // Called to turn on encryption of all sync data as well as
  // reencrypt everything.
  void DoEnableEncryptEverything();

  // Called to refresh encryption with the most recent passphrase
  // and set of encrypted types. Also adds device information to the nigori
  // node. |done_callback| is called on the sync thread.
  void DoRefreshNigori(const base::Closure& done_callback);

  // The shutdown order is a bit complicated:
  // 1) From |sync_thread_|, invoke the syncapi Shutdown call to do
  //    a final SaveChanges, and close sqlite handles.
  // 2) Then, from |frontend_loop_|, halt the sync_thread_ (which is
  //    a blocking call). This causes syncapi thread-exit handlers
  //    to run and make use of cached pointers to various components
  //    owned implicitly by us.
  // 3) Destroy this Core. That will delete syncapi components in a
  //    safe order because the thread that was using them has exited
  //    (in step 2).
  void DoStopSyncManagerForShutdown(const base::Closure& closure);
  void DoShutdown(bool stopping_sync);

  virtual void DoRequestConfig(
      syncable::ModelTypeSet types_to_config,
      sync_api::ConfigureReason reason);

  // Start the configuration mode.  |callback| is called on the sync
  // thread.
  virtual void DoStartConfiguration(const base::Closure& callback);

  // Set the base request context to use when making HTTP calls.
  // This method will add a reference to the context to persist it
  // on the IO thread. Must be removed from IO thread.

  sync_api::SyncManager* sync_manager() { return sync_manager_.get(); }

  // Delete the sync data folder to cleanup backend data.  Happens the first
  // time sync is enabled for a user (to prevent accidentally reusing old
  // sync databases), as well as shutdown when you're no longer syncing.
  void DeleteSyncDataFolder();

  // A callback from the SyncerThread when it is safe to continue config.
  void FinishConfigureDataTypes();

 private:
  friend class base::RefCountedThreadSafe<SyncBackendHost::Core>;
  friend class SyncBackendHostForProfileSyncTest;

  virtual ~Core();

  // Invoked when initialization of syncapi is complete and we can start
  // our timer.
  // This must be called from the thread on which SaveChanges is intended to
  // be run on; the host's |sync_thread_|.
  void StartSavingChanges();

  // Invoked periodically to tell the syncapi to persist its state
  // by writing to disk.
  // This is called from the thread we were created on (which is the
  // SyncBackendHost |sync_thread_|), using a repeating timer that is kicked
  // off as soon as the SyncManager tells us it completed
  // initialization.
  void SaveChanges();

  // Name used for debugging.
  const std::string name_;

  // Path of the folder that stores the sync data files.
  const FilePath sync_data_folder_path_;

  // Our parent SyncBackendHost.
  WeakHandle<SyncBackendHost> host_;

  // The loop where all the sync backend operations happen.
  // Non-NULL only between calls to DoInitialize() and DoShutdown().
  MessageLoop* sync_loop_;

  // Our parent's registrar (not owned).  Non-NULL only between
  // calls to DoInitialize() and DoShutdown().
  SyncBackendRegistrar* registrar_;

  // The timer used to periodically call SaveChanges.
  base::RepeatingTimer<Core> save_changes_timer_;

  // The top-level syncapi entry point.  Lives on the sync thread.
  scoped_ptr<sync_api::SyncManager> sync_manager_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

SyncBackendHost::SyncBackendHost(const std::string& name,
                                 Profile* profile,
                                 const base::WeakPtr<SyncPrefs>& sync_prefs)
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      sync_thread_("Chrome_SyncThread"),
      frontend_loop_(MessageLoop::current()),
      profile_(profile),
      name_(name),
      core_(new Core(name, profile_->GetPath().Append(kSyncDataFolderName),
                     weak_ptr_factory_.GetWeakPtr())),
      initialization_state_(NOT_ATTEMPTED),
      sync_prefs_(sync_prefs),
      sync_notifier_factory_(
          content::GetUserAgent(GURL()),
          profile_->GetRequestContext(),
          sync_prefs,
          *CommandLine::ForCurrentProcess()),
      frontend_(NULL),
      last_auth_error_(AuthError::None()) {
}

SyncBackendHost::SyncBackendHost()
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      sync_thread_("Chrome_SyncThread"),
      frontend_loop_(MessageLoop::current()),
      profile_(NULL),
      name_("Unknown"),
      initialization_state_(NOT_ATTEMPTED),
      sync_notifier_factory_(
          content::GetUserAgent(GURL()),
          NULL,
          base::WeakPtr<sync_notifier::InvalidationVersionTracker>(),
          *CommandLine::ForCurrentProcess()),
      frontend_(NULL),
      last_auth_error_(AuthError::None()) {
}

SyncBackendHost::~SyncBackendHost() {
  DCHECK(!core_ && !frontend_) << "Must call Shutdown before destructor.";
  DCHECK(!registrar_.get());
}

namespace {

sync_api::HttpPostProviderFactory* MakeHttpBridgeFactory(
    const scoped_refptr<net::URLRequestContextGetter>& getter) {
  return new HttpBridgeFactory(getter);
}

}  // namespace

void SyncBackendHost::Initialize(
    SyncFrontend* frontend,
    const WeakHandle<JsEventHandler>& event_handler,
    const GURL& sync_service_url,
    syncable::ModelTypeSet initial_types,
    const SyncCredentials& credentials,
    bool delete_sync_data_folder,
    UnrecoverableErrorHandler* unrecoverable_error_handler) {
  if (!sync_thread_.Start())
    return;

  frontend_ = frontend;
  DCHECK(frontend);

  syncable::ModelTypeSet initial_types_with_nigori(initial_types);
  CHECK(sync_prefs_.get());
  if (sync_prefs_->HasSyncSetupCompleted()) {
    initial_types_with_nigori.Put(syncable::NIGORI);
  }

  registrar_.reset(new SyncBackendRegistrar(initial_types_with_nigori,
                                            name_,
                                            profile_,
                                            sync_thread_.message_loop()));
  initialization_state_ = CREATING_SYNC_MANAGER;
  InitCore(DoInitializeOptions(
      sync_thread_.message_loop(),
      registrar_.get(),
      event_handler,
      sync_service_url,
      base::Bind(&MakeHttpBridgeFactory,
                 make_scoped_refptr(profile_->GetRequestContext())),
      credentials,
      &sync_notifier_factory_,
      delete_sync_data_folder,
      sync_prefs_->GetEncryptionBootstrapToken(),
      false,
      unrecoverable_error_handler));
}

void SyncBackendHost::UpdateCredentials(const SyncCredentials& credentials) {
  sync_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoUpdateCredentials, core_.get(),
                 credentials));
}

void SyncBackendHost::StartSyncingWithServer() {
  SDVLOG(1) << "SyncBackendHost::StartSyncingWithServer called.";
  sync_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoStartSyncing, core_.get()));
}

void SyncBackendHost::SetPassphrase(const std::string& passphrase,
                                    bool is_explicit) {
  if (!IsNigoriEnabled()) {
    SLOG(WARNING) << "Silently dropping SetPassphrase request.";
    return;
  }

  // This should only be called by the frontend.
  DCHECK_EQ(MessageLoop::current(), frontend_loop_);

  // If encryption is enabled and we've got a SetPassphrase
  sync_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoSetPassphrase, core_.get(),
                 passphrase, is_explicit));
}

void SyncBackendHost::StopSyncManagerForShutdown(
    const base::Closure& closure) {
  DCHECK_GT(initialization_state_, NOT_ATTEMPTED);
  if (initialization_state_ == CREATING_SYNC_MANAGER) {
    // We post here to implicitly wait for the SyncManager to be created,
    // if needed.  We have to wait, since we need to shutdown immediately,
    // and we need to tell the SyncManager so it can abort any activity
    // (net I/O, data application).
    DCHECK(sync_thread_.IsRunning());
    sync_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(
            &SyncBackendHost::Core::DoStopSyncManagerForShutdown,
            core_.get(),
            closure));
  } else {
    core_->DoStopSyncManagerForShutdown(closure);
  }
}

void SyncBackendHost::StopSyncingForShutdown() {
  DCHECK_EQ(MessageLoop::current(), frontend_loop_);
  // Thread shutdown should occur in the following order:
  // - Sync Thread
  // - UI Thread (stops some time after we return from this call).
  //
  // In order to acheive this, we first shutdown components from the UI thread
  // and send signals to abort components that may be busy on the sync thread.
  // The callback (OnSyncerShutdownComplete) will happen on the sync thread,
  // after which we'll shutdown components on the sync thread, and then be
  // able to stop the sync loop.
  if (sync_thread_.IsRunning()) {
    StopSyncManagerForShutdown(
        base::Bind(&SyncBackendRegistrar::OnSyncerShutdownComplete,
                   base::Unretained(registrar_.get())));

    // Before joining the sync_thread_, we wait for the UIModelWorker to
    // give us the green light that it is not depending on the frontend_loop_
    // to process any more tasks. Stop() blocks until this termination
    // condition is true.
    if (registrar_.get())
      registrar_->StopOnUIThread();
  } else {
    // If the sync thread isn't running, then the syncer is effectively
    // stopped.  Moreover, it implies that we never attempted initialization,
    // so the registrar won't need stopping either.
    DCHECK_EQ(NOT_ATTEMPTED, initialization_state_);
    DCHECK(!registrar_.get());
  }
}

void SyncBackendHost::Shutdown(bool sync_disabled) {
  // TODO(tim): DCHECK(registrar_->StoppedOnUIThread()) would be nice.
  if (sync_thread_.IsRunning()) {
    sync_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&SyncBackendHost::Core::DoShutdown, core_.get(),
                   sync_disabled));
  }

  // Stop will return once the thread exits, which will be after DoShutdown
  // runs. DoShutdown needs to run from sync_thread_ because the sync backend
  // requires any thread that opened sqlite handles to relinquish them
  // personally. We need to join threads, because otherwise the main Chrome
  // thread (ui loop) can exit before DoShutdown finishes, at which point
  // virtually anything the sync backend does (or the post-back to
  // frontend_loop_ by our Core) will epically fail because the CRT won't be
  // initialized.
  // Since we are blocking the UI thread here, we need to turn ourselves in
  // with the ThreadRestriction police.  For sentencing and how we plan to fix
  // this, see bug 19757.
  {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    sync_thread_.Stop();
  }

  registrar_.reset();
  frontend_ = NULL;
  core_ = NULL;  // Releases reference to core_.
}

void SyncBackendHost::ConfigureDataTypes(
    syncable::ModelTypeSet types_to_add,
    syncable::ModelTypeSet types_to_remove,
    sync_api::ConfigureReason reason,
    base::Callback<void(syncable::ModelTypeSet)> ready_task,
    bool enable_nigori) {
  syncable::ModelTypeSet types_to_add_with_nigori = types_to_add;
  syncable::ModelTypeSet types_to_remove_with_nigori = types_to_remove;
  if (enable_nigori) {
    types_to_add_with_nigori.Put(syncable::NIGORI);
    types_to_remove_with_nigori.Remove(syncable::NIGORI);
  } else {
    types_to_add_with_nigori.Remove(syncable::NIGORI);
    types_to_remove_with_nigori.Put(syncable::NIGORI);
  }
  // Only one configure is allowed at a time.
  DCHECK(!pending_config_mode_state_.get());
  DCHECK(!pending_download_state_.get());
  DCHECK_GT(initialization_state_, NOT_INITIALIZED);

  pending_config_mode_state_.reset(new PendingConfigureDataTypesState());
  pending_config_mode_state_->ready_task = ready_task;
  pending_config_mode_state_->types_to_add = types_to_add_with_nigori;
  pending_config_mode_state_->added_types =
      registrar_->ConfigureDataTypes(types_to_add_with_nigori,
                                     types_to_remove_with_nigori);
  pending_config_mode_state_->reason = reason;

  // Cleanup disabled types before starting configuration so that
  // callers can assume that the data types are cleaned up once
  // configuration is done.
  if (!types_to_remove_with_nigori.Empty()) {
    sync_thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&SyncBackendHost::Core::DoRequestCleanupDisabledTypes,
                   core_.get()));
  }

  StartConfiguration(
      base::Bind(&SyncBackendHost::Core::FinishConfigureDataTypes,
                 core_.get()));
}

void SyncBackendHost::StartConfiguration(const base::Closure& callback) {
  // Put syncer in the config mode. DTM will put us in normal mode once it is
  // done. This is to ensure we dont do a normal sync when we are doing model
  // association.
  sync_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &SyncBackendHost::Core::DoStartConfiguration, core_.get(), callback));
}

void SyncBackendHost::EnableEncryptEverything() {
  sync_thread_.message_loop()->PostTask(FROM_HERE,
     base::Bind(&SyncBackendHost::Core::DoEnableEncryptEverything,
                core_.get()));
}

void SyncBackendHost::ActivateDataType(
    syncable::ModelType type, ModelSafeGroup group,
    ChangeProcessor* change_processor) {
  registrar_->ActivateDataType(type, group, change_processor, GetUserShare());
}

void SyncBackendHost::DeactivateDataType(syncable::ModelType type) {
  registrar_->DeactivateDataType(type);
}

bool SyncBackendHost::RequestClearServerData() {
  sync_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoRequestClearServerData,
                 core_.get()));
  return true;
}

sync_api::UserShare* SyncBackendHost::GetUserShare() const {
  DCHECK(initialized());
  return core_->sync_manager()->GetUserShare();
}

SyncBackendHost::Status SyncBackendHost::GetDetailedStatus() {
  DCHECK(initialized());
  return core_->sync_manager()->GetDetailedStatus();
}

SyncBackendHost::StatusSummary SyncBackendHost::GetStatusSummary() {
  DCHECK(initialized());
  return core_->sync_manager()->GetStatusSummary();
}

const GoogleServiceAuthError& SyncBackendHost::GetAuthError() const {
  return last_auth_error_;
}

const SyncSessionSnapshot* SyncBackendHost::GetLastSessionSnapshot() const {
  return last_snapshot_.get();
}

bool SyncBackendHost::HasUnsyncedItems() const {
  DCHECK(initialized());
  return core_->sync_manager()->HasUnsyncedItems();
}

bool SyncBackendHost::IsNigoriEnabled() const {
  return registrar_.get() && registrar_->IsNigoriEnabled();
}

bool SyncBackendHost::IsUsingExplicitPassphrase() {
  // This should only be called once we're initialized (and the nigori node has
  // therefore been downloaded) as otherwise we have no idea what kind of
  // passphrase we are using.
  if (!initialized()) {
    NOTREACHED() << "IsUsingExplicitPassphrase() should not be called "
                 << "before the nigori node is downloaded";
    return false;
  }
  return IsNigoriEnabled() &&
      core_->sync_manager()->IsUsingExplicitPassphrase();
}

bool SyncBackendHost::IsCryptographerReady(
    const sync_api::BaseTransaction* trans) const {
  return initialized() && trans->GetCryptographer()->is_ready();
}

void SyncBackendHost::GetModelSafeRoutingInfo(
    ModelSafeRoutingInfo* out) const {
  if (initialized()) {
    CHECK(registrar_.get());
    registrar_->GetModelSafeRoutingInfo(out);
  } else {
    NOTREACHED();
  }
}

void SyncBackendHost::InitCore(const DoInitializeOptions& options) {
  sync_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoInitialize, core_.get(),options));
}

void SyncBackendHost::HandleSyncCycleCompletedOnFrontendLoop(
    SyncSessionSnapshot* snapshot) {
  if (!frontend_)
    return;
  DCHECK_EQ(MessageLoop::current(), frontend_loop_);

  last_snapshot_.reset(snapshot);

  SDVLOG(1) << "Got snapshot " << snapshot->ToString();

  const syncable::ModelTypeSet to_migrate =
      snapshot->syncer_status.types_needing_local_migration;
  if (!to_migrate.Empty())
    frontend_->OnMigrationNeededForTypes(to_migrate);

  // Process any changes to the datatypes we're syncing.
  // TODO(sync): add support for removing types.
  if (initialized())
    AddExperimentalTypes();

  // If we are waiting for a configuration change, check here to see
  // if this sync cycle has initialized all of the types we've been
  // waiting for.
  if (pending_download_state_.get()) {
    scoped_ptr<PendingConfigureDataTypesState> state(
        pending_download_state_.release());
    const syncable::ModelTypeSet types_to_add = state->types_to_add;
    const syncable::ModelTypeSet added_types = state->added_types;
    DCHECK(types_to_add.HasAll(added_types));
    const syncable::ModelTypeSet initial_sync_ended =
        snapshot->initial_sync_ended;
    const syncable::ModelTypeSet failed_configuration_types =
        Difference(added_types, initial_sync_ended);
    SDVLOG(1)
        << "Added types: "
        << syncable::ModelTypeSetToString(added_types)
        << ", configured types: "
        << syncable::ModelTypeSetToString(initial_sync_ended)
        << ", failed configuration types: "
        << syncable::ModelTypeSetToString(failed_configuration_types);
    state->ready_task.Run(failed_configuration_types);
    if (!failed_configuration_types.Empty())
      return;
  }

  if (initialized())
    frontend_->OnSyncCycleCompleted();
}

void SyncBackendHost::FinishConfigureDataTypesOnFrontendLoop() {
  DCHECK_EQ(MessageLoop::current(), frontend_loop_);
  // Nudge the syncer. This is necessary for both datatype addition/deletion.
  //
  // Deletions need a nudge in order to ensure the deletion occurs in a timely
  // manner (see issue 56416).
  //
  // In the case of additions, on the next sync cycle, the syncer should
  // notice that the routing info has changed and start the process of
  // downloading updates for newly added data types.  Once this is
  // complete, the configure_state_.ready_task_ is run via an
  // OnInitializationComplete notification.

  SDVLOG(1) << "Syncer in config mode. SBH executing "
            << "FinishConfigureDataTypesOnFrontendLoop";

  if (pending_config_mode_state_->added_types.Empty() &&
      !core_->sync_manager()->InitialSyncEndedForAllEnabledTypes()) {

    syncable::ModelTypeSet enabled_types;
    ModelSafeRoutingInfo routing_info;
    registrar_->GetModelSafeRoutingInfo(&routing_info);
    for (ModelSafeRoutingInfo::const_iterator i = routing_info.begin();
         i != routing_info.end(); ++i) {
      enabled_types.Put(i->first);
    }

    // TODO(tim): Log / UMA / count this somehow?
    // Add only the types with empty progress markers. Note: it is possible
    // that some types have their initial_sync_ended be false but with non
    // empty progress marker. Which is ok as the rest of the changes would
    // be downloaded on a regular nudge and initial_sync_ended should be set
    // to true. However this is a very corner case. So it is not explicitly
    // handled.
    pending_config_mode_state_->added_types =
        sync_api::GetTypesWithEmptyProgressMarkerToken(enabled_types,
                                                       GetUserShare());
  }

  // If we've added types, we always want to request a nudge/config (even if
  // the initial sync is ended), in case we could not decrypt the data.
  if (pending_config_mode_state_->added_types.Empty()) {
    SDVLOG(1) << "No new types added; calling ready_task directly";
    // No new types - just notify the caller that the types are available.
    const syncable::ModelTypeSet failed_configuration_types;
    pending_config_mode_state_->ready_task.Run(failed_configuration_types);
  } else {
    pending_download_state_.reset(pending_config_mode_state_.release());

    // Always configure nigori if it's enabled.
    syncable::ModelTypeSet types_to_config =
        pending_download_state_->added_types;
    if (IsNigoriEnabled()) {
      // Note: Nigori is the only type that gets added with a nonempty
      // progress marker during config. If the server returns a migration
      // error then we will go into unrecoverable error. We dont handle it
      // explicitly because server might help us out here by not sending a
      // migraiton error for nigori during config.
      types_to_config.Put(syncable::NIGORI);
    }
    SDVLOG(1) << "Types "
              << syncable::ModelTypeSetToString(types_to_config)
              << " added; calling DoRequestConfig";
    sync_thread_.message_loop()->PostTask(FROM_HERE,
         base::Bind(&SyncBackendHost::Core::DoRequestConfig,
                    core_.get(),
                    types_to_config,
                    pending_download_state_->reason));
  }

  pending_config_mode_state_.reset();

  // Notify the SyncManager about the new types.
  sync_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoUpdateEnabledTypes, core_.get()));
}

bool SyncBackendHost::IsDownloadingNigoriForTest() const {
  return initialization_state_ == DOWNLOADING_NIGORI;
}

SyncBackendHost::DoInitializeOptions::DoInitializeOptions(
    MessageLoop* sync_loop,
    SyncBackendRegistrar* registrar,
    const WeakHandle<JsEventHandler>& event_handler,
    const GURL& service_url,
    MakeHttpBridgeFactoryFn make_http_bridge_factory_fn,
    const sync_api::SyncCredentials& credentials,
    sync_notifier::SyncNotifierFactory* sync_notifier_factory,
    bool delete_sync_data_folder,
    const std::string& restored_key_for_bootstrapping,
    bool setup_for_test_mode,
    UnrecoverableErrorHandler* unrecoverable_error_handler)
    : sync_loop(sync_loop),
      registrar(registrar),
      event_handler(event_handler),
      service_url(service_url),
      make_http_bridge_factory_fn(make_http_bridge_factory_fn),
      credentials(credentials),
      sync_notifier_factory(sync_notifier_factory),
      delete_sync_data_folder(delete_sync_data_folder),
      restored_key_for_bootstrapping(restored_key_for_bootstrapping),
      setup_for_test_mode(setup_for_test_mode),
      unrecoverable_error_handler(unrecoverable_error_handler){
}

SyncBackendHost::DoInitializeOptions::~DoInitializeOptions() {}

SyncBackendHost::Core::Core(const std::string& name,
                            const FilePath& sync_data_folder_path,
                            const base::WeakPtr<SyncBackendHost>& backend)
    : name_(name),
      sync_data_folder_path_(sync_data_folder_path),
      host_(backend),
      sync_loop_(NULL),
      registrar_(NULL) {
  DCHECK(backend.get());
}

SyncBackendHost::Core::~Core() {
  DCHECK(!sync_manager_.get());
  DCHECK(!sync_loop_);
}

SyncBackendHost::PendingConfigureDataTypesState::
PendingConfigureDataTypesState()
    : reason(sync_api::CONFIGURE_REASON_UNKNOWN) {}

SyncBackendHost::PendingConfigureDataTypesState::
~PendingConfigureDataTypesState() {}

void SyncBackendHost::Core::OnSyncCycleCompleted(
    const SyncSessionSnapshot* snapshot) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::HandleSyncCycleCompletedOnFrontendLoop,
      new SyncSessionSnapshot(*snapshot));
}


void SyncBackendHost::Core::OnInitializationComplete(
    const WeakHandle<JsBackend>& js_backend,
    bool success) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::HandleInitializationCompletedOnFrontendLoop,
      js_backend, success);

  if (success) {
    // Initialization is complete, so we can schedule recurring SaveChanges.
    sync_loop_->PostTask(FROM_HERE,
                         base::Bind(&Core::StartSavingChanges, this));
  }
}

void SyncBackendHost::Core::OnAuthError(const AuthError& auth_error) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::HandleAuthErrorEventOnFrontendLoop, auth_error);
}

void SyncBackendHost::Core::OnPassphraseRequired(
    sync_api::PassphraseRequiredReason reason) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::NotifyPassphraseRequired, reason);
}

void SyncBackendHost::Core::OnPassphraseAccepted(
    const std::string& bootstrap_token) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::NotifyPassphraseAccepted, bootstrap_token);
}

void SyncBackendHost::Core::OnStopSyncingPermanently() {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::HandleStopSyncingPermanentlyOnFrontendLoop);
}

void SyncBackendHost::Core::OnUpdatedToken(const std::string& token) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::NotifyUpdatedToken, token);
}

void SyncBackendHost::Core::OnClearServerDataFailed() {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::HandleClearServerDataFailedOnFrontendLoop);
}

void SyncBackendHost::Core::OnClearServerDataSucceeded() {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::HandleClearServerDataSucceededOnFrontendLoop);
}

void SyncBackendHost::Core::OnEncryptedTypesChanged(
    syncable::ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  // NOTE: We're in a transaction.
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::NotifyEncryptedTypesChanged,
      encrypted_types, encrypt_everything);
}

void SyncBackendHost::Core::OnEncryptionComplete() {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  // NOTE: We're in a transaction.
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::NotifyEncryptionComplete);
}

void SyncBackendHost::Core::OnActionableError(
    const browser_sync::SyncProtocolError& sync_error) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::HandleActionableErrorEventOnFrontendLoop,
      sync_error);
}

// Helper to construct a user agent string (ASCII) suitable for use by
// the syncapi for any HTTP communication. This string is used by the sync
// backend for classifying client types when calculating statistics.
std::string MakeUserAgentForSyncApi() {
  std::string user_agent;
  user_agent = "Chrome ";
#if defined(OS_WIN)
  user_agent += "WIN ";
#elif defined(OS_LINUX)
  user_agent += "LINUX ";
#elif defined(OS_FREEBSD)
  user_agent += "FREEBSD ";
#elif defined(OS_OPENBSD)
  user_agent += "OPENBSD ";
#elif defined(OS_MACOSX)
  user_agent += "MAC ";
#endif
  chrome::VersionInfo version_info;
  if (!version_info.is_valid()) {
    DLOG(ERROR) << "Unable to create chrome::VersionInfo object";
    return user_agent;
  }

  user_agent += version_info.Version();
  user_agent += " (" + version_info.LastChange() + ")";
  if (!version_info.IsOfficialBuild())
    user_agent += "-devel";
  return user_agent;
}

void SyncBackendHost::Core::DoInitialize(const DoInitializeOptions& options) {
  DCHECK(!sync_loop_);
  sync_loop_ = options.sync_loop;
  DCHECK(sync_loop_);

  // Blow away the partial or corrupt sync data folder before doing any more
  // initialization, if necessary.
  if (options.delete_sync_data_folder) {
    DeleteSyncDataFolder();
  }

  // Make sure that the directory exists before initializing the backend.
  // If it already exists, this will do no harm.
  bool success = file_util::CreateDirectory(sync_data_folder_path_);
  DCHECK(success);

  DCHECK(!registrar_);
  registrar_ = options.registrar;
  DCHECK(registrar_);

  sync_manager_.reset(new sync_api::SyncManager(name_));
  sync_manager_->AddObserver(this);
  success = sync_manager_->Init(
      sync_data_folder_path_,
      options.event_handler,
      options.service_url.host() + options.service_url.path(),
      options.service_url.EffectiveIntPort(),
      options.service_url.SchemeIsSecure(),
      options.make_http_bridge_factory_fn.Run(),
      options.registrar /* as ModelSafeWorkerRegistrar */,
      options.registrar /* as SyncManager::ChangeDelegate */,
      MakeUserAgentForSyncApi(),
      options.credentials,
      options.sync_notifier_factory->CreateSyncNotifier(),
      options.restored_key_for_bootstrapping,
      options.setup_for_test_mode,
      options.unrecoverable_error_handler);
  LOG_IF(ERROR, !success) << "Syncapi initialization failed!";
}

void SyncBackendHost::Core::DoCheckServerReachable() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sync_manager_->CheckServerReachable();
}

void SyncBackendHost::Core::DoUpdateCredentials(
    const SyncCredentials& credentials) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sync_manager_->UpdateCredentials(credentials);
}

void SyncBackendHost::Core::DoUpdateEnabledTypes() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sync_manager_->UpdateEnabledTypes();
}

void SyncBackendHost::Core::DoStartSyncing() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sync_manager_->StartSyncingNormally();
}

void SyncBackendHost::Core::DoRequestClearServerData() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sync_manager_->RequestClearServerData();
}

void SyncBackendHost::Core::DoRequestCleanupDisabledTypes() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sync_manager_->RequestCleanupDisabledTypes();
}

void SyncBackendHost::Core::DoSetPassphrase(const std::string& passphrase,
                                            bool is_explicit) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sync_manager_->SetPassphrase(passphrase, is_explicit);
}

void SyncBackendHost::Core::DoEnableEncryptEverything() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sync_manager_->EnableEncryptEverything();
}

void SyncBackendHost::Core::DoRefreshNigori(
    const base::Closure& done_callback) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sync_manager_->RefreshNigori(done_callback);
}

void SyncBackendHost::Core::DoStopSyncManagerForShutdown(
    const base::Closure& closure) {
  DCHECK(sync_manager_.get());
  sync_manager_->StopSyncingForShutdown(closure);
}

void SyncBackendHost::Core::DoShutdown(bool sync_disabled) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (!sync_manager_.get())
    return;

  save_changes_timer_.Stop();
  sync_manager_->ShutdownOnSyncThread();
  sync_manager_->RemoveObserver(this);
  sync_manager_.reset();
  registrar_ = NULL;

  if (sync_disabled)
    DeleteSyncDataFolder();

  sync_loop_ = NULL;

  host_.Reset();
}

void SyncBackendHost::Core::DoRequestConfig(
    syncable::ModelTypeSet types_to_config,
    sync_api::ConfigureReason reason) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sync_manager_->RequestConfig(types_to_config, reason);
}

void SyncBackendHost::Core::DoStartConfiguration(
    const base::Closure& callback) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sync_manager_->StartConfigurationMode(callback);
}

void SyncBackendHost::Core::DeleteSyncDataFolder() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (file_util::DirectoryExists(sync_data_folder_path_)) {
    if (!file_util::Delete(sync_data_folder_path_, true))
      SLOG(DFATAL) << "Could not delete the Sync Data folder.";
  }
}

void SyncBackendHost::Core::FinishConfigureDataTypes() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_.Call(
      FROM_HERE,
      &SyncBackendHost::FinishConfigureDataTypesOnFrontendLoop);
}

void SyncBackendHost::Core::StartSavingChanges() {
  // We may already be shut down.
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  save_changes_timer_.Start(FROM_HERE,
      base::TimeDelta::FromSeconds(kSaveChangesIntervalSeconds),
      this, &Core::SaveChanges);
}

void SyncBackendHost::Core::SaveChanges() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sync_manager_->SaveChanges();
}

void SyncBackendHost::AddExperimentalTypes() {
  CHECK(initialized());
  syncable::ModelTypeSet to_add;
  if (core_->sync_manager()->ReceivedExperimentalTypes(&to_add))
    frontend_->OnDataTypesChanged(to_add);
}

void SyncBackendHost::HandleInitializationCompletedOnFrontendLoop(
    const WeakHandle<JsBackend>& js_backend, bool success) {
  DCHECK_NE(NOT_ATTEMPTED, initialization_state_);
  if (!frontend_)
    return;

  // We've at least created the sync manager at this point, but if that is all
  // we've done we're just beginning the initialization process.
  if (initialization_state_ == CREATING_SYNC_MANAGER)
    initialization_state_ = NOT_INITIALIZED;

  DCHECK_EQ(MessageLoop::current(), frontend_loop_);
  if (!success) {
    initialization_state_ = NOT_INITIALIZED;
    frontend_->OnBackendInitialized(WeakHandle<JsBackend>(), false);
    return;
  }

  // If setup has completed, start off in DOWNLOADING_NIGORI so that
  // we start off by refreshing nigori.
  CHECK(sync_prefs_.get());
  if (sync_prefs_->HasSyncSetupCompleted() &&
      initialization_state_ < DOWNLOADING_NIGORI) {
    initialization_state_ = DOWNLOADING_NIGORI;
  }

  // Run initialization state machine.
  switch (initialization_state_) {
    case NOT_INITIALIZED:
      initialization_state_ = DOWNLOADING_NIGORI;
      ConfigureDataTypes(
          syncable::ModelTypeSet(),
          syncable::ModelTypeSet(),
          sync_api::CONFIGURE_REASON_NEW_CLIENT,
          // Calls back into this function.
          base::Bind(
              &SyncBackendHost::
                  HandleNigoriConfigurationCompletedOnFrontendLoop,
              weak_ptr_factory_.GetWeakPtr(), js_backend),
          true);
      break;
    case DOWNLOADING_NIGORI:
      initialization_state_ = REFRESHING_NIGORI;
      // Triggers OnEncryptedTypesChanged() and OnEncryptionComplete()
      // if necessary.
      RefreshNigori(
          base::Bind(
              &SyncBackendHost::
                  HandleInitializationCompletedOnFrontendLoop,
              weak_ptr_factory_.GetWeakPtr(), js_backend, true));
      break;
    case REFRESHING_NIGORI:
      initialization_state_ = INITIALIZED;
      // Now that we've downloaded the nigori node, we can see if there are any
      // experimental types to enable. This should be done before we inform
      // the frontend to ensure they're visible in the customize screen.
      AddExperimentalTypes();
      frontend_->OnBackendInitialized(js_backend, true);
      // Now that we're fully initialized, kick off a server
      // reachability check.
      sync_thread_.message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&SyncBackendHost::Core::DoCheckServerReachable,
                     core_.get()));
      break;
    default:
      NOTREACHED();
  }
}

void SyncBackendHost::PersistEncryptionBootstrapToken(
    const std::string& token) {
  CHECK(sync_prefs_.get());
  sync_prefs_->SetEncryptionBootstrapToken(token);
}

void SyncBackendHost::HandleActionableErrorEventOnFrontendLoop(
    const browser_sync::SyncProtocolError& sync_error) {
  if (!frontend_)
    return;
  DCHECK_EQ(MessageLoop::current(), frontend_loop_);
  frontend_->OnActionableError(sync_error);
}

void SyncBackendHost::NotifyPassphraseRequired(
    sync_api::PassphraseRequiredReason reason) {
  if (!frontend_)
    return;

  DCHECK_EQ(MessageLoop::current(), frontend_loop_);

  frontend_->OnPassphraseRequired(reason);
}

void SyncBackendHost::NotifyPassphraseAccepted(
    const std::string& bootstrap_token) {
  if (!frontend_)
    return;

  DCHECK_EQ(MessageLoop::current(), frontend_loop_);

  PersistEncryptionBootstrapToken(bootstrap_token);
  frontend_->OnPassphraseAccepted();
}

void SyncBackendHost::NotifyUpdatedToken(const std::string& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TokenAvailableDetails details(GaiaConstants::kSyncService, token);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TOKEN_UPDATED,
      content::Source<Profile>(profile_),
      content::Details<const TokenAvailableDetails>(&details));
}

void SyncBackendHost::NotifyEncryptedTypesChanged(
    syncable::ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  if (!frontend_)
    return;

  DCHECK_EQ(MessageLoop::current(), frontend_loop_);
  frontend_->OnEncryptedTypesChanged(
      encrypted_types, encrypt_everything);
}

void SyncBackendHost::NotifyEncryptionComplete() {
  if (!frontend_)
    return;

  DCHECK_EQ(MessageLoop::current(), frontend_loop_);
  frontend_->OnEncryptionComplete();
}

void SyncBackendHost::HandleStopSyncingPermanentlyOnFrontendLoop() {
  if (!frontend_)
    return;
  frontend_->OnStopSyncingPermanently();
}

void SyncBackendHost::HandleClearServerDataSucceededOnFrontendLoop() {
  if (!frontend_)
    return;
  frontend_->OnClearServerDataSucceeded();
}

void SyncBackendHost::HandleClearServerDataFailedOnFrontendLoop() {
  if (!frontend_)
    return;
  frontend_->OnClearServerDataFailed();
}

void SyncBackendHost::HandleAuthErrorEventOnFrontendLoop(
    const GoogleServiceAuthError& new_auth_error) {
  if (!frontend_)
    return;

  DCHECK_EQ(MessageLoop::current(), frontend_loop_);

  last_auth_error_ = new_auth_error;
  frontend_->OnAuthError();
}

void SyncBackendHost::HandleNigoriConfigurationCompletedOnFrontendLoop(
    const WeakHandle<JsBackend>& js_backend,
    const syncable::ModelTypeSet failed_configuration_types) {
  HandleInitializationCompletedOnFrontendLoop(
      js_backend, failed_configuration_types.Empty());
}

namespace {

// Needed because MessageLoop::PostTask is overloaded.
void PostClosure(MessageLoop* message_loop,
                 const tracked_objects::Location& from_here,
                 const base::Closure& callback) {
  message_loop->PostTask(from_here, callback);
}

}  // namespace

void SyncBackendHost::RefreshNigori(const base::Closure& done_callback) {
  DCHECK_EQ(MessageLoop::current(), frontend_loop_);
  base::Closure sync_thread_done_callback =
      base::Bind(&PostClosure,
                 MessageLoop::current(), FROM_HERE, done_callback);
  sync_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoRefreshNigori,
                 core_.get(), sync_thread_done_callback));
}

#undef SDVLOG

#undef SLOG

}  // namespace browser_sync
