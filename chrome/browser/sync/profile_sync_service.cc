// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service.h"

#include <cstddef>
#include <map>
#include <set>
#include <utility>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/backend_migrator.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/chrome_encryptor.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/session_data_type_controller.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/sync_global_error.h"
#include "chrome/browser/sync/user_selectable_sync_type.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "net/cookies/cookie_monster.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/configure_reason.h"
#include "sync/js/js_arg_list.h"
#include "sync/js/js_event_details.h"
#include "sync/util/cryptographer.h"
#include "sync/util/experiments.h"
#include "ui/base/l10n/l10n_util.h"

using browser_sync::ChangeProcessor;
using browser_sync::DataTypeController;
using browser_sync::DataTypeManager;
using browser_sync::JsBackend;
using browser_sync::JsController;
using browser_sync::JsEventDetails;
using browser_sync::JsEventHandler;
using browser_sync::SyncBackendHost;
using browser_sync::SyncProtocolError;
using browser_sync::WeakHandle;
using sync_api::SyncCredentials;

typedef GoogleServiceAuthError AuthError;

const char* ProfileSyncService::kSyncServerUrl =
    "https://clients4.google.com/chrome-sync";

const char* ProfileSyncService::kDevServerUrl =
    "https://clients4.google.com/chrome-sync/dev";

static const int kSyncClearDataTimeoutInSeconds = 60;  // 1 minute.

static const char* kRelevantTokenServices[] = {
    GaiaConstants::kSyncService
};
static const int kRelevantTokenServicesCount =
    arraysize(kRelevantTokenServices);

// Helper to check if the given token service is relevant for sync.
static bool IsTokenServiceRelevant(const std::string& service) {
  for (int i = 0; i < kRelevantTokenServicesCount; ++i) {
    if (service == kRelevantTokenServices[i])
      return true;
  }
  return false;
}

bool ShouldShowActionOnUI(
    const browser_sync::SyncProtocolError& error) {
  return (error.action != browser_sync::UNKNOWN_ACTION &&
          error.action != browser_sync::DISABLE_SYNC_ON_CLIENT);
}

ProfileSyncService::ProfileSyncService(ProfileSyncComponentsFactory* factory,
                                       Profile* profile,
                                       SigninManager* signin_manager,
                                       StartBehavior start_behavior)
    : last_auth_error_(AuthError::None()),
      passphrase_required_reason_(sync_api::REASON_PASSPHRASE_NOT_REQUIRED),
      factory_(factory),
      profile_(profile),
      // |profile| may be NULL in unit tests.
      sync_prefs_(profile_ ? profile_->GetPrefs() : NULL),
      sync_service_url_(kDevServerUrl),
      backend_initialized_(false),
      is_auth_in_progress_(false),
      signin_(signin_manager),
      unrecoverable_error_detected_(false),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      expect_sync_configuration_aborted_(false),
      clear_server_data_state_(CLEAR_NOT_STARTED),
      encrypted_types_(browser_sync::Cryptographer::SensitiveTypes()),
      encrypt_everything_(false),
      encryption_pending_(false),
      auto_start_enabled_(start_behavior == AUTO_START),
      failed_datatypes_handler_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      configure_status_(DataTypeManager::UNKNOWN),
      setup_in_progress_(false) {
#if defined(OS_ANDROID)
  chrome::VersionInfo version_info;
  if (version_info.IsOfficialBuild()) {
    sync_service_url_ = GURL(kSyncServerUrl);
  }
#else
  // By default, dev, canary, and unbranded Chromium users will go to the
  // development servers. Development servers have more features than standard
  // sync servers. Users with officially-branded Chrome stable and beta builds
  // will go to the standard sync servers.
  //
  // GetChannel hits the registry on Windows. See http://crbug.com/70380.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    sync_service_url_ = GURL(kSyncServerUrl);
  }
#endif
}

ProfileSyncService::~ProfileSyncService() {
  sync_prefs_.RemoveSyncPrefObserver(this);
  Shutdown();
}

bool ProfileSyncService::AreCredentialsAvailable() {
  if (IsManaged()) {
    return false;
  }

  // If we have start suppressed, then basically just act like we have no
  // credentials (login is required to fix this, since we need the user's
  // passphrase to encrypt/decrypt anyway).
  // TODO(sync): Revisit this when we move to a server-based keystore.
  if (sync_prefs_.IsStartSuppressed())
    return false;

  // CrOS user is always logged in. Chrome uses signin_ to check logged in.
  if (signin_->GetAuthenticatedUsername().empty())
    return false;

  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  if (!token_service)
    return false;

  // TODO(chron): Verify CrOS unit test behavior.
  return token_service->HasTokenForService(GaiaConstants::kSyncService);
}

void ProfileSyncService::Initialize() {
  InitSettings();

  // We clear this here (vs Shutdown) because we want to remember that an error
  // happened on shutdown so we can display details (message, location) about it
  // in about:sync.
  ClearStaleErrors();

  sync_prefs_.AddSyncPrefObserver(this);

  // For now, the only thing we can do through policy is to turn sync off.
  if (IsManaged()) {
    DisableForUser();
    return;
  }

  RegisterAuthNotifications();

  if (!HasSyncSetupCompleted())
    DisableForUser();  // Clean up in case of previous crash / setup abort.

  TryStart();
}

void ProfileSyncService::TryStart() {
  if (!sync_prefs_.IsStartSuppressed() && AreCredentialsAvailable()) {
    if (HasSyncSetupCompleted() || auto_start_enabled_) {
      // If sync setup has completed we always start the backend.
      // If autostart is enabled, but we haven't completed sync setup, we try to
      // start sync anyway, since it's possible we crashed/shutdown after
      // logging in but before the backend finished initializing the last time.
      // Note that if we haven't finished setting up sync, backend bring up will
      // be done by the wizard.
      StartUp();
    }
  } else if (HasSyncSetupCompleted()) {
    TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
    if (token_service && token_service->TokensLoadedFromDB() &&
        !AreCredentialsAvailable()) {
      // The token service has lost sync's tokens. We cannot recover from this
      // without signing back in, which is not yet supported. For now, we
      // trigger an unrecoverable error.
      OnUnrecoverableError(FROM_HERE, "Sync credentials lost.");
    }
  }
}

void ProfileSyncService::StartSyncingWithServer() {
  if (backend_.get())
    backend_->StartSyncingWithServer();
}

void ProfileSyncService::RegisterAuthNotifications() {
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 content::Source<TokenService>(token_service));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_LOADING_FINISHED,
                 content::Source<TokenService>(token_service));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                 content::Source<TokenService>(token_service));
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
                 content::Source<Profile>(profile_));
}

void ProfileSyncService::RegisterDataTypeController(
    DataTypeController* data_type_controller) {
  DCHECK_EQ(data_type_controllers_.count(data_type_controller->type()), 0U);
  data_type_controllers_[data_type_controller->type()] =
      data_type_controller;
}

browser_sync::SessionModelAssociator*
    ProfileSyncService::GetSessionModelAssociator() {
  if (data_type_controllers_.find(syncable::SESSIONS) ==
      data_type_controllers_.end() ||
      data_type_controllers_.find(syncable::SESSIONS)->second->state() !=
      DataTypeController::RUNNING) {
    return NULL;
  }
  return static_cast<browser_sync::SessionDataTypeController*>(
      data_type_controllers_.find(
      syncable::SESSIONS)->second.get())->GetModelAssociator();
}

void ProfileSyncService::ResetClearServerDataState() {
  clear_server_data_state_ = CLEAR_NOT_STARTED;
}

ProfileSyncService::ClearServerDataState
    ProfileSyncService::GetClearServerDataState() {
  return clear_server_data_state_;
}

void ProfileSyncService::GetDataTypeControllerStates(
  browser_sync::DataTypeController::StateMap* state_map) const {
    for (browser_sync::DataTypeController::TypeMap::const_iterator iter =
         data_type_controllers_.begin(); iter != data_type_controllers_.end();
         ++iter)
      (*state_map)[iter->first] = iter->second.get()->state();
}

void ProfileSyncService::InitSettings() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Override the sync server URL from the command-line, if sync server
  // command-line argument exists.
  if (command_line.HasSwitch(switches::kSyncServiceURL)) {
    std::string value(command_line.GetSwitchValueASCII(
        switches::kSyncServiceURL));
    if (!value.empty()) {
      GURL custom_sync_url(value);
      if (custom_sync_url.is_valid()) {
        sync_service_url_ = custom_sync_url;
      } else {
        LOG(WARNING) << "The following sync URL specified at the command-line "
                     << "is invalid: " << value;
      }
    }
  }
}

SyncCredentials ProfileSyncService::GetCredentials() {
  SyncCredentials credentials;
  credentials.email = signin_->GetAuthenticatedUsername();
  DCHECK(!credentials.email.empty());
  TokenService* service = TokenServiceFactory::GetForProfile(profile_);
  credentials.sync_token = service->GetTokenForService(
      GaiaConstants::kSyncService);
  return credentials;
}

void ProfileSyncService::InitializeBackend(bool delete_stale_data) {
  if (!backend_.get()) {
    NOTREACHED();
    return;
  }

  syncable::ModelTypeSet initial_types;
  // If sync setup hasn't finished, we don't want to initialize routing info
  // for any data types so that we don't download updates for types that the
  // user chooses not to sync on the first DownloadUpdatesCommand.
  if (HasSyncSetupCompleted()) {
    initial_types = GetPreferredDataTypes();
  }

  SyncCredentials credentials = GetCredentials();

  scoped_refptr<net::URLRequestContextGetter> request_context_getter(
      profile_->GetRequestContext());

  if (delete_stale_data)
    ClearStaleErrors();

  backend_unrecoverable_error_handler_.reset(
    new browser_sync::BackendUnrecoverableErrorHandler(
        MakeWeakHandle(AsWeakPtr())));

  backend_->Initialize(
      this,
      MakeWeakHandle(sync_js_controller_.AsWeakPtr()),
      sync_service_url_,
      initial_types,
      credentials,
      delete_stale_data,
      backend_unrecoverable_error_handler_.get(),
      &browser_sync::ChromeReportUnrecoverableError);
}

void ProfileSyncService::CreateBackend() {
  backend_.reset(
      new SyncBackendHost(profile_->GetDebugName(),
                          profile_, sync_prefs_.AsWeakPtr()));
}

bool ProfileSyncService::IsEncryptedDatatypeEnabled() const {
  if (encryption_pending())
    return true;
  const syncable::ModelTypeSet preferred_types = GetPreferredDataTypes();
  const syncable::ModelTypeSet encrypted_types = GetEncryptedDataTypes();
  DCHECK(encrypted_types.Has(syncable::PASSWORDS));
  return !Intersection(preferred_types, encrypted_types).Empty();
}

void ProfileSyncService::OnSyncConfigureDone(
    DataTypeManager::ConfigureResult result) {
  if (failed_datatypes_handler_.UpdateFailedDatatypes(result.errors,
          FailedDatatypesHandler::STARTUP)) {
    ReconfigureDatatypeManager();
  }
}

void ProfileSyncService::OnSyncConfigureRetry() {
  // In platforms with auto start we would just wait for the
  // configure to finish. In other platforms we would throw
  // an unrecoverable error. The reason we do this is so that
  // the login dialog would show an error and the user would have
  // to relogin.
  // Also if backend has been initialized(the user is authenticated
  // and nigori is downloaded) we would simply wait rather than going into
  // unrecoverable error, even if the platform has auto start disabled.
  // Note: In those scenarios the UI does not wait for the configuration
  // to finish.
  if (!auto_start_enabled_ && !backend_initialized_) {
    OnUnrecoverableError(FROM_HERE,
                         "Configure failed to download.");
  }

  NotifyObservers();
}


void ProfileSyncService::StartUp() {
  // Don't start up multiple times.
  if (backend_.get()) {
    DVLOG(1) << "Skipping bringing up backend host.";
    return;
  }

  DCHECK(AreCredentialsAvailable());

  last_synced_time_ = sync_prefs_.GetLastSyncedTime();

#if defined(OS_CHROMEOS)
  std::string bootstrap_token = sync_prefs_.GetEncryptionBootstrapToken();
  if (bootstrap_token.empty()) {
    sync_prefs_.SetEncryptionBootstrapToken(
        sync_prefs_.GetSpareBootstrapToken());
  }
#endif
  CreateBackend();

  // Initialize the backend.  Every time we start up a new SyncBackendHost,
  // we'll want to start from a fresh SyncDB, so delete any old one that might
  // be there.
  InitializeBackend(!HasSyncSetupCompleted());

  if (!sync_global_error_.get()) {
    sync_global_error_.reset(new SyncGlobalError(this, signin()));
    GlobalErrorServiceFactory::GetForProfile(profile_)->AddGlobalError(
        sync_global_error_.get());
    AddObserver(sync_global_error_.get());
  }
}

void ProfileSyncService::Shutdown() {
  ShutdownImpl(false);
}

void ProfileSyncService::ShutdownImpl(bool sync_disabled) {
  // First, we spin down the backend and wait for it to stop syncing completely
  // before we Stop the data type manager.  This is to avoid a late sync cycle
  // applying changes to the sync db that wouldn't get applied via
  // ChangeProcessors, leading to back-from-the-dead bugs.
  base::Time shutdown_start_time = base::Time::Now();
  if (backend_.get())
    backend_->StopSyncingForShutdown();

  // Stop all data type controllers, if needed.  Note that until Stop
  // completes, it is possible in theory to have a ChangeProcessor apply a
  // change from a native model.  In that case, it will get applied to the sync
  // database (which doesn't get destroyed until we destroy the backend below)
  // as an unsynced change.  That will be persisted, and committed on restart.
  if (data_type_manager_.get()) {
    if (data_type_manager_->state() != DataTypeManager::STOPPED) {
      // When aborting as part of shutdown, we should expect an aborted sync
      // configure result, else we'll dcheck when we try to read the sync error.
      expect_sync_configuration_aborted_ = true;
      data_type_manager_->Stop();
    }

    registrar_.Remove(
        this,
        chrome::NOTIFICATION_SYNC_CONFIGURE_START,
        content::Source<DataTypeManager>(data_type_manager_.get()));
    registrar_.Remove(
        this,
        chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
        content::Source<DataTypeManager>(data_type_manager_.get()));
    registrar_.Remove(
        this,
        chrome::NOTIFICATION_SYNC_CONFIGURE_BLOCKED,
        content::Source<DataTypeManager>(data_type_manager_.get()));
    data_type_manager_.reset();
  }

  // Shutdown the migrator before the backend to ensure it doesn't pull a null
  // snapshot.
  migrator_.reset();
  sync_js_controller_.AttachJsBackend(WeakHandle<JsBackend>());

  // Move aside the backend so nobody else tries to use it while we are
  // shutting it down.
  scoped_ptr<SyncBackendHost> doomed_backend(backend_.release());
  if (doomed_backend.get()) {
    doomed_backend->Shutdown(sync_disabled);

    doomed_backend.reset();
  }
  base::TimeDelta shutdown_time = base::Time::Now() - shutdown_start_time;
  UMA_HISTOGRAM_TIMES("Sync.Shutdown.BackendDestroyedTime", shutdown_time);

  weak_factory_.InvalidateWeakPtrs();

  // Clear various flags.
  expect_sync_configuration_aborted_ = false;
  is_auth_in_progress_ = false;
  backend_initialized_ = false;
  cached_passphrase_.clear();
  encryption_pending_ = false;
  encrypt_everything_ = false;
  encrypted_types_ = browser_sync::Cryptographer::SensitiveTypes();
  passphrase_required_reason_ = sync_api::REASON_PASSPHRASE_NOT_REQUIRED;
  last_auth_error_ = GoogleServiceAuthError::None();

  if (sync_global_error_.get()) {
    GlobalErrorServiceFactory::GetForProfile(profile_)->RemoveGlobalError(
        sync_global_error_.get());
    RemoveObserver(sync_global_error_.get());
    sync_global_error_.reset(NULL);
  }
}

void ProfileSyncService::ClearServerData() {
  clear_server_data_state_ = CLEAR_CLEARING;
  clear_server_data_timer_.Start(FROM_HERE,
      base::TimeDelta::FromSeconds(kSyncClearDataTimeoutInSeconds), this,
      &ProfileSyncService::OnClearServerDataTimeout);
  backend_->RequestClearServerData();
}

void ProfileSyncService::DisableForUser() {
  // Clear prefs (including SyncSetupHasCompleted) before shutting down so
  // PSS clients don't think we're set up while we're shutting down.
  sync_prefs_.ClearPreferences();
  ClearUnrecoverableError();
  ShutdownImpl(true);

  // TODO(atwilson): Don't call SignOut() on *any* platform - move this into
  // the UI layer if needed (sync activity should never result in the user
  // being logged out of all chrome services).
  if (!auto_start_enabled_)
    signin_->SignOut();

  NotifyObservers();
}

bool ProfileSyncService::HasSyncSetupCompleted() const {
  return sync_prefs_.HasSyncSetupCompleted();
}

void ProfileSyncService::SetSyncSetupCompleted() {
  sync_prefs_.SetSyncSetupCompleted();
}

void ProfileSyncService::UpdateLastSyncedTime() {
  last_synced_time_ = base::Time::Now();
  sync_prefs_.SetLastSyncedTime(last_synced_time_);
}

void ProfileSyncService::NotifyObservers() {
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
  // TODO(akalin): Make an Observer subclass that listens and does the
  // event routing.
  sync_js_controller_.HandleJsEvent(
      "onServiceStateChanged", JsEventDetails());
}

void ProfileSyncService::ClearStaleErrors() {
  ClearUnrecoverableError();
  last_actionable_error_ = SyncProtocolError();
}

void ProfileSyncService::ClearUnrecoverableError() {
  unrecoverable_error_detected_ = false;
  unrecoverable_error_message_.clear();
  unrecoverable_error_location_ = tracked_objects::Location();
}

// static
std::string ProfileSyncService::GetExperimentNameForDataType(
    syncable::ModelType data_type) {
  switch (data_type) {
    case syncable::SESSIONS:
      return "sync-tabs";
    default:
      break;
  }
  NOTREACHED();
  return "";
}

void ProfileSyncService::RegisterNewDataType(syncable::ModelType data_type) {
  if (data_type_controllers_.count(data_type) > 0)
    return;
  switch (data_type) {
    case syncable::SESSIONS:
      if (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableSyncTabs)) {
        return;
      }
      RegisterDataTypeController(
          new browser_sync::SessionDataTypeController(factory_.get(),
                                                      profile_,
                                                      this));
      return;
    default:
      break;
  }
  NOTREACHED();
}

// An invariant has been violated.  Transition to an error state where we try
// to do as little work as possible, to avoid further corruption or crashes.
void ProfileSyncService::OnUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  OnUnrecoverableErrorImpl(from_here, message, true);
}

void ProfileSyncService::OnUnrecoverableErrorImpl(
    const tracked_objects::Location& from_here,
    const std::string& message,
    bool delete_sync_database) {
  unrecoverable_error_detected_ = true;
  unrecoverable_error_message_ = message;
  unrecoverable_error_location_ = from_here;

  NotifyObservers();
  std::string location;
  from_here.Write(true, true, &location);
  LOG(ERROR)
      << "Unrecoverable error detected at " << location
      << " -- ProfileSyncService unusable: " << message;

  // Shut all data types down.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&ProfileSyncService::ShutdownImpl, weak_factory_.GetWeakPtr(),
                 delete_sync_database));
}

void ProfileSyncService::OnDisableDatatype(
    syncable::ModelType type,
    const tracked_objects::Location& from_here,
    std::string message) {
  // First deactivate the type so that no further server changes are
  // passed onto the change processor.
  DeactivateDataType(type);

  SyncError error(from_here, message, type);

  std::list<SyncError> errors;
  errors.push_back(error);

  // Update this before posting a task. So if a configure happens before
  // the task that we are going to post, this type would still be disabled.
  failed_datatypes_handler_.UpdateFailedDatatypes(errors,
      FailedDatatypesHandler::RUNTIME);

  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&ProfileSyncService::ReconfigureDatatypeManager,
                 weak_factory_.GetWeakPtr()));
}

void ProfileSyncService::OnBackendInitialized(
    const WeakHandle<JsBackend>& js_backend, bool success) {
  if (!HasSyncSetupCompleted()) {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeFirstTimeSuccess", success);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeRestoreSuccess", success);
  }

  if (!success) {
    // Something went unexpectedly wrong.  Play it safe: stop syncing at once
    // and surface error UI to alert the user sync has stopped.
    // Keep the directory around for now so that on restart we will retry
    // again and potentially succeed in presence of transient file IO failures
    // or permissions issues, etc.
    OnUnrecoverableErrorImpl(FROM_HERE, "BackendInitialize failure", false);
    return;
  }

  backend_initialized_ = true;

  sync_js_controller_.AttachJsBackend(js_backend);

  // If we have a cached passphrase use it to decrypt/encrypt data now that the
  // backend is initialized. We want to call this before notifying observers in
  // case this operation affects the "passphrase required" status.
  ConsumeCachedPassphraseIfPossible();

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  last_synced_time_ will only be null
  // in this case, because the pref wasn't restored on StartUp.
  if (last_synced_time_.is_null()) {
    UpdateLastSyncedTime();
  }
  NotifyObservers();

  if (auto_start_enabled_ && !FirstSetupInProgress()) {
    // Backend is initialized but we're not in sync setup, so this must be an
    // autostart - mark our sync setup as completed and we'll start syncing
    // below.
    SetSyncSetupCompleted();
    NotifyObservers();
  }

  if (HasSyncSetupCompleted()) {
    ConfigureDataTypeManager();
  } else {
    DCHECK(FirstSetupInProgress());
  }
}

void ProfileSyncService::OnSyncCycleCompleted() {
  UpdateLastSyncedTime();
  if (GetSessionModelAssociator()) {
    // Trigger garbage collection of old sessions now that we've downloaded
    // any new session data. TODO(zea): Have this be a notification the session
    // model associator listens too. Also consider somehow plumbing the current
    // server time as last reported by CheckServerReachable, so we don't have to
    // rely on the local clock, which may be off significantly.
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&browser_sync::SessionModelAssociator::DeleteStaleSessions,
                   GetSessionModelAssociator()->AsWeakPtr()));
  }
  DVLOG(2) << "Notifying observers sync cycle completed";
  NotifyObservers();
}

void ProfileSyncService::OnExperimentsChanged(
    const browser_sync::Experiments& experiments) {
  if (current_experiments.Matches(experiments))
    return;

  // If this is a first time sync for a client, this will be called before
  // OnBackendInitialized() to ensure the new datatypes are available at sync
  // setup. As a result, the migrator won't exist yet. This is fine because for
  // first time sync cases we're only concerned with making the datatype
  // available.
  if (migrator_.get() &&
      migrator_->state() != browser_sync::BackendMigrator::IDLE) {
    DVLOG(1) << "Dropping OnExperimentsChanged due to migrator busy.";
    return;
  }

  const syncable::ModelTypeSet registered_types = GetRegisteredDataTypes();
  syncable::ModelTypeSet to_add;
  if (experiments.sync_tabs)
    to_add.Put(syncable::SESSIONS);
  const syncable::ModelTypeSet to_register =
      Difference(to_add, registered_types);
  DVLOG(2) << "OnExperimentsChanged called with types: "
           << syncable::ModelTypeSetToString(to_add);
  DVLOG(2) << "Enabling types: " << syncable::ModelTypeSetToString(to_register);

  for (syncable::ModelTypeSet::Iterator it = to_register.First();
       it.Good(); it.Inc()) {
    // Received notice to enable experimental type. Check if the type is
    // registered, and if not register a new datatype controller.
    RegisterNewDataType(it.Get());
#if !defined(OS_ANDROID)
    // Enable the about:flags switch for the experimental type so we don't have
    // to always perform this reconfiguration. Once we set this, the type will
    // remain registered on restart, so we will no longer go down this code
    // path.
    std::string experiment_name = GetExperimentNameForDataType(it.Get());
    if (experiment_name.empty())
      continue;
    about_flags::SetExperimentEnabled(g_browser_process->local_state(),
                                      experiment_name,
                                      true);
#endif  // !defined(OS_ANDROID)
  }

  // Check if the user has "Keep Everything Synced" enabled. If so, we want
  // to turn on all experimental types if they're not already on. Otherwise we
  // leave them off.
  // Note: if any types are already registered, we don't turn them on. This
  // covers the case where we're already in the process of reconfiguring
  // to turn an experimental type on.
  if (sync_prefs_.HasKeepEverythingSynced()) {
    // Mark all data types as preferred.
    sync_prefs_.SetPreferredDataTypes(registered_types, registered_types);

    // Only automatically turn on types if we have already finished set up.
    // Otherwise, just leave the experimental types on by default.
    if (!to_register.Empty() && HasSyncSetupCompleted() && migrator_.get()) {
      DVLOG(1) << "Dynamically enabling new datatypes: "
               << syncable::ModelTypeSetToString(to_register);
      OnMigrationNeededForTypes(to_register);
    }
  }

  // Now enable any non-datatype features.
  if (experiments.sync_tab_favicons) {
    DVLOG(1) << "Enabling syncing of tab favicons.";
    about_flags::SetExperimentEnabled(g_browser_process->local_state(),
                                      "sync-tab-favicons",
                                      true);
  }

  current_experiments = experiments;
}

void ProfileSyncService::UpdateAuthErrorState(
    const GoogleServiceAuthError& error) {
  is_auth_in_progress_ = false;
  last_auth_error_ = error;

  // Fan the notification out to interested UI-thread components.
  NotifyObservers();
}

namespace {

GoogleServiceAuthError ConnectionStatusToAuthError(
    sync_api::ConnectionStatus status) {
  switch (status) {
    case sync_api::CONNECTION_OK:
      return GoogleServiceAuthError::None();
      break;
    case sync_api::CONNECTION_AUTH_ERROR:
      return GoogleServiceAuthError(
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
      break;
    case sync_api::CONNECTION_SERVER_ERROR:
      return GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED);
      break;
    default:
      NOTREACHED();
      return GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED);
  }
}

}  // namespace

void ProfileSyncService::OnConnectionStatusChange(
    sync_api::ConnectionStatus status) {
  UpdateAuthErrorState(ConnectionStatusToAuthError(status));
}

void ProfileSyncService::OnStopSyncingPermanently() {
  UpdateAuthErrorState(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  sync_prefs_.SetStartSuppressed(true);
  DisableForUser();
}

void ProfileSyncService::OnClearServerDataTimeout() {
  if (clear_server_data_state_ != CLEAR_SUCCEEDED &&
      clear_server_data_state_ != CLEAR_FAILED) {
    clear_server_data_state_ = CLEAR_FAILED;
    NotifyObservers();
  }
}

void ProfileSyncService::OnClearServerDataFailed() {
  clear_server_data_timer_.Stop();

  // Only once clear has succeeded there is no longer a need to transition to
  // a failed state as sync is disabled locally.  Also, no need to fire off
  // the observers if the state didn't change (i.e. it was FAILED before).
  if (clear_server_data_state_ != CLEAR_SUCCEEDED &&
      clear_server_data_state_ != CLEAR_FAILED) {
    clear_server_data_state_ = CLEAR_FAILED;
    NotifyObservers();
  }
}

void ProfileSyncService::OnClearServerDataSucceeded() {
  clear_server_data_timer_.Stop();

  // Even if the timout fired, we still transition to the succeeded state as
  // we want UI to update itself and no longer allow the user to press "clear"
  if (clear_server_data_state_ != CLEAR_SUCCEEDED) {
    clear_server_data_state_ = CLEAR_SUCCEEDED;
    NotifyObservers();
  }
}

void ProfileSyncService::OnPassphraseRequired(
    sync_api::PassphraseRequiredReason reason,
    const sync_pb::EncryptedData& pending_keys) {
  DCHECK(backend_.get());
  DCHECK(backend_->IsNigoriEnabled());

  // TODO(lipalani) : add this check to other locations as well.
  if (unrecoverable_error_detected_) {
    // When unrecoverable error is detected we post a task to shutdown the
    // backend. The task might not have executed yet.
    return;
  }

  DVLOG(1) << "Passphrase required with reason: "
           << sync_api::PassphraseRequiredReasonToString(reason);
  passphrase_required_reason_ = reason;

  // Notify observers that the passphrase status may have changed.
  NotifyObservers();
}

void ProfileSyncService::OnPassphraseAccepted() {
  DVLOG(1) << "Received OnPassphraseAccepted.";
  // If we are not using an explicit passphrase, and we have a cache of the gaia
  // password, use it for encryption at this point.
  DCHECK(cached_passphrase_.empty()) <<
      "Passphrase no longer required but there is still a cached passphrase";

  // Reset passphrase_required_reason_ since we know we no longer require the
  // passphrase. We do this here rather than down in ResolvePassphraseRequired()
  // because that can be called by OnPassphraseRequired() if no encrypted data
  // types are enabled, and we don't want to clobber the true passphrase error.
  passphrase_required_reason_ = sync_api::REASON_PASSPHRASE_NOT_REQUIRED;

  // Make sure the data types that depend on the passphrase are started at
  // this time.
  const syncable::ModelTypeSet types = GetPreferredDataTypes();

  if (data_type_manager_.get()) {
    // Unblock the data type manager if necessary.
    data_type_manager_->Configure(types,
                                  sync_api::CONFIGURE_REASON_RECONFIGURATION);
  }

  NotifyObservers();
}

void ProfileSyncService::OnEncryptedTypesChanged(
    syncable::ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  encrypted_types_ = encrypted_types;
  encrypt_everything_ = encrypt_everything;
  DVLOG(1) << "Encrypted types changed to "
           << syncable::ModelTypeSetToString(encrypted_types_)
           << " (encrypt everything is set to "
           << (encrypt_everything_ ? "true" : "false") << ")";
  DCHECK(encrypted_types_.Has(syncable::PASSWORDS));
}

void ProfileSyncService::OnEncryptionComplete() {
  DVLOG(1) << "Encryption complete";
  if (encryption_pending_ && encrypt_everything_) {
    encryption_pending_ = false;
    // This is to nudge the integration tests when encryption is
    // finished.
    NotifyObservers();
  }
}

void ProfileSyncService::OnMigrationNeededForTypes(
    syncable::ModelTypeSet types) {
  DCHECK(backend_initialized_);
  DCHECK(data_type_manager_.get());

  // Migrator must be valid, because we don't sync until it is created and this
  // callback originates from a sync cycle.
  migrator_->MigrateTypes(types);
}

void ProfileSyncService::OnActionableError(const SyncProtocolError& error) {
  last_actionable_error_ = error;
  DCHECK_NE(last_actionable_error_.action,
            browser_sync::UNKNOWN_ACTION);
  switch (error.action) {
    case browser_sync::UPGRADE_CLIENT:
    case browser_sync::CLEAR_USER_DATA_AND_RESYNC:
    case browser_sync::ENABLE_SYNC_ON_ACCOUNT:
    case browser_sync::STOP_AND_RESTART_SYNC:
      // TODO(lipalani) : if setup in progress we want to display these
      // actions in the popup. The current experience might not be optimal for
      // the user. We just dismiss the dialog.
      if (setup_in_progress_) {
        OnStopSyncingPermanently();
        expect_sync_configuration_aborted_ = true;
      }
      // Trigger an unrecoverable error to stop syncing.
      OnUnrecoverableError(FROM_HERE, last_actionable_error_.error_description);
      break;
    case browser_sync::DISABLE_SYNC_ON_CLIENT:
      OnStopSyncingPermanently();
      break;
    default:
      NOTREACHED();
  }
  NotifyObservers();
}

std::string ProfileSyncService::QuerySyncStatusSummary() {
  if (unrecoverable_error_detected_) {
    return "Unrecoverable error detected";
  } else if (!backend_.get()) {
    return "Syncing not enabled";
  } else if (backend_.get() && !HasSyncSetupCompleted()) {
    return "First time sync setup incomplete";
  } else if (backend_.get() && HasSyncSetupCompleted() &&
             data_type_manager_.get() &&
             data_type_manager_->state() != DataTypeManager::CONFIGURED) {
    return "Datatypes not fully initialized";
  } else if (ShouldPushChanges()) {
    return "Sync service initialized";
  } else {
    return "Status unknown: Internal error?";
  }
}

SyncBackendHost::Status ProfileSyncService::QueryDetailedSyncStatus() {
  if (backend_.get() && backend_initialized_) {
    return backend_->GetDetailedStatus();
  } else {
    SyncBackendHost::Status status;
    status.sync_protocol_error = last_actionable_error_;
    return status;
  }
}

const GoogleServiceAuthError& ProfileSyncService::GetAuthError() const {
  return last_auth_error_;
}

bool ProfileSyncService::FirstSetupInProgress() const {
  return !HasSyncSetupCompleted() && setup_in_progress_;
}

bool ProfileSyncService::sync_initialized() const {
  return backend_initialized_;
}

bool ProfileSyncService::waiting_for_auth() const {
  return is_auth_in_progress_;
}

bool ProfileSyncService::unrecoverable_error_detected() const {
  return unrecoverable_error_detected_;
}

bool ProfileSyncService::IsPassphraseRequired() const {
  return passphrase_required_reason_ !=
      sync_api::REASON_PASSPHRASE_NOT_REQUIRED;
}

// TODO(zea): Rename this IsPassphraseNeededFromUI and ensure it's used
// appropriately (see http://crbug.com/91379).
bool ProfileSyncService::IsPassphraseRequiredForDecryption() const {
  // If there is an encrypted datatype enabled and we don't have the proper
  // passphrase, we must prompt the user for a passphrase. The only way for the
  // user to avoid entering their passphrase is to disable the encrypted types.
  return IsEncryptedDatatypeEnabled() && IsPassphraseRequired();
}

string16 ProfileSyncService::GetLastSyncedTimeString() const {
  if (last_synced_time_.is_null())
    return l10n_util::GetStringUTF16(IDS_SYNC_TIME_NEVER);

  base::TimeDelta last_synced = base::Time::Now() - last_synced_time_;

  if (last_synced < base::TimeDelta::FromMinutes(1))
    return l10n_util::GetStringUTF16(IDS_SYNC_TIME_JUST_NOW);

  return TimeFormat::TimeElapsed(last_synced);
}

void ProfileSyncService::UpdateSelectedTypesHistogram(
    bool sync_everything, const syncable::ModelTypeSet chosen_types) const {
  if (!HasSyncSetupCompleted() ||
      sync_everything != sync_prefs_.HasKeepEverythingSynced()) {
    UMA_HISTOGRAM_BOOLEAN("Sync.SyncEverything", sync_everything);
  }

  // Only log the data types that are shown in the sync settings ui.
  const syncable::ModelType model_types[] = {
    syncable::APPS,
    syncable::AUTOFILL,
    syncable::BOOKMARKS,
    syncable::EXTENSIONS,
    syncable::PASSWORDS,
    syncable::PREFERENCES,
    syncable::SESSIONS,
    syncable::THEMES,
    syncable::TYPED_URLS
  };

  const browser_sync::user_selectable_type::UserSelectableSyncType
      user_selectable_types[] = {
    browser_sync::user_selectable_type::APPS,
    browser_sync::user_selectable_type::AUTOFILL,
    browser_sync::user_selectable_type::BOOKMARKS,
    browser_sync::user_selectable_type::EXTENSIONS,
    browser_sync::user_selectable_type::PASSWORDS,
    browser_sync::user_selectable_type::PREFERENCES,
    browser_sync::user_selectable_type::SESSIONS,
    browser_sync::user_selectable_type::THEMES,
    browser_sync::user_selectable_type::TYPED_URLS
  };

  COMPILE_ASSERT(17 == syncable::MODEL_TYPE_COUNT,
                 UpdateCustomConfigHistogram);
  COMPILE_ASSERT(arraysize(model_types) ==
                 browser_sync::user_selectable_type::SELECTABLE_DATATYPE_COUNT,
                 UpdateCustomConfigHistogram);
  COMPILE_ASSERT(arraysize(model_types) == arraysize(user_selectable_types),
                 UpdateCustomConfigHistogram);

  if (!sync_everything) {
    const syncable::ModelTypeSet current_types = GetPreferredDataTypes();
    for (size_t i = 0; i < arraysize(model_types); ++i) {
      const syncable::ModelType type = model_types[i];
      if (chosen_types.Has(type) &&
          (!HasSyncSetupCompleted() || !current_types.Has(type))) {
        // Selected type has changed - log it.
        UMA_HISTOGRAM_ENUMERATION(
            "Sync.CustomSync",
            user_selectable_types[i],
            browser_sync::user_selectable_type::SELECTABLE_DATATYPE_COUNT + 1);
      }
    }
  }
}

#if defined(OS_CHROMEOS)
void ProfileSyncService::RefreshSpareBootstrapToken(
    const std::string& passphrase) {
  browser_sync::ChromeEncryptor encryptor;
  browser_sync::Cryptographer temp_cryptographer(&encryptor);
  // The first 2 params (hostname and username) doesn't have any effect here.
  browser_sync::KeyParams key_params = {"localhost", "dummy", passphrase};

  std::string bootstrap_token;
  if (!temp_cryptographer.AddKey(key_params)) {
    NOTREACHED() << "Failed to add key to cryptographer.";
  }
  temp_cryptographer.GetBootstrapToken(&bootstrap_token);
  sync_prefs_.SetSpareBootstrapToken(bootstrap_token);
}
#endif

void ProfileSyncService::OnUserChoseDatatypes(bool sync_everything,
    syncable::ModelTypeSet chosen_types) {
  if (!backend_.get() &&
      unrecoverable_error_detected_ == false) {
    NOTREACHED();
    return;
  }

  UpdateSelectedTypesHistogram(sync_everything, chosen_types);
  sync_prefs_.SetKeepEverythingSynced(sync_everything);

  failed_datatypes_handler_.OnUserChoseDatatypes();
  ChangePreferredDataTypes(chosen_types);
  AcknowledgeSyncedTypes();
  NotifyObservers();
}

void ProfileSyncService::ChangePreferredDataTypes(
    syncable::ModelTypeSet preferred_types) {

  DVLOG(1) << "ChangePreferredDataTypes invoked";
  const syncable::ModelTypeSet registered_types = GetRegisteredDataTypes();
  const syncable::ModelTypeSet registered_preferred_types =
      Intersection(registered_types, preferred_types);
  sync_prefs_.SetPreferredDataTypes(registered_types,
                                    registered_preferred_types);

  // Now reconfigure the DTM.
  ReconfigureDatatypeManager();
}

syncable::ModelTypeSet ProfileSyncService::GetPreferredDataTypes() const {
  const syncable::ModelTypeSet registered_types = GetRegisteredDataTypes();
  const syncable::ModelTypeSet preferred_types =
      sync_prefs_.GetPreferredDataTypes(registered_types);
  const syncable::ModelTypeSet failed_types =
      failed_datatypes_handler_.GetFailedTypes();
  return Difference(preferred_types, failed_types);
}

syncable::ModelTypeSet ProfileSyncService::GetRegisteredDataTypes() const {
  syncable::ModelTypeSet registered_types;
  // The data_type_controllers_ are determined by command-line flags; that's
  // effectively what controls the values returned here.
  for (DataTypeController::TypeMap::const_iterator it =
       data_type_controllers_.begin();
       it != data_type_controllers_.end(); ++it) {
    registered_types.Put(it->first);
  }
  return registered_types;
}

bool ProfileSyncService::IsUsingSecondaryPassphrase() const {
  return backend_->IsUsingExplicitPassphrase();
}

bool ProfileSyncService::IsCryptographerReady(
    const sync_api::BaseTransaction* trans) const {
  return backend_.get() && backend_->IsCryptographerReady(trans);
}

SyncBackendHost* ProfileSyncService::GetBackendForTest() {
  // We don't check |backend_initialized_|; we assume the test class
  // knows what it's doing.
  return backend_.get();
}

void ProfileSyncService::ConfigureDataTypeManager() {
  bool restart = false;
  if (!data_type_manager_.get()) {
    restart = true;
    data_type_manager_.reset(
        factory_->CreateDataTypeManager(backend_.get(),
                                        &data_type_controllers_));
    registrar_.Add(this,
                   chrome::NOTIFICATION_SYNC_CONFIGURE_START,
                   content::Source<DataTypeManager>(data_type_manager_.get()));
    registrar_.Add(this,
                   chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
                   content::Source<DataTypeManager>(data_type_manager_.get()));
    registrar_.Add(this,
                   chrome::NOTIFICATION_SYNC_CONFIGURE_BLOCKED,
                   content::Source<DataTypeManager>(data_type_manager_.get()));

    // We create the migrator at the same time.
    migrator_.reset(
        new browser_sync::BackendMigrator(
            profile_->GetDebugName(), GetUserShare(),
            this, data_type_manager_.get(),
            base::Bind(&ProfileSyncService::StartSyncingWithServer,
                       base::Unretained(this))));
  }

  const syncable::ModelTypeSet types = GetPreferredDataTypes();
  if (IsPassphraseRequiredForDecryption()) {
    // We need a passphrase still. We don't bother to attempt to configure
    // until we receive an OnPassphraseAccepted (which triggers a configure).
    DVLOG(1) << "ProfileSyncService::ConfigureDataTypeManager bailing out "
             << "because a passphrase required";
    NotifyObservers();
    return;
  }
  sync_api::ConfigureReason reason = sync_api::CONFIGURE_REASON_UNKNOWN;
  if (!HasSyncSetupCompleted()) {
    reason = sync_api::CONFIGURE_REASON_NEW_CLIENT;
  } else if (restart == false ||
             sync_api::InitialSyncEndedForTypes(types, GetUserShare())) {
    reason = sync_api::CONFIGURE_REASON_RECONFIGURATION;
  } else {
    DCHECK(restart);
    reason = sync_api::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE;
  }
  DCHECK(reason != sync_api::CONFIGURE_REASON_UNKNOWN);

  data_type_manager_->Configure(types, reason);
}

sync_api::UserShare* ProfileSyncService::GetUserShare() const {
  if (backend_.get() && backend_initialized_) {
    return backend_->GetUserShare();
  }
  NOTREACHED();
  return NULL;
}

browser_sync::sessions::SyncSessionSnapshot
    ProfileSyncService::GetLastSessionSnapshot() const {
  if (backend_.get() && backend_initialized_) {
    return backend_->GetLastSessionSnapshot();
  }
  NOTREACHED();
  return browser_sync::sessions::SyncSessionSnapshot();
}

bool ProfileSyncService::HasUnsyncedItems() const {
  if (backend_.get() && backend_initialized_) {
    return backend_->HasUnsyncedItems();
  }
  NOTREACHED();
  return false;
}

browser_sync::BackendMigrator*
    ProfileSyncService::GetBackendMigratorForTest() {
  return migrator_.get();
}

void ProfileSyncService::GetModelSafeRoutingInfo(
    browser_sync::ModelSafeRoutingInfo* out) const {
  if (backend_.get() && backend_initialized_) {
    backend_->GetModelSafeRoutingInfo(out);
  } else {
    NOTREACHED();
  }
}

void ProfileSyncService::ActivateDataType(
    syncable::ModelType type, browser_sync::ModelSafeGroup group,
    ChangeProcessor* change_processor) {
  if (!backend_.get()) {
    NOTREACHED();
    return;
  }
  DCHECK(backend_initialized_);
  backend_->ActivateDataType(type, group, change_processor);
}

void ProfileSyncService::DeactivateDataType(syncable::ModelType type) {
  if (!backend_.get())
    return;
  backend_->DeactivateDataType(type);
}

void ProfileSyncService::ConsumeCachedPassphraseIfPossible() {
  // If no cached passphrase, or sync backend hasn't started up yet, just exit.
  // If the backend isn't running yet, OnBackendInitialized() will call this
  // method again after the backend starts up.
  if (cached_passphrase_.empty() || !sync_initialized())
    return;

  // Backend is up and running, so we can consume the cached passphrase.
  std::string passphrase = cached_passphrase_;
  cached_passphrase_.clear();

  // If we need a passphrase to decrypt data, try the cached passphrase.
  if (passphrase_required_reason() == sync_api::REASON_DECRYPTION) {
    if (SetDecryptionPassphrase(passphrase)) {
      DVLOG(1) << "Cached passphrase successfully decrypted pending keys";
      return;
    }
  }

  // If we get here, we don't have pending keys (or at least, the passphrase
  // doesn't decrypt them) - just try to re-encrypt using the encryption
  // passphrase.
  if (!IsUsingSecondaryPassphrase())
    SetEncryptionPassphrase(passphrase, IMPLICIT);
}

void ProfileSyncService::SetEncryptionPassphrase(const std::string& passphrase,
                                                 PassphraseType type) {
  // This should only be called when the backend has been initialized.
  DCHECK(sync_initialized());
  DCHECK(!(type == IMPLICIT && IsUsingSecondaryPassphrase())) <<
      "Data is already encrypted using an explicit passphrase";
  DCHECK(!(type == EXPLICIT && IsPassphraseRequired())) <<
      "Cannot switch to an explicit passphrase if a passphrase is required";

  if (type == EXPLICIT)
    UMA_HISTOGRAM_BOOLEAN("Sync.CustomPassphrase", true);

  DVLOG(1) << "Setting " << (type == EXPLICIT ? "explicit" : "implicit")
           << " passphrase for encryption.";
  if (passphrase_required_reason_ == sync_api::REASON_ENCRYPTION) {
    // REASON_ENCRYPTION implies that the cryptographer does not have pending
    // keys. Hence, as long as we're not trying to do an invalid passphrase
    // change (e.g. explicit -> explicit or explicit -> implicit), we know this
    // will succeed. If for some reason a new encryption key arrives via
    // sync later, the SBH will trigger another OnPassphraseRequired().
    passphrase_required_reason_ = sync_api::REASON_PASSPHRASE_NOT_REQUIRED;
    NotifyObservers();
  }
  backend_->SetEncryptionPassphrase(passphrase, type == EXPLICIT);
}

bool ProfileSyncService::SetDecryptionPassphrase(
    const std::string& passphrase) {
  if (IsPassphraseRequired()) {
    DVLOG(1) << "Setting passphrase for decryption.";
    return backend_->SetDecryptionPassphrase(passphrase);
  } else {
    NOTREACHED() << "SetDecryptionPassphrase must not be called when "
                    "IsPassphraseRequired() is false.";
    return false;
  }
}

void ProfileSyncService::EnableEncryptEverything() {
  // Tests override sync_initialized() to always return true, so we
  // must check that instead of |backend_initialized_|.
  // TODO(akalin): Fix the above. :/
  DCHECK(sync_initialized());
  // TODO(atwilson): Persist the encryption_pending_ flag to address the various
  // problems around cancelling encryption in the background (crbug.com/119649).
  if (!encrypt_everything_)
    encryption_pending_ = true;
  UMA_HISTOGRAM_BOOLEAN("Sync.EncryptAllData", true);
}

bool ProfileSyncService::encryption_pending() const {
  // We may be called during the setup process before we're
  // initialized (via IsEncryptedDatatypeEnabled and
  // IsPassphraseRequiredForDecryption).
  return encryption_pending_;
}

bool ProfileSyncService::EncryptEverythingEnabled() const {
  DCHECK(backend_initialized_);
  return encrypt_everything_ || encryption_pending_;
}

syncable::ModelTypeSet ProfileSyncService::GetEncryptedDataTypes() const {
  DCHECK(encrypted_types_.Has(syncable::PASSWORDS));
  // We may be called during the setup process before we're
  // initialized.  In this case, we default to the sensitive types.
  return encrypted_types_;
}

void ProfileSyncService::OnSyncManagedPrefChange(bool is_sync_managed) {
  NotifyObservers();
  if (is_sync_managed) {
    DisableForUser();
  } else if (HasSyncSetupCompleted() && AreCredentialsAvailable()) {
    StartUp();
  }
}

void ProfileSyncService::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_SYNC_CONFIGURE_START:
    case chrome::NOTIFICATION_SYNC_CONFIGURE_BLOCKED:
      NotifyObservers();
      // TODO(sync): Maybe toast?
      break;
    case chrome::NOTIFICATION_SYNC_CONFIGURE_DONE: {
      // We should have cleared our cached passphrase before we get here (in
      // OnBackendInitialized()).
      DCHECK(cached_passphrase_.empty());

      DataTypeManager::ConfigureResult* result =
          content::Details<DataTypeManager::ConfigureResult>(details).ptr();

      configure_status_ = result->status;
      DVLOG(1) << "PSS SYNC_CONFIGURE_DONE called with status: "
               << configure_status_;

      // The possible status values:
      //    ABORT - Configuration was aborted. This is not an error, if
      //            initiated by user.
      //    RETRY - Configure failed but we are retrying.
      //    OK - Everything succeeded.
      //    PARTIAL_SUCCESS - Some datatypes failed to start.
      //    Everything else is an UnrecoverableError. So treat it as such.

      // First handle the abort case.
      if (configure_status_ == DataTypeManager::ABORTED &&
          expect_sync_configuration_aborted_) {
        DVLOG(0) << "ProfileSyncService::Observe Sync Configure aborted";
        expect_sync_configuration_aborted_ = false;
        return;
      }

      // Handle retry case.
      if (configure_status_ == DataTypeManager::RETRY) {
        OnSyncConfigureRetry();
        return;
      }

      // Handle unrecoverable error.
      if (configure_status_ != DataTypeManager::OK &&
          configure_status_ != DataTypeManager::PARTIAL_SUCCESS) {
        // Something catastrophic had happened. We should only have one
        // error representing it.
        DCHECK(result->errors.size() == 1);
        SyncError error = result->errors.front();
        DCHECK(error.IsSet());
        std::string message =
          "Sync configuration failed with status " +
          DataTypeManager::ConfigureStatusToString(configure_status_) +
          " during " + syncable::ModelTypeToString(error.type()) +
          ": " + error.message();
        LOG(ERROR) << "ProfileSyncService error: "
                   << message;
        OnUnrecoverableError(error.location(), message);
        return;
      }

      // Now handle partial success and full success.
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(&ProfileSyncService::OnSyncConfigureDone,
                     weak_factory_.GetWeakPtr(), *result));

      // We should never get in a state where we have no encrypted datatypes
      // enabled, and yet we still think we require a passphrase for decryption.
      DCHECK(!(IsPassphraseRequiredForDecryption() &&
               !IsEncryptedDatatypeEnabled()));

      // This must be done before we start syncing with the server to avoid
      // sending unencrypted data up on a first time sync.
      if (encryption_pending_)
        backend_->EnableEncryptEverything();
      NotifyObservers();

      if (migrator_.get() &&
          migrator_->state() != browser_sync::BackendMigrator::IDLE) {
        // Migration in progress.  Let the migrator know we just finished
        // configuring something.  It will be up to the migrator to call
        // StartSyncingWithServer() if migration is now finished.
        migrator_->OnConfigureDone(*result);
      } else {
        StartSyncingWithServer();
      }

      break;
    }
    case chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL: {
      const GoogleServiceSigninSuccessDetails* successful =
          content::Details<const GoogleServiceSigninSuccessDetails>(
              details).ptr();
      DCHECK(!successful->password.empty());
      if (!sync_prefs_.IsStartSuppressed()) {
        cached_passphrase_ = successful->password;
        // Try to consume the passphrase we just cached. If the sync backend
        // is not running yet, the passphrase will remain cached until the
        // backend starts up.
        ConsumeCachedPassphraseIfPossible();
      }
#if defined(OS_CHROMEOS)
      RefreshSpareBootstrapToken(successful->password);
#endif
      if (!sync_initialized() ||
          GetAuthError().state() != GoogleServiceAuthError::NONE) {
        // Track the fact that we're still waiting for auth to complete.
        is_auth_in_progress_ = true;
      }
      break;
    }
    case chrome::NOTIFICATION_TOKEN_REQUEST_FAILED: {
      const TokenService::TokenRequestFailedDetails& token_details =
          *(content::Details<const TokenService::TokenRequestFailedDetails>(
              details).ptr());
      if (IsTokenServiceRelevant(token_details.service()) &&
          !AreCredentialsAvailable()) {
        // The additional check around AreCredentialsAvailable above prevents us
        // sounding the alarm if we actually have a valid token but a refresh
        // attempt by TokenService failed for any variety of reasons (e.g. flaky
        // network). It's possible the token we do have is also invalid, but in
        // that case we should already have (or can expect) an auth error sent
        // from the sync backend.
        GoogleServiceAuthError error(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
        UpdateAuthErrorState(error);
      }
      break;
    }
    case chrome::NOTIFICATION_TOKEN_AVAILABLE: {
      const TokenService::TokenAvailableDetails& token_details =
          *(content::Details<const TokenService::TokenAvailableDetails>(
              details).ptr());
      if (IsTokenServiceRelevant(token_details.service()) &&
          AreCredentialsAvailable()) {
        if (backend_initialized_)
          backend_->UpdateCredentials(GetCredentials());
        else if (!sync_prefs_.IsStartSuppressed())
          StartUp();
      }
      break;
    }
    case chrome::NOTIFICATION_TOKEN_LOADING_FINISHED: {
      // This notification gets fired when TokenService loads the tokens
      // from storage.
      if (AreCredentialsAvailable()) {
        // Initialize the backend if sync token was loaded.
        if (backend_initialized_) {
          backend_->UpdateCredentials(GetCredentials());
        }
        if (!sync_prefs_.IsStartSuppressed())
          StartUp();
      } else if (!auto_start_enabled_ &&
                 !signin_->GetAuthenticatedUsername().empty() &&
                 HasSyncSetupCompleted()) {
        // If not in auto-start / Chrome OS mode, and we have a username
        // without tokens, the user will need to signin again. At the moment
        // this is not supported, so we trigger an unrecoverable error.
        OnUnrecoverableError(FROM_HERE, "Sync credentials lost.");
      }
      break;
    }
    default: {
      NOTREACHED();
    }
  }
}

void ProfileSyncService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProfileSyncService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ProfileSyncService::HasObserver(Observer* observer) const {
  return observers_.HasObserver(observer);
}

base::WeakPtr<JsController> ProfileSyncService::GetJsController() {
  return sync_js_controller_.AsWeakPtr();
}

void ProfileSyncService::SyncEvent(SyncEventCodes code) {
  UMA_HISTOGRAM_ENUMERATION("Sync.EventCodes", code, MAX_SYNC_EVENT_CODE);
}

// static
bool ProfileSyncService::IsSyncEnabled() {
  // We have switches::kEnableSync just in case we need to change back to
  // sync-disabled-by-default on a platform.
  return !CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableSync);
}

bool ProfileSyncService::IsManaged() const {
  return sync_prefs_.IsManaged();
}

bool ProfileSyncService::ShouldPushChanges() {
  // True only after all bootstrapping has succeeded: the sync backend
  // is initialized, all enabled data types are consistent with one
  // another, and no unrecoverable error has transpired.
  if (unrecoverable_error_detected_)
    return false;

  if (!data_type_manager_.get())
    return false;

  return data_type_manager_->state() == DataTypeManager::CONFIGURED;
}

void ProfileSyncService::StopAndSuppress() {
  sync_prefs_.SetStartSuppressed(true);
  ShutdownImpl(false);
}

void ProfileSyncService::UnsuppressAndStart() {
  DCHECK(profile_);
  sync_prefs_.SetStartSuppressed(false);
  // Set username in SigninManager, as SigninManager::OnGetUserInfoSuccess
  // is never called for some clients.
  if (signin_ && signin_->GetAuthenticatedUsername().empty()) {
    signin_->SetAuthenticatedUsername(sync_prefs_.GetGoogleServicesUsername());
  }
  TryStart();
}

void ProfileSyncService::AcknowledgeSyncedTypes() {
  sync_prefs_.AcknowledgeSyncedTypes(GetRegisteredDataTypes());
}

void ProfileSyncService::ReconfigureDatatypeManager() {
  // If we haven't initialized yet, don't configure the DTM as it could cause
  // association to start before a Directory has even been created.
  if (backend_initialized_) {
    DCHECK(backend_.get());
    ConfigureDataTypeManager();
  } else if (unrecoverable_error_detected()) {
    // There is nothing more to configure. So inform the listeners,
    NotifyObservers();

    DVLOG(1) << "ConfigureDataTypeManager not invoked because of an "
             << "Unrecoverable error.";
  } else {
    DVLOG(0) << "ConfigureDataTypeManager not invoked because backend is not "
             << "initialized";
  }
}

const FailedDatatypesHandler& ProfileSyncService::failed_datatypes_handler() {
  return failed_datatypes_handler_;
}

void ProfileSyncService::ResetForTest() {
  Profile* profile = profile_;
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile);
  ProfileSyncService::StartBehavior behavior =
      browser_defaults::kSyncAutoStarts ? ProfileSyncService::AUTO_START
                                        : ProfileSyncService::MANUAL_START;

  // We call the destructor and placement new here because we want to explicitly
  // recreate a new ProfileSyncService instance at the same memory location as
  // the old one. Doing so is fine because this code is run only from within
  // integration tests, and the message loop is not running at this point.
  // See http://stackoverflow.com/questions/6224121/is-new-this-myclass-undefined-behaviour-after-directly-calling-the-destru.
  ProfileSyncService* old_this = this;
  this->~ProfileSyncService();
  new(old_this) ProfileSyncService(
      new ProfileSyncComponentsFactoryImpl(profile,
                                           CommandLine::ForCurrentProcess()),
      profile,
      signin,
      behavior);
}
