// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/task.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/autofill_model_associator.h"
#include "chrome/browser/sync/glue/autofill_profile_model_associator.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/database_model_worker.h"
#include "chrome/browser/sync/glue/history_model_worker.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/http_bridge.h"
#include "chrome/browser/sync/glue/password_model_worker.h"
#include "chrome/browser/sync/sessions/session_state.h"
// TODO(tim): Remove this! We should have a syncapi pass-thru instead.
#include "chrome/browser/sync/syncable/directory_manager.h"  // Cryptographer.
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "webkit/glue/webkit_glue.h"

static const int kSaveChangesIntervalSeconds = 10;
static const FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

using browser_sync::DataTypeController;
typedef TokenService::TokenAvailableDetails TokenAvailableDetails;

typedef GoogleServiceAuthError AuthError;

namespace browser_sync {

using sessions::SyncSessionSnapshot;
using sync_api::SyncCredentials;

SyncBackendHost::SyncBackendHost(Profile* profile)
    : core_(new Core(ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      core_thread_("Chrome_SyncCoreThread"),
      frontend_loop_(MessageLoop::current()),
      profile_(profile),
      frontend_(NULL),
      sync_data_folder_path_(
          profile_->GetPath().Append(kSyncDataFolderName)),
      last_auth_error_(AuthError::None()),
      syncapi_initialized_(false) {
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
    SyncFrontend* frontend,
    const GURL& sync_service_url,
    const syncable::ModelTypeSet& types,
    URLRequestContextGetter* baseline_context_getter,
    const SyncCredentials& credentials,
    bool delete_sync_data_folder,
    const notifier::NotifierOptions& notifier_options) {
  if (!core_thread_.Start())
    return;

  frontend_ = frontend;
  DCHECK(frontend);

  // Create a worker for the UI thread and route bookmark changes to it.
  // TODO(tim): Pull this into a method to reuse.  For now we don't even
  // need to lock because we init before the syncapi exists and we tear down
  // after the syncapi is destroyed.  Make sure to NULL-check workers_ indices
  // when a new type is synced as the worker may already exist and you just
  // need to update routing_info_.
  registrar_.workers[GROUP_DB] = new DatabaseModelWorker();
  registrar_.workers[GROUP_UI] = new UIModelWorker(frontend_loop_);
  registrar_.workers[GROUP_PASSIVE] = new ModelSafeWorker();

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSyncTypedUrls) || types.count(syncable::TYPED_URLS)) {
    // TODO(tim): Bug 53916.  HistoryModelWorker crashes, so avoid adding it
    // unless specifically requested until bug is fixed.
    registrar_.workers[GROUP_HISTORY] =
        new HistoryModelWorker(
            profile_->GetHistoryService(Profile::IMPLICIT_ACCESS));
  }

  PasswordStore* password_store =
      profile_->GetPasswordStore(Profile::IMPLICIT_ACCESS);
  if (password_store) {
    registrar_.workers[GROUP_PASSWORD] =
        new PasswordModelWorker(password_store);
  } else {
    LOG(WARNING) << "Password store not initialized, cannot sync passwords";
  }

  // Any datatypes that we want the syncer to pull down must
  // be in the routing_info map.  We set them to group passive, meaning that
  // updates will be applied, but not dispatched to the UI thread yet.
  for (syncable::ModelTypeSet::const_iterator it = types.begin();
      it != types.end(); ++it) {
    registrar_.routing_info[(*it)] = GROUP_PASSIVE;
  }

  // TODO(tim): Remove this special case once NIGORI is populated by
  // default.  We piggy back off of the passwords flag for now to not
  // require both encryption and passwords flags.
  bool enable_encryption = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSyncPasswords) || types.count(syncable::PASSWORDS);
  if (enable_encryption)
    registrar_.routing_info[syncable::NIGORI] = GROUP_PASSIVE;

  InitCore(Core::DoInitializeOptions(
      sync_service_url,
      MakeHttpBridgeFactory(baseline_context_getter),
      credentials,
      delete_sync_data_folder,
      notifier_options,
      RestoreEncryptionBootstrapToken(),
      false));
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

bool SyncBackendHost::IsNigoriEnabled() const {
  base::AutoLock lock(registrar_lock_);
  // Note that NIGORI is only ever added/removed from routing_info once,
  // during initialization / first configuration, so there is no real 'race'
  // possible here or possibility of stale return value.
  return registrar_.routing_info.find(syncable::NIGORI) !=
      registrar_.routing_info.end();
}

bool SyncBackendHost::IsUsingExplicitPassphrase() {
  return IsNigoriEnabled() && syncapi_initialized_ &&
      core_->syncapi()->InitialSyncEndedForAllEnabledTypes() &&
      core_->syncapi()->IsUsingExplicitPassphrase();
}

bool SyncBackendHost::IsCryptographerReady() const {
  return syncapi_initialized_ &&
      GetUserShare()->dir_manager->cryptographer()->is_ready();
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

void SyncBackendHost::UpdateCredentials(const SyncCredentials& credentials) {
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(core_.get(),
                        &SyncBackendHost::Core::DoUpdateCredentials,
                        credentials));
}

void SyncBackendHost::StartSyncingWithServer() {
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(core_.get(), &SyncBackendHost::Core::DoStartSyncing));
}

void SyncBackendHost::SetPassphrase(const std::string& passphrase,
                                    bool is_explicit) {
  if (!IsNigoriEnabled()) {
    LOG(WARNING) << "Silently dropping SetPassphrase request.";
    return;
  }

  // If encryption is enabled and we've got a SetPassphrase
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(core_.get(), &SyncBackendHost::Core::DoSetPassphrase,
                        passphrase, is_explicit));
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
  // initialized.
  // Since we are blocking the UI thread here, we need to turn ourselves in
  // with the ThreadRestriction police.  For sentencing and how we plan to fix
  // this, see bug 19757.
  {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    core_thread_.Stop();
  }

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

syncable::AutofillMigrationState
    SyncBackendHost::GetAutofillMigrationState() {
  return core_->syncapi()->GetAutofillMigrationState();
}

void SyncBackendHost::SetAutofillMigrationState(
    syncable::AutofillMigrationState state) {
  return core_->syncapi()->SetAutofillMigrationState(state);
}

syncable::AutofillMigrationDebugInfo
    SyncBackendHost::GetAutofillMigrationDebugInfo() {
  return core_->syncapi()->GetAutofillMigrationDebugInfo();
}

void SyncBackendHost::SetAutofillMigrationDebugInfo(
    syncable::AutofillMigrationDebugInfo::PropertyToSet property_to_set,
    const syncable::AutofillMigrationDebugInfo& info) {
  return core_->syncapi()->SetAutofillMigrationDebugInfo(property_to_set, info);
}

void SyncBackendHost::ConfigureAutofillMigration() {
  if (GetAutofillMigrationState() == syncable::NOT_DETERMINED) {
    sync_api::ReadTransaction trans(GetUserShare());
    sync_api::ReadNode autofil_root_node(&trans);

    // Check for the presence of autofill node.
    if (!autofil_root_node.InitByTagLookup(browser_sync::kAutofillTag)) {
        SetAutofillMigrationState(syncable::INSUFFICIENT_INFO_TO_DETERMINE);
      return;
    }

    // Check for children under autofill node.
    if (autofil_root_node.GetFirstChildId() == static_cast<int64>(0)) {
      SetAutofillMigrationState(syncable::INSUFFICIENT_INFO_TO_DETERMINE);
      return;
    }

    sync_api::ReadNode autofill_profile_root_node(&trans);

    // Check for the presence of autofill profile root node.
    if (!autofill_profile_root_node.InitByTagLookup(
       browser_sync::kAutofillProfileTag)) {
      SetAutofillMigrationState(syncable::NOT_MIGRATED);
      return;
    }

    // If our state is not determined then we should not have the autofill
    // profile node.
    DCHECK(false);

    // just set it as not migrated.
    SetAutofillMigrationState(syncable::NOT_MIGRATED);
    return;
  }
}

void SyncBackendHost::ConfigureDataTypes(
    const DataTypeController::TypeMap& data_type_controllers,
    const syncable::ModelTypeSet& types,
    CancelableTask* ready_task) {
  // Only one configure is allowed at a time.
  DCHECK(!configure_ready_task_.get());
  DCHECK(syncapi_initialized_);

  if (types.count(syncable::AUTOFILL_PROFILE) != 0) {
    ConfigureAutofillMigration();
  }

  bool deleted_type = false;

  {
    base::AutoLock lock(registrar_lock_);
    for (DataTypeController::TypeMap::const_iterator it =
             data_type_controllers.begin();
         it != data_type_controllers.end(); ++it) {
      syncable::ModelType type = (*it).first;

      // If a type is not specified, remove it from the routing_info.
      if (types.count(type) == 0) {
        registrar_.routing_info.erase(type);
        deleted_type = true;
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
  } else {
    // Save the task here so we can run it when the syncer finishes
    // initializing the new data types.  It will be run only when the
    // set of initially synced data types matches the types requested in
    // this configure.
    configure_ready_task_.reset(ready_task);
    configure_initial_sync_types_ = types;
  }

  // Nudge the syncer. This is necessary for both datatype addition/deletion.
  //
  // Deletions need a nudge in order to ensure the deletion occurs in a timely
  // manner (see issue 56416).
  //
  // In the case of additions, on the next sync cycle, the syncer should
  // notice that the routing info has changed and start the process of
  // downloading updates for newly added data types.  Once this is
  // complete, the configure_ready_task_ is run via an
  // OnInitializationComplete notification.
  if (deleted_type || !core_->syncapi()->InitialSyncEndedForAllEnabledTypes())
    // We can only nudge when we've either deleted a dataype or added one, else
    // we break all the profile sync unit tests.
    RequestNudge();
}

void SyncBackendHost::RequestNudge() {
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(core_.get(), &SyncBackendHost::Core::DoRequestNudge));
}

void SyncBackendHost::ActivateDataType(
    DataTypeController* data_type_controller,
    ChangeProcessor* change_processor) {
  base::AutoLock lock(registrar_lock_);

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
  base::AutoLock lock(registrar_lock_);
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

bool SyncBackendHost::RequestClearServerData() {
  core_thread_.message_loop()->PostTask(FROM_HERE,
     NewRunnableMethod(core_.get(),
     &SyncBackendHost::Core::DoRequestClearServerData));
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

void SyncBackendHost::Core::NotifyPassphraseRequired(bool for_decryption) {
  if (!host_ || !host_->frontend_)
    return;

  DCHECK_EQ(MessageLoop::current(), host_->frontend_loop_);

  host_->frontend_->OnPassphraseRequired(for_decryption);
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
  NotificationService::current()->Notify(
      NotificationType::TOKEN_UPDATED,
      NotificationService::AllSources(),
      Details<const TokenAvailableDetails>(&details));
}

SyncBackendHost::Core::DoInitializeOptions::DoInitializeOptions(
    const GURL& service_url,
    sync_api::HttpPostProviderFactory* http_bridge_factory,
    const sync_api::SyncCredentials& credentials,
    bool delete_sync_data_folder,
    const notifier::NotifierOptions& notifier_options,
    std::string restored_key_for_bootstrapping,
    bool setup_for_test_mode)
    : service_url(service_url),
      http_bridge_factory(http_bridge_factory),
      credentials(credentials),
      delete_sync_data_folder(delete_sync_data_folder),
      notifier_options(notifier_options),
      restored_key_for_bootstrapping(restored_key_for_bootstrapping),
      setup_for_test_mode(setup_for_test_mode) {
}

SyncBackendHost::Core::DoInitializeOptions::~DoInitializeOptions() {}

sync_api::UserShare* SyncBackendHost::GetUserShare() const {
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
  base::AutoLock lock(registrar_lock_);
  out->clear();
  for (WorkerMap::const_iterator it = registrar_.workers.begin();
       it != registrar_.workers.end(); ++it) {
    out->push_back((*it).second);
  }
}

void SyncBackendHost::GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {
  base::AutoLock lock(registrar_lock_);
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
  if (options.delete_sync_data_folder) {
    DeleteSyncDataFolder();
  }

  // Make sure that the directory exists before initializing the backend.
  // If it already exists, this will do no harm.
  bool success = file_util::CreateDirectory(host_->sync_data_folder_path());
  DCHECK(success);

  syncapi_->SetObserver(this);
  const FilePath& path_str = host_->sync_data_folder_path();
  success = syncapi_->Init(
      path_str,
      (options.service_url.host() + options.service_url.path()).c_str(),
      options.service_url.EffectiveIntPort(),
      options.service_url.SchemeIsSecure(),
      options.http_bridge_factory,
      host_,  // ModelSafeWorkerRegistrar.
      MakeUserAgentForSyncapi().c_str(),
      options.credentials,
      options.notifier_options,
      options.restored_key_for_bootstrapping,
      options.setup_for_test_mode);
  DCHECK(success) << "Syncapi initialization failed!";
}

void SyncBackendHost::Core::DoUpdateCredentials(
    const SyncCredentials& credentials) {
  DCHECK(MessageLoop::current() == host_->core_thread_.message_loop());
  syncapi_->UpdateCredentials(credentials);
}

void SyncBackendHost::Core::DoStartSyncing() {
  DCHECK(MessageLoop::current() == host_->core_thread_.message_loop());
  syncapi_->StartSyncing();
}

void SyncBackendHost::Core::DoSetPassphrase(const std::string& passphrase,
                                            bool is_explicit) {
  DCHECK(MessageLoop::current() == host_->core_thread_.message_loop());
  syncapi_->SetPassphrase(passphrase, is_explicit);
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

ChangeProcessor* SyncBackendHost::Core::GetProcessor(
    syncable::ModelType model_type) {
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
    return NULL;

  if (!IsCurrentThreadSafeForModel(model_type)) {
    NOTREACHED() << "Changes applied on wrong thread.";
    return NULL;
  }

  // Now that we're sure we're on the correct thread, we can access the
  // ChangeProcessor.
  ChangeProcessor* processor = it->second;

  // Ensure the change processor is willing to accept changes.
  if (!processor->IsRunning())
    return NULL;

  return processor;
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
  ChangeProcessor* processor = GetProcessor(model_type);
  if (!processor)
    return;

  processor->ApplyChangesFromSyncModel(trans, changes, change_count);
}

void SyncBackendHost::Core::OnChangesComplete(
    syncable::ModelType model_type) {
  if (!host_ || !host_->frontend_) {
    DCHECK(false) << "OnChangesComplete called after Shutdown?";
    return;
  }

  ChangeProcessor* processor = GetProcessor(model_type);
  if (!processor)
    return;

  // This call just notifies the processor that it can commit, it already
  // buffered any changes it plans to makes so needs no further information.
  processor->CommitChangesFromSyncModel();
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
  base::AutoLock lock(host_->registrar_lock_);

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
  // Post to our core loop so we can modify state. Could be on another thread.
  host_->frontend_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &Core::HandleAuthErrorEventOnFrontendLoop,
      auth_error));
}

void SyncBackendHost::Core::OnPassphraseRequired(bool for_decryption) {
  host_->frontend_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &Core::NotifyPassphraseRequired, for_decryption));
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

void SyncBackendHost::Core::OnUpdatedToken(const std::string& token) {
  host_->frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &Core::NotifyUpdatedToken, token));
}

void SyncBackendHost::Core::OnClearServerDataSucceeded() {
  host_->frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &Core::HandleClearServerDataSucceededOnFrontendLoop));
}

void SyncBackendHost::Core::OnClearServerDataFailed() {
  host_->frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &Core::HandleClearServerDataFailedOnFrontendLoop));
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

void SyncBackendHost::Core::DoRequestClearServerData() {
  syncapi_->RequestClearServerData();
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
