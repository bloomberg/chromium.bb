// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/sync/glue/sync_backend_host.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/profiles/profile.h"
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

#define SVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

SyncBackendHost::SyncBackendHost(const std::string& name,
                                 Profile* profile,
                                 const base::WeakPtr<SyncPrefs>& sync_prefs)
    : core_(new Core(name, ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      initialization_state_(NOT_ATTEMPTED),
      sync_thread_("Chrome_SyncThread"),
      frontend_loop_(MessageLoop::current()),
      profile_(profile),
      sync_prefs_(sync_prefs),
      name_(name),
      sync_notifier_factory_(
          content::GetUserAgent(GURL()),
          profile_->GetRequestContext(),
          sync_prefs,
          *CommandLine::ForCurrentProcess()),
      frontend_(NULL),
      sync_data_folder_path_(
          profile_->GetPath().Append(kSyncDataFolderName)),
      last_auth_error_(AuthError::None()) {
}

SyncBackendHost::SyncBackendHost()
    : initialization_state_(NOT_ATTEMPTED),
      sync_thread_("Chrome_SyncThread"),
      frontend_loop_(MessageLoop::current()),
      profile_(NULL),
      name_("Unknown"),
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

void SyncBackendHost::Initialize(
    SyncFrontend* frontend,
    const WeakHandle<JsEventHandler>& event_handler,
    const GURL& sync_service_url,
    const syncable::ModelTypeSet& initial_types,
    const SyncCredentials& credentials,
    bool delete_sync_data_folder) {
  if (!sync_thread_.Start())
    return;

  frontend_ = frontend;
  DCHECK(frontend);

  syncable::ModelTypeSet initial_types_with_nigori(initial_types);
  CHECK(sync_prefs_.get());
  if (sync_prefs_->HasSyncSetupCompleted()) {
    initial_types_with_nigori.insert(syncable::NIGORI);
  }

  registrar_.reset(new SyncBackendRegistrar(initial_types_with_nigori,
                                            name_,
                                            profile_,
                                            sync_thread_.message_loop()));
  initialization_state_ = CREATING_SYNC_MANAGER;
  InitCore(Core::DoInitializeOptions(
      sync_thread_.message_loop(),
      registrar_.get(),
      event_handler,
      sync_service_url,
      profile_->GetRequestContext(),
      credentials,
      delete_sync_data_folder,
      sync_prefs_->GetEncryptionBootstrapToken(),
      false));
}

void SyncBackendHost::InitCore(const Core::DoInitializeOptions& options) {
  sync_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoInitialize, core_.get(),options));
}

void SyncBackendHost::UpdateCredentials(const SyncCredentials& credentials) {
  sync_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoUpdateCredentials, core_.get(),
                 credentials));
}

void SyncBackendHost::StartSyncingWithServer() {
  SVLOG(1) << "SyncBackendHost::StartSyncingWithServer called.";
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
    const syncable::ModelTypeSet& types_to_add,
    const syncable::ModelTypeSet& types_to_remove,
    sync_api::ConfigureReason reason,
    base::Callback<void(const syncable::ModelTypeSet&)> ready_task,
    bool enable_nigori) {
  syncable::ModelTypeSet types_to_add_with_nigori = types_to_add;
  syncable::ModelTypeSet types_to_remove_with_nigori = types_to_remove;
  if (enable_nigori) {
    types_to_add_with_nigori.insert(syncable::NIGORI);
    types_to_remove_with_nigori.erase(syncable::NIGORI);
  } else {
    types_to_add_with_nigori.erase(syncable::NIGORI);
    types_to_remove_with_nigori.insert(syncable::NIGORI);
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
  if (!types_to_remove_with_nigori.empty()) {
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

SyncBackendHost::Core::Core(const std::string& name,
                            SyncBackendHost* backend)
    : name_(name),
      host_(backend),
      sync_loop_(NULL),
      registrar_(NULL) {
  DCHECK(host_);
}

SyncBackendHost::Core::~Core() {
  DCHECK(!sync_manager_.get());
  DCHECK(!sync_loop_);
}

void SyncBackendHost::Core::OnSyncCycleCompleted(
    const SyncSessionSnapshot* snapshot) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_->frontend_loop_->PostTask(FROM_HERE, base::Bind(
      &Core::HandleSyncCycleCompletedOnFrontendLoop,
      this,
      new SyncSessionSnapshot(*snapshot)));
}


void SyncBackendHost::Core::OnInitializationComplete(
    const WeakHandle<JsBackend>& js_backend,
    bool success) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

  host_->frontend_loop_->PostTask(FROM_HERE,
      base::Bind(&Core::HandleInitializationCompletedOnFrontendLoop, this,
                 js_backend, success));

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
  // Post to our core loop so we can modify state. Could be on another thread.
  host_->frontend_loop_->PostTask(FROM_HERE,
      base::Bind(&Core::HandleAuthErrorEventOnFrontendLoop, this, auth_error));
}

void SyncBackendHost::Core::OnPassphraseRequired(
    sync_api::PassphraseRequiredReason reason) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_->frontend_loop_->PostTask(FROM_HERE,
      base::Bind(&Core::NotifyPassphraseRequired, this, reason));
}

void SyncBackendHost::Core::OnPassphraseAccepted(
    const std::string& bootstrap_token) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_->frontend_loop_->PostTask(FROM_HERE,
      base::Bind(&Core::NotifyPassphraseAccepted, this, bootstrap_token));
}

void SyncBackendHost::Core::OnStopSyncingPermanently() {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_->frontend_loop_->PostTask(FROM_HERE, base::Bind(
      &Core::HandleStopSyncingPermanentlyOnFrontendLoop, this));
}

void SyncBackendHost::Core::OnUpdatedToken(const std::string& token) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_->frontend_loop_->PostTask(FROM_HERE, base::Bind(
      &Core::NotifyUpdatedToken, this, token));
}

void SyncBackendHost::Core::OnClearServerDataFailed() {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_->frontend_loop_->PostTask(FROM_HERE, base::Bind(
      &Core::HandleClearServerDataFailedOnFrontendLoop, this));
}

void SyncBackendHost::Core::OnClearServerDataSucceeded() {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_->frontend_loop_->PostTask(FROM_HERE, base::Bind(
      &Core::HandleClearServerDataSucceededOnFrontendLoop, this));
}

void SyncBackendHost::Core::OnEncryptedTypesChanged(
    const syncable::ModelTypeSet& encrypted_types,
    bool encrypt_everything) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  // NOTE: We're in a transaction.
  host_->frontend_loop_->PostTask(
      FROM_HERE,
      base::Bind(&Core::NotifyEncryptedTypesChanged, this,
                 encrypted_types, encrypt_everything));
}

void SyncBackendHost::Core::OnEncryptionComplete() {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  // NOTE: We're in a transaction.
  host_->frontend_loop_->PostTask(
      FROM_HERE,
      base::Bind(&Core::NotifyEncryptionComplete, this));
}

void SyncBackendHost::Core::OnActionableError(
    const browser_sync::SyncProtocolError& sync_error) {
  if (!sync_loop_)
    return;
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_->frontend_loop_->PostTask(
      FROM_HERE,
      base::Bind(&Core::HandleActionableErrorEventOnFrontendLoop, this,
                 sync_error));
}

SyncBackendHost::Core::DoInitializeOptions::DoInitializeOptions(
    MessageLoop* sync_loop,
    SyncBackendRegistrar* registrar,
    const WeakHandle<JsEventHandler>& event_handler,
    const GURL& service_url,
    const scoped_refptr<net::URLRequestContextGetter>&
        request_context_getter,
    const sync_api::SyncCredentials& credentials,
    bool delete_sync_data_folder,
    const std::string& restored_key_for_bootstrapping,
    bool setup_for_test_mode)
    : sync_loop(sync_loop),
      registrar(registrar),
      event_handler(event_handler),
      service_url(service_url),
      request_context_getter(request_context_getter),
      credentials(credentials),
      delete_sync_data_folder(delete_sync_data_folder),
      restored_key_for_bootstrapping(restored_key_for_bootstrapping),
      setup_for_test_mode(setup_for_test_mode) {
}

SyncBackendHost::Core::DoInitializeOptions::~DoInitializeOptions() {}

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
  bool success = file_util::CreateDirectory(host_->sync_data_folder_path());
  DCHECK(success);

  DCHECK(!registrar_);
  registrar_ = options.registrar;
  DCHECK(registrar_);

  sync_manager_.reset(new sync_api::SyncManager(name_));
  sync_manager_->AddObserver(this);
  const FilePath& path_str = host_->sync_data_folder_path();
  success = sync_manager_->Init(
      path_str,
      options.event_handler,
      options.service_url.host() + options.service_url.path(),
      options.service_url.EffectiveIntPort(),
      options.service_url.SchemeIsSecure(),
      host_->MakeHttpBridgeFactory(options.request_context_getter),
      options.registrar /* as ModelSafeWorkerRegistrar */,
      options.registrar /* as SyncManager::ChangeDelegate */,
      MakeUserAgentForSyncApi(),
      options.credentials,
      host_->sync_notifier_factory_.CreateSyncNotifier(),
      options.restored_key_for_bootstrapping,
      options.setup_for_test_mode);
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

void SyncBackendHost::Core::DoRefreshEncryption(
    const base::Closure& done_callback) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sync_manager_->RefreshEncryption();
  done_callback.Run();
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

  host_ = NULL;
}

void SyncBackendHost::Core::DoRequestConfig(
    const syncable::ModelTypeBitSet& types_to_config,
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
  if (file_util::DirectoryExists(host_->sync_data_folder_path())) {
    if (!file_util::Delete(host_->sync_data_folder_path(), true))
      SLOG(DFATAL) << "Could not delete the Sync Data folder.";
  }
}

void SyncBackendHost::Core::FinishConfigureDataTypes() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  host_->frontend_loop_->PostTask(FROM_HERE, base::Bind(
      &SyncBackendHost::Core::FinishConfigureDataTypesOnFrontendLoop, this));
}

void SyncBackendHost::Core::HandleInitializationCompletedOnFrontendLoop(
    const WeakHandle<JsBackend>& js_backend,
    bool success) {
  if (!host_)
    return;
  host_->HandleInitializationCompletedOnFrontendLoop(js_backend, success);
}

void SyncBackendHost::Core::HandleNigoriConfigurationCompletedOnFrontendLoop(
    const WeakHandle<JsBackend>& js_backend,
    const syncable::ModelTypeSet& failed_configuration_types) {
  if (!host_)
    return;
  host_->HandleInitializationCompletedOnFrontendLoop(
      js_backend, failed_configuration_types.empty());
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

void SyncBackendHost::Core::HandleActionableErrorEventOnFrontendLoop(
    const browser_sync::SyncProtocolError& sync_error) {
  if (!host_ || !host_->frontend_)
    return;
  DCHECK_EQ(MessageLoop::current(), host_->frontend_loop_);
  host_->frontend_->OnActionableError(sync_error);
}

void SyncBackendHost::Core::HandleAuthErrorEventOnFrontendLoop(
    const GoogleServiceAuthError& new_auth_error) {
  if (!host_ || !host_->frontend_)
    return;

  DCHECK_EQ(MessageLoop::current(), host_->frontend_loop_);

  host_->last_auth_error_ = new_auth_error;
  host_->frontend_->OnAuthError();
}

void SyncBackendHost::Core::NotifyPassphraseRequired(
    sync_api::PassphraseRequiredReason reason) {
  if (!host_ || !host_->frontend_)
    return;

  DCHECK_EQ(MessageLoop::current(), host_->frontend_loop_);

  host_->frontend_->OnPassphraseRequired(reason);
}

void SyncBackendHost::Core::NotifyPassphraseAccepted(
    const std::string& bootstrap_token) {
  if (!host_ || !host_->frontend_)
    return;

  DCHECK_EQ(MessageLoop::current(), host_->frontend_loop_);

  host_->PersistEncryptionBootstrapToken(bootstrap_token);
  host_->frontend_->OnPassphraseAccepted();
}

void SyncBackendHost::Core::NotifyUpdatedToken(const std::string& token) {
  if (!host_)
    return;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TokenAvailableDetails details(GaiaConstants::kSyncService, token);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TOKEN_UPDATED,
      content::Source<Profile>(host_->profile_),
      content::Details<const TokenAvailableDetails>(&details));
}

void SyncBackendHost::Core::NotifyEncryptedTypesChanged(
    const syncable::ModelTypeSet& encrypted_types,
    bool encrypt_everything) {
  if (!host_)
    return;
  DCHECK_EQ(MessageLoop::current(), host_->frontend_loop_);
  host_->frontend_->OnEncryptedTypesChanged(
      encrypted_types, encrypt_everything);
}

void SyncBackendHost::Core::NotifyEncryptionComplete() {
  if (!host_)
    return;
  DCHECK_EQ(MessageLoop::current(), host_->frontend_loop_);
  host_->frontend_->OnEncryptionComplete();
}

void SyncBackendHost::Core::HandleSyncCycleCompletedOnFrontendLoop(
    SyncSessionSnapshot* snapshot) {
  if (!host_ || !host_->frontend_)
    return;
  DCHECK_EQ(MessageLoop::current(), host_->frontend_loop_);

  host_->last_snapshot_.reset(snapshot);

  SVLOG(1) << "Got snapshot " << snapshot->ToString();

  const syncable::ModelTypeSet& to_migrate =
      snapshot->syncer_status.types_needing_local_migration;
  if (!to_migrate.empty())
    host_->frontend_->OnMigrationNeededForTypes(to_migrate);

  // Process any changes to the datatypes we're syncing.
  // TODO(sync): add support for removing types.
  if (host_->initialized())
    host_->AddExperimentalTypes();

  // If we are waiting for a configuration change, check here to see
  // if this sync cycle has initialized all of the types we've been
  // waiting for.
  if (host_->pending_download_state_.get()) {
    scoped_ptr<PendingConfigureDataTypesState> state(
        host_->pending_download_state_.release());
    DCHECK(
        std::includes(state->types_to_add.begin(), state->types_to_add.end(),
                      state->added_types.begin(), state->added_types.end()));
    syncable::ModelTypeSet initial_sync_ended =
        syncable::ModelTypeBitSetToSet(snapshot->initial_sync_ended);
    syncable::ModelTypeSet failed_configuration_types;
    std::set_difference(
        state->added_types.begin(), state->added_types.end(),
        initial_sync_ended.begin(), initial_sync_ended.end(),
        std::inserter(failed_configuration_types,
                      failed_configuration_types.end()));
    SVLOG(1)
        << "Added types: "
        << syncable::ModelTypeSetToString(state->added_types)
        << ", configured types: "
        << syncable::ModelTypeSetToString(initial_sync_ended)
        << ", failed configuration types: "
        << syncable::ModelTypeSetToString(failed_configuration_types);
    state->ready_task.Run(failed_configuration_types);
    if (!failed_configuration_types.empty())
      return;
  }

  if (host_->initialized())
    host_->frontend_->OnSyncCycleCompleted();
}

void SyncBackendHost::Core::HandleStopSyncingPermanentlyOnFrontendLoop() {
  if (!host_ || !host_->frontend_)
    return;
  host_->frontend_->OnStopSyncingPermanently();
}

void SyncBackendHost::Core::HandleClearServerDataSucceededOnFrontendLoop() {
  if (!host_ || !host_->frontend_)
    return;
  host_->frontend_->OnClearServerDataSucceeded();
}

void SyncBackendHost::Core::HandleClearServerDataFailedOnFrontendLoop() {
  if (!host_ || !host_->frontend_)
    return;
  host_->frontend_->OnClearServerDataFailed();
}

void SyncBackendHost::Core::FinishConfigureDataTypesOnFrontendLoop() {
  host_->FinishConfigureDataTypesOnFrontendLoop();
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
  // we start off by refreshing encryption.
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
              &SyncBackendHost::Core::
                  HandleNigoriConfigurationCompletedOnFrontendLoop,
              core_.get(), js_backend),
          true);
      break;
    case DOWNLOADING_NIGORI:
      initialization_state_ = REFRESHING_ENCRYPTION;
      // Triggers OnEncryptedTypesChanged() and OnEncryptionComplete()
      // if necessary.
      RefreshEncryption(
          base::Bind(
              &SyncBackendHost::Core::
                  HandleInitializationCompletedOnFrontendLoop,
              core_.get(), js_backend, true));
      break;
    case REFRESHING_ENCRYPTION:
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

  SVLOG(1) << "Syncer in config mode. SBH executing "
           << "FinishConfigureDataTypesOnFrontendLoop";

  if (pending_config_mode_state_->added_types.empty() &&
      !core_->sync_manager()->InitialSyncEndedForAllEnabledTypes()) {

    syncable::ModelTypeSet enabled_types;
    ModelSafeRoutingInfo routing_info;
    registrar_->GetModelSafeRoutingInfo(&routing_info);
    for (ModelSafeRoutingInfo::const_iterator i = routing_info.begin();
         i != routing_info.end(); ++i) {
      enabled_types.insert(i->first);
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
  if (pending_config_mode_state_->added_types.empty()) {
    SVLOG(1) << "No new types added; calling ready_task directly";
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
      types_to_config.insert(syncable::NIGORI);
    }
    SVLOG(1) << "Types " << ModelTypeSetToString(types_to_config)
            << " added; calling DoRequestConfig";
    sync_thread_.message_loop()->PostTask(FROM_HERE,
         base::Bind(&SyncBackendHost::Core::DoRequestConfig,
                    core_.get(),
                    syncable::ModelTypeBitSetFromSet(types_to_config),
                    pending_download_state_->reason));
  }

  pending_config_mode_state_.reset();

  // Notify the SyncManager about the new types.
  sync_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoUpdateEnabledTypes, core_.get()));
}

sync_api::HttpPostProviderFactory* SyncBackendHost::MakeHttpBridgeFactory(
    const scoped_refptr<net::URLRequestContextGetter>& getter) {
  return new HttpBridgeFactory(getter);
}

void SyncBackendHost::PersistEncryptionBootstrapToken(
    const std::string& token) {
  CHECK(sync_prefs_.get());
  sync_prefs_->SetEncryptionBootstrapToken(token);
}

SyncBackendHost::PendingConfigureDataTypesState::
PendingConfigureDataTypesState()
    : reason(sync_api::CONFIGURE_REASON_UNKNOWN) {}

SyncBackendHost::PendingConfigureDataTypesState::
~PendingConfigureDataTypesState() {}

namespace {

// Needed because MessageLoop::PostTask is overloaded.
void PostClosure(MessageLoop* message_loop,
                 const tracked_objects::Location& from_here,
                 const base::Closure& callback) {
  message_loop->PostTask(from_here, callback);
}

}  // namespace

void SyncBackendHost::RefreshEncryption(const base::Closure& done_callback) {
  DCHECK_EQ(MessageLoop::current(), frontend_loop_);
  base::Closure sync_thread_done_callback =
      base::Bind(&PostClosure,
                 MessageLoop::current(), FROM_HERE, done_callback);
  sync_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHost::Core::DoRefreshEncryption,
                 core_.get(), sync_thread_done_callback));
}

#undef SVLOG

#undef SLOG

}  // namespace browser_sync
