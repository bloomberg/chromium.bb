// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/database_model_worker.h"
#include "chrome/browser/sync/glue/history_model_worker.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/http_bridge.h"
#include "chrome/browser/sync/glue/password_model_worker.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "webkit/glue/webkit_glue.h"

static const int kSaveChangesIntervalSeconds = 10;
static const char kGaiaServiceId[] = "chromiumsync";
static const char kGaiaSourceForChrome[] = "ChromiumBrowser";
static const FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

using browser_sync::DataTypeController;

typedef GoogleServiceAuthError AuthError;

namespace browser_sync {

using sessions::SyncSessionSnapshot;

SyncBackendHost::SyncBackendHost(
    SyncFrontend* frontend,
    Profile* profile,
    const FilePath& profile_path,
    const DataTypeController::TypeMap& data_type_controllers)
    : core_thread_("Chrome_SyncCoreThread"),
      frontend_loop_(MessageLoop::current()),
      profile_(profile),
      frontend_(frontend),
      sync_data_folder_path_(profile_path.Append(kSyncDataFolderName)),
      data_type_controllers_(data_type_controllers),
      last_auth_error_(AuthError::None()),
      syncapi_initialized_(false) {

  core_ = new Core(this);
}

SyncBackendHost::SyncBackendHost()
    : core_thread_("Chrome_SyncCoreThread"),
      frontend_loop_(MessageLoop::current()),
      profile_(NULL),
      frontend_(NULL),
      last_auth_error_(AuthError::None()),
      syncapi_initialized_(false) {
}

SyncBackendHost::~SyncBackendHost() {
  DCHECK(!core_ && !frontend_) << "Must call Shutdown before destructor.";
  DCHECK(registrar_.workers.empty());
}

void SyncBackendHost::Initialize(
    const GURL& sync_service_url,
    const syncable::ModelTypeSet& types,
    URLRequestContextGetter* baseline_context_getter,
    const std::string& lsid,
    bool delete_sync_data_folder,
    bool invalidate_sync_login,
    bool invalidate_sync_xmpp_login,
    bool use_chrome_async_socket,
    bool try_ssltcp_first,
    NotificationMethod notification_method) {
  if (!core_thread_.Start())
    return;

  // Create a worker for the UI thread and route bookmark changes to it.
  // TODO(tim): Pull this into a method to reuse.  For now we don't even
  // need to lock because we init before the syncapi exists and we tear down
  // after the syncapi is destroyed.  Make sure to NULL-check workers_ indices
  // when a new type is synced as the worker may already exist and you just
  // need to update routing_info_.
  registrar_.workers[GROUP_DB] = new DatabaseModelWorker();
  registrar_.workers[GROUP_HISTORY] =
      new HistoryModelWorker(
          profile_->GetHistoryService(Profile::IMPLICIT_ACCESS));
  registrar_.workers[GROUP_UI] = new UIModelWorker(frontend_loop_);
  registrar_.workers[GROUP_PASSIVE] = new ModelSafeWorker();
  registrar_.workers[GROUP_PASSWORD] =
      new PasswordModelWorker(
          profile_->GetPasswordStore(Profile::IMPLICIT_ACCESS));

  // Any datatypes that we want the syncer to pull down must
  // be in the routing_info map.  We set them to group passive, meaning that
  // updates will be applied, but not dispatched to the UI thread yet.
  for (syncable::ModelTypeSet::const_iterator it = types.begin();
      it != types.end(); ++it) {
    registrar_.routing_info[(*it)] = GROUP_PASSIVE;
  }

  InitCore(Core::DoInitializeOptions(
      sync_service_url, lsid.empty(),
      MakeHttpBridgeFactory(baseline_context_getter),
      MakeHttpBridgeFactory(baseline_context_getter),
      lsid,
      delete_sync_data_folder,
      invalidate_sync_login,
      invalidate_sync_xmpp_login,
      use_chrome_async_socket,
      try_ssltcp_first,
      notification_method,
      RestoreEncryptionBootstrapToken()));
}

void SyncBackendHost::PersistEncryptionBootstrapToken(
    const std::string& token) {
  PrefService* prefs = profile_->GetPrefs();

  prefs->SetString(prefs::kEncryptionBootstrapToken, token);
  prefs->ScheduleSavePersistentPrefs();
}

std::string SyncBackendHost::RestoreEncryptionBootstrapToken() {
  PrefService* prefs = profile_->GetPrefs();
  std::string token = prefs->GetString(prefs::kEncryptionBootstrapToken);
  return token;
}

sync_api::HttpPostProviderFactory* SyncBackendHost::MakeHttpBridgeFactory(
    URLRequestContextGetter* getter) {
  return new HttpBridgeFactory(getter);
}

void SyncBackendHost::InitCore(const Core::DoInitializeOptions& options) {
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(core_.get(), &SyncBackendHost::Core::DoInitialize,
                        options));
}

void SyncBackendHost::Authenticate(const std::string& username,
                                   const std::string& password,
                                   const std::string& captcha) {
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(core_.get(), &SyncBackendHost::Core::DoAuthenticate,
                        username, password, captcha));
}

void SyncBackendHost::StartSyncingWithServer() {
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(core_.get(), &SyncBackendHost::Core::DoStartSyncing));
}

void SyncBackendHost::SetPassphrase(const std::string& passphrase) {
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(core_.get(), &SyncBackendHost::Core::DoSetPassphrase,
                        passphrase));
}

void SyncBackendHost::Shutdown(bool sync_disabled) {
  // Thread shutdown should occur in the following order:
  // - SyncerThread
  // - CoreThread
  // - UI Thread (stops some time after we return from this call).
  if (core_thread_.IsRunning()) {  // Not running in tests.
    core_thread_.message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(core_.get(),
                          &SyncBackendHost::Core::DoShutdown,
                          sync_disabled));
  }

  // Before joining the core_thread_, we wait for the UIModelWorker to
  // give us the green light that it is not depending on the frontend_loop_ to
  // process any more tasks. Stop() blocks until this termination condition
  // is true.
  if (ui_worker())
    ui_worker()->Stop();

  // Stop will return once the thread exits, which will be after DoShutdown
  // runs. DoShutdown needs to run from core_thread_ because the sync backend
  // requires any thread that opened sqlite handles to relinquish them
  // personally. We need to join threads, because otherwise the main Chrome
  // thread (ui loop) can exit before DoShutdown finishes, at which point
  // virtually anything the sync backend does (or the post-back to
  // frontend_loop_ by our Core) will epically fail because the CRT won't be
  // initialized. For now this only ever happens at sync-enabled-Chrome exit,
  // meaning bug 1482548 applies to prolonged "waiting" that may occur in
  // DoShutdown.
  core_thread_.Stop();

  registrar_.routing_info.clear();
  registrar_.workers[GROUP_DB] = NULL;
  registrar_.workers[GROUP_HISTORY] = NULL;
  registrar_.workers[GROUP_UI] = NULL;
  registrar_.workers[GROUP_PASSIVE] = NULL;
  registrar_.workers[GROUP_PASSWORD] = NULL;
  registrar_.workers.erase(GROUP_DB);
  registrar_.workers.erase(GROUP_HISTORY);
  registrar_.workers.erase(GROUP_UI);
  registrar_.workers.erase(GROUP_PASSIVE);
  registrar_.workers.erase(GROUP_PASSWORD);
  frontend_ = NULL;
  core_ = NULL;  // Releases reference to core_.
}

void SyncBackendHost::ConfigureDataTypes(const syncable::ModelTypeSet& types,
                                         CancelableTask* ready_task) {
  // Only one configure is allowed at a time.
  DCHECK(!configure_ready_task_.get());
  DCHECK(syncapi_initialized_);

  {
    AutoLock lock(registrar_lock_);
    for (DataTypeController::TypeMap::const_iterator it =
             data_type_controllers_.begin();
         it != data_type_controllers_.end(); ++it) {
      syncable::ModelType type = (*it).first;

      // If a type is not specified, remove it from the routing_info.
      if (types.count(type) == 0) {
        registrar_.routing_info.erase(type);
      } else {
        // Add a newly specified data type as GROUP_PASSIVE into the
        // routing_info, if it does not already exist.
        if (registrar_.routing_info.count(type) == 0) {
          registrar_.routing_info[type] = GROUP_PASSIVE;
        }
      }
    }
  }

  // If no new data types were added to the passive group, no need to
  // wait for the syncer.
  if (core_->syncapi()->InitialSyncEndedForAllEnabledTypes()) {
    ready_task->Run();
    delete ready_task;
    return;
  }

  // Save the task here so we can run it when the syncer finishes
  // initializing the new data types.  It will be run only when the
  // set of initially synced data types matches the types requested in
  // this configure.
  configure_ready_task_.reset(ready_task);
  configure_initial_sync_types_ = types;

  // Nudge the syncer.  On the next sync cycle, the syncer should
  // notice that the routing info has changed and start the process of
  // downloading updates for newly added data types.  Once this is
  // complete, the configure_ready_task_ is run via an
  // OnInitializationComplete notification.
  RequestNudge();
}

void SyncBackendHost::RequestNudge() {
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(core_.get(), &SyncBackendHost::Core::DoRequestNudge));
}

void SyncBackendHost::ActivateDataType(
    DataTypeController* data_type_controller,
    ChangeProcessor* change_processor) {
  AutoLock lock(registrar_lock_);

  // Ensure that the given data type is in the PASSIVE group.
  browser_sync::ModelSafeRoutingInfo::iterator i =
      registrar_.routing_info.find(data_type_controller->type());
  DCHECK(i != registrar_.routing_info.end());
  DCHECK((*i).second == GROUP_PASSIVE);
  syncable::ModelType type = data_type_controller->type();
  // Change the data type's routing info to its group.
  registrar_.routing_info[type] = data_type_controller->model_safe_group();

  // Add the data type's change processor to the list of change
  // processors so it can receive updates.
  DCHECK_EQ(processors_.count(type), 0U);
  processors_[type] = change_processor;
}

void SyncBackendHost::DeactivateDataType(
    DataTypeController* data_type_controller,
    ChangeProcessor* change_processor) {
  AutoLock lock(registrar_lock_);
  registrar_.routing_info.erase(data_type_controller->type());

  std::map<syncable::ModelType, ChangeProcessor*>::size_type erased =
      processors_.erase(data_type_controller->type());
  DCHECK_EQ(erased, 1U);

  // TODO(sync): At this point we need to purge the data associated
  // with this data type from the sync db.
}

bool SyncBackendHost::RequestPause() {
  core_thread_.message_loop()->PostTask(FROM_HERE,
     NewRunnableMethod(core_.get(), &SyncBackendHost::Core::DoRequestPause));
  return true;
}

bool SyncBackendHost::RequestResume() {
  core_thread_.message_loop()->PostTask(FROM_HERE,
     NewRunnableMethod(core_.get(), &SyncBackendHost::Core::DoRequestResume));
  return true;
}

SyncBackendHost::Core::~Core() {
}

void SyncBackendHost::Core::NotifyPaused() {
  NotificationService::current()->Notify(NotificationType::SYNC_PAUSED,
                                         NotificationService::AllSources(),
                                         NotificationService::NoDetails());
}

void SyncBackendHost::Core::NotifyResumed() {
  NotificationService::current()->Notify(NotificationType::SYNC_RESUMED,
                                         NotificationService::AllSources(),
                                         NotificationService::NoDetails());
}

void SyncBackendHost::Core::NotifyPassphraseRequired() {
  NotificationService::current()->Notify(
      NotificationType::SYNC_PASSPHRASE_REQUIRED,
      NotificationService::AllSources(),
      NotificationService::NoDetails());
}

void SyncBackendHost::Core::NotifyPassphraseAccepted(
    const std::string& bootstrap_token) {
  host_->PersistEncryptionBootstrapToken(bootstrap_token);
  NotificationService::current()->Notify(
      NotificationType::SYNC_PASSPHRASE_ACCEPTED,
      NotificationService::AllSources(),
      NotificationService::NoDetails());
}

SyncBackendHost::UserShareHandle SyncBackendHost::GetUserShareHandle() const {
  DCHECK(syncapi_initialized_);
  return core_->syncapi()->GetUserShare();
}

SyncBackendHost::Status SyncBackendHost::GetDetailedStatus() {
  DCHECK(syncapi_initialized_);
  return core_->syncapi()->GetDetailedStatus();
}

SyncBackendHost::StatusSummary SyncBackendHost::GetStatusSummary() {
  DCHECK(syncapi_initialized_);
  return core_->syncapi()->GetStatusSummary();
}

string16 SyncBackendHost::GetAuthenticatedUsername() const {
  DCHECK(syncapi_initialized_);
  return UTF8ToUTF16(core_->syncapi()->GetAuthenticatedUsername());
}

const GoogleServiceAuthError& SyncBackendHost::GetAuthError() const {
  return last_auth_error_;
}

const SyncSessionSnapshot* SyncBackendHost::GetLastSessionSnapshot() const {
  return last_snapshot_.get();
}

void SyncBackendHost::GetWorkers(std::vector<ModelSafeWorker*>* out) {
  AutoLock lock(registrar_lock_);
  out->clear();
  for (WorkerMap::const_iterator it = registrar_.workers.begin();
       it != registrar_.workers.end(); ++it) {
    out->push_back((*it).second);
  }
}

void SyncBackendHost::GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {
  AutoLock lock(registrar_lock_);
  ModelSafeRoutingInfo copy(registrar_.routing_info);
  out->swap(copy);
}

bool SyncBackendHost::HasUnsyncedItems() const {
  DCHECK(syncapi_initialized_);
  return core_->syncapi()->HasUnsyncedItems();
}

SyncBackendHost::Core::Core(SyncBackendHost* backend)
    : host_(backend),
      syncapi_(new sync_api::SyncManager()) {
}

// Helper to construct a user agent string (ASCII) suitable for use by
// the syncapi for any HTTP communication. This string is used by the sync
// backend for classifying client types when calculating statistics.
std::string MakeUserAgentForSyncapi() {
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
  DCHECK(MessageLoop::current() == host_->core_thread_.message_loop());

  // Blow away the partial or corrupt sync data folder before doing any more
  // initialization, if necessary.
  if (options.delete_sync_data_folder)
    DeleteSyncDataFolder();

  // Make sure that the directory exists before initializing the backend.
  // If it already exists, this will do no harm.
  bool success = file_util::CreateDirectory(host_->sync_data_folder_path());
  DCHECK(success);

  syncapi_->SetObserver(this);
  const FilePath& path_str = host_->sync_data_folder_path();
  success = syncapi_->Init(path_str,
      (options.service_url.host() + options.service_url.path()).c_str(),
      options.service_url.EffectiveIntPort(),
      kGaiaServiceId,
      kGaiaSourceForChrome,
      options.service_url.SchemeIsSecure(),
      options.http_bridge_factory,
      options.auth_http_bridge_factory,
      host_,  // ModelSafeWorkerRegistrar.
      options.attempt_last_user_authentication,
      options.invalidate_sync_login,
      options.invalidate_sync_xmpp_login,
      MakeUserAgentForSyncapi().c_str(),
      options.lsid.c_str(),
      options.use_chrome_async_socket,
      options.try_ssltcp_first,
      options.notification_method,
      options.restored_key_for_bootstrapping);
  DCHECK(success) << "Syncapi initialization failed!";
}

void SyncBackendHost::Core::DoAuthenticate(const std::string& username,
                                           const std::string& password,
                                           const std::string& captcha) {
  DCHECK(MessageLoop::current() == host_->core_thread_.message_loop());
  syncapi_->Authenticate(username.c_str(), password.c_str(), captcha.c_str());
}

void SyncBackendHost::Core::DoStartSyncing() {
  DCHECK(MessageLoop::current() == host_->core_thread_.message_loop());
  syncapi_->StartSyncing();
}

void SyncBackendHost::Core::DoSetPassphrase(const std::string& passphrase) {
  DCHECK(MessageLoop::current() == host_->core_thread_.message_loop());
  syncapi_->SetPassphrase(passphrase);
}

UIModelWorker* SyncBackendHost::ui_worker() {
  ModelSafeWorker* w = registrar_.workers[GROUP_UI];
  if (w == NULL)
    return NULL;
  if (w->GetModelSafeGroup() != GROUP_UI)
    NOTREACHED();
  return static_cast<UIModelWorker*>(w);
}

void SyncBackendHost::Core::DoShutdown(bool sync_disabled) {
  DCHECK(MessageLoop::current() == host_->core_thread_.message_loop());

  save_changes_timer_.Stop();
  syncapi_->Shutdown();  // Stops the SyncerThread.
  syncapi_->RemoveObserver();
  host_->ui_worker()->OnSyncerShutdownComplete();

  if (sync_disabled)
    DeleteSyncDataFolder();

  host_ = NULL;
}

void SyncBackendHost::Core::OnChangesApplied(
    syncable::ModelType model_type,
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  if (!host_ || !host_->frontend_) {
    DCHECK(false) << "OnChangesApplied called after Shutdown?";
    return;
  }

  std::map<syncable::ModelType, ChangeProcessor*>::const_iterator it =
      host_->processors_.find(model_type);

  // Until model association happens for a datatype, it will not appear in
  // the processors list.  During this time, it is OK to drop changes on
  // the floor (since model association has not happened yet).  When the
  // data type is activated, model association takes place then the change
  // processor is added to the processors_ list.  This all happens on
  // the UI thread so we will never drop any changes after model
  // association.
  if (it == host_->processors_.end())
    return;

  if (!IsCurrentThreadSafeForModel(model_type)) {
    NOTREACHED() << "Changes applied on wrong thread.";
    return;
  }

  // Now that we're sure we're on the correct thread, we can access the
  // ChangeProcessor.
  ChangeProcessor* processor = it->second;

  // Ensure the change processor is willing to accept changes.
  if (!processor->IsRunning())
    return;

  processor->ApplyChangesFromSyncModel(trans, changes, change_count);
}

void SyncBackendHost::Core::OnSyncCycleCompleted(
    const SyncSessionSnapshot* snapshot) {
  host_->frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &Core::HandleSyncCycleCompletedOnFrontendLoop,
      new SyncSessionSnapshot(*snapshot)));
}

void SyncBackendHost::Core::HandleSyncCycleCompletedOnFrontendLoop(
    SyncSessionSnapshot* snapshot) {
  if (!host_ || !host_->frontend_)
    return;
  DCHECK_EQ(MessageLoop::current(), host_->frontend_loop_);

  host_->last_snapshot_.reset(snapshot);

  // If we are waiting for a configuration change, check here to see
  // if this sync cycle has initialized all of the types we've been
  // waiting for.
  if (host_->configure_ready_task_.get()) {
    bool found_all = true;
    for (syncable::ModelTypeSet::const_iterator it =
             host_->configure_initial_sync_types_.begin();
         it != host_->configure_initial_sync_types_.end(); ++it) {
      found_all &= snapshot->initial_sync_ended.test(*it);
    }

    if (found_all) {
      host_->configure_ready_task_->Run();
      host_->configure_ready_task_.reset();
      host_->configure_initial_sync_types_.clear();
    }
  }
  host_->frontend_->OnSyncCycleCompleted();
}

void SyncBackendHost::Core::OnInitializationComplete() {
  if (!host_ || !host_->frontend_)
    return;  // We may have been told to Shutdown before initialization
             // completed.

  // We could be on some random sync backend thread, so MessageLoop::current()
  // can definitely be null in here.
  host_->frontend_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &Core::HandleInitalizationCompletedOnFrontendLoop));

  // Initialization is complete, so we can schedule recurring SaveChanges.
  host_->core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &Core::StartSavingChanges));
}

void SyncBackendHost::Core::HandleInitalizationCompletedOnFrontendLoop() {
  if (!host_)
    return;
  host_->HandleInitializationCompletedOnFrontendLoop();
}

void SyncBackendHost::HandleInitializationCompletedOnFrontendLoop() {
  if (!frontend_)
    return;
  syncapi_initialized_ = true;
  frontend_->OnBackendInitialized();
}

bool SyncBackendHost::Core::IsCurrentThreadSafeForModel(
    syncable::ModelType model_type) {
  AutoLock lock(host_->registrar_lock_);

  browser_sync::ModelSafeRoutingInfo::const_iterator routing_it =
      host_->registrar_.routing_info.find(model_type);
  if (routing_it == host_->registrar_.routing_info.end())
    return false;
  browser_sync::ModelSafeGroup group = routing_it->second;
  WorkerMap::const_iterator worker_it = host_->registrar_.workers.find(group);
  if (worker_it == host_->registrar_.workers.end())
    return false;
  ModelSafeWorker* worker = worker_it->second;
  return worker->CurrentThreadIsWorkThread();
}


void SyncBackendHost::Core::OnAuthError(const AuthError& auth_error) {
  // We could be on SyncEngine_AuthWatcherThread.  Post to our core loop so
  // we can modify state.
  host_->frontend_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &Core::HandleAuthErrorEventOnFrontendLoop,
      auth_error));
}

void SyncBackendHost::Core::OnPassphraseRequired() {
  host_->frontend_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &Core::NotifyPassphraseRequired));
}

void SyncBackendHost::Core::OnPassphraseAccepted(
    const std::string& bootstrap_token) {
  host_->frontend_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &Core::NotifyPassphraseAccepted,
          bootstrap_token));
}

void SyncBackendHost::Core::OnPaused() {
  host_->frontend_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &Core::NotifyPaused));
}

void SyncBackendHost::Core::OnResumed() {
  host_->frontend_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &Core::NotifyResumed));
}

void SyncBackendHost::Core::OnStopSyncingPermanently() {
  host_->frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &Core::HandleStopSyncingPermanentlyOnFrontendLoop));
}

void SyncBackendHost::Core::HandleStopSyncingPermanentlyOnFrontendLoop() {
  if (!host_ || !host_->frontend_)
    return;
  host_->frontend_->OnStopSyncingPermanently();
}

void SyncBackendHost::Core::HandleAuthErrorEventOnFrontendLoop(
    const GoogleServiceAuthError& new_auth_error) {
  if (!host_ || !host_->frontend_)
    return;

  DCHECK_EQ(MessageLoop::current(), host_->frontend_loop_);

  host_->last_auth_error_ = new_auth_error;
  host_->frontend_->OnAuthError();
}

void SyncBackendHost::Core::StartSavingChanges() {
  save_changes_timer_.Start(
      base::TimeDelta::FromSeconds(kSaveChangesIntervalSeconds),
      this, &Core::SaveChanges);
}

void SyncBackendHost::Core::DoRequestNudge() {
  syncapi_->RequestNudge();
}

void SyncBackendHost::Core::DoRequestResume() {
  syncapi_->RequestResume();
}

void SyncBackendHost::Core::DoRequestPause() {
  syncapi()->RequestPause();
}

void SyncBackendHost::Core::SaveChanges() {
  syncapi_->SaveChanges();
}

void SyncBackendHost::Core::DeleteSyncDataFolder() {
  if (file_util::DirectoryExists(host_->sync_data_folder_path())) {
    if (!file_util::Delete(host_->sync_data_folder_path(), true))
      LOG(DFATAL) << "Could not delete the Sync Data folder.";
  }
}

}  // namespace browser_sync
