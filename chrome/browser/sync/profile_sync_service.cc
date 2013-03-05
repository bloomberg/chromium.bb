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
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
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
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/browser/sync/glue/session_data_type_controller.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/synced_device_tracker.h"
#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/sync_global_error.h"
#include "chrome/browser/sync/user_selectable_sync_type.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_constants.h"
#include "grit/generated_resources.h"
#include "net/cookies/cookie_monster.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/public/configure_reason.h"
#include "sync/internal_api/public/sync_encryption_handler.h"
#include "sync/internal_api/public/util/experiments.h"
#include "sync/internal_api/public/util/sync_string_conversions.h"
#include "sync/js/js_arg_list.h"
#include "sync/js/js_event_details.h"
#include "sync/notifier/invalidator_registrar.h"
#include "sync/notifier/invalidator_state.h"
#include "sync/util/cryptographer.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "sync/internal_api/public/read_transaction.h"
#endif

using browser_sync::ChangeProcessor;
using browser_sync::DataTypeController;
using browser_sync::DataTypeManager;
using browser_sync::SyncBackendHost;
using syncer::ModelType;
using syncer::ModelTypeSet;
using syncer::JsBackend;
using syncer::JsController;
using syncer::JsEventDetails;
using syncer::JsEventHandler;
using syncer::ModelSafeRoutingInfo;
using syncer::SyncCredentials;
using syncer::SyncProtocolError;
using syncer::WeakHandle;

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

static const char* kSyncUnrecoverableErrorHistogram =
    "Sync.UnrecoverableErrors";

// Helper to check if the given token service is relevant for sync.
static bool IsTokenServiceRelevant(const std::string& service) {
  for (int i = 0; i < kRelevantTokenServicesCount; ++i) {
    if (service == kRelevantTokenServices[i])
      return true;
  }
  return false;
}

bool ShouldShowActionOnUI(
    const syncer::SyncProtocolError& error) {
  return (error.action != syncer::UNKNOWN_ACTION &&
          error.action != syncer::DISABLE_SYNC_ON_CLIENT);
}

ProfileSyncService::ProfileSyncService(ProfileSyncComponentsFactory* factory,
                                       Profile* profile,
                                       SigninManager* signin_manager,
                                       StartBehavior start_behavior)
    : last_auth_error_(AuthError::None()),
      passphrase_required_reason_(syncer::REASON_PASSPHRASE_NOT_REQUIRED),
      factory_(factory),
      profile_(profile),
      // |profile| may be NULL in unit tests.
      sync_prefs_(profile_ ? profile_->GetPrefs() : NULL),
      invalidator_storage_(profile_ ? profile_->GetPrefs(): NULL),
      sync_service_url_(kDevServerUrl),
      is_first_time_sync_configure_(false),
      backend_initialized_(false),
      is_auth_in_progress_(false),
      signin_(signin_manager),
      unrecoverable_error_reason_(ERROR_REASON_UNSET),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      expect_sync_configuration_aborted_(false),
      encrypted_types_(syncer::SyncEncryptionHandler::SensitiveTypes()),
      encrypt_everything_(false),
      encryption_pending_(false),
      auto_start_enabled_(start_behavior == AUTO_START),
      failed_datatypes_handler_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      configure_status_(DataTypeManager::UNKNOWN),
      setup_in_progress_(false),
      invalidator_state_(syncer::DEFAULT_INVALIDATION_ERROR) {
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
  if (signin_)
    signin_->signin_global_error()->AddProvider(this);
}

ProfileSyncService::~ProfileSyncService() {
  sync_prefs_.RemoveSyncPrefObserver(this);
  // Shutdown() should have been called before destruction.
  CHECK(!backend_initialized_);
}

bool ProfileSyncService::IsSyncEnabledAndLoggedIn() {
  // Exit if sync is disabled.
  if (IsManaged() || sync_prefs_.IsStartSuppressed())
    return false;

  // Sync is logged in if there is a non-empty authenticated username.
  return !signin_->GetAuthenticatedUsername().empty();
}

bool ProfileSyncService::IsSyncTokenAvailable() {
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  if (!token_service)
    return false;
  return token_service->HasTokenForService(GaiaConstants::kSyncService);
}
#if defined(OS_ANDROID)
bool ProfileSyncService::ShouldEnablePasswordSyncForAndroid() const {
  const syncer::ModelTypeSet registered_types = GetRegisteredDataTypes();
  const syncer::ModelTypeSet preferred_types =
      sync_prefs_.GetPreferredDataTypes(registered_types);
  if (!preferred_types.Has(syncer::PASSWORDS))
    return false;
  // On Android we do not want to prompt user to enter a passphrase. If
  // passwords cannot be decrypted we just disable them.
  syncer::ReadTransaction trans(FROM_HERE, GetUserShare());
  return IsCryptographerReady(&trans);
}
#endif

void ProfileSyncService::Initialize() {
  DCHECK(!invalidator_registrar_.get());
  invalidator_registrar_.reset(new syncer::InvalidatorRegistrar());

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

  if (!HasSyncSetupCompleted() || signin_->GetAuthenticatedUsername().empty()) {
    // Clean up in case of previous crash / setup abort / signout.
    DisableForUser();
  }

  TrySyncDatatypePrefRecovery();

  TryStart();
}

void ProfileSyncService::TrySyncDatatypePrefRecovery() {
  DCHECK(!sync_initialized());
  if (!HasSyncSetupCompleted())
    return;

  // There was a bug where OnUserChoseDatatypes was not properly called on
  // configuration (see crbug.com/154940). We detect this by checking whether
  // kSyncKeepEverythingSynced has a default value. If so, and sync setup has
  // completed, it means sync was not properly configured, so we manually
  // set kSyncKeepEverythingSynced.
  PrefService* const pref_service = profile_->GetPrefs();
  if (!pref_service)
    return;
  const syncer::ModelTypeSet registered_types = GetRegisteredDataTypes();
  if (sync_prefs_.GetPreferredDataTypes(registered_types).Size() > 1)
    return;

  const PrefService::Preference* keep_everything_synced =
      pref_service->FindPreference(prefs::kSyncKeepEverythingSynced);
  // This will be false if the preference was properly set or if it's controlled
  // by policy.
  if (!keep_everything_synced->IsDefaultValue())
    return;

  // kSyncKeepEverythingSynced was not properly set. Set it and the preferred
  // types now, before we configure.
  UMA_HISTOGRAM_COUNTS("Sync.DatatypePrefRecovery", 1);
  sync_prefs_.SetKeepEverythingSynced(true);
  sync_prefs_.SetPreferredDataTypes(registered_types,
                                    registered_types);
}

void ProfileSyncService::TryStart() {
  if (!IsSyncEnabledAndLoggedIn())
    return;
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  if (!token_service)
    return;
  // Don't start the backend if the token service hasn't finished loading tokens
  // yet. Note if the backend is started before the sync token has been loaded,
  // GetCredentials() will return bogus credentials. On auto_start platforms
  // (like ChromeOS) we don't start sync until tokens are loaded, because the
  // user can be "signed in" on those platforms long before the tokens get
  // loaded, and we don't want to generate spurious auth errors.
  if (!IsSyncTokenAvailable() &&
      !(!auto_start_enabled_ && token_service->TokensLoadedFromDB())) {
    return;
  }

  // If sync setup has completed we always start the backend. If the user is in
  // the process of setting up now, we should start the backend to download
  // account control state / encryption information). If autostart is enabled,
  // but we haven't completed sync setup, we try to start sync anyway, since
  // it's possible we crashed/shutdown after logging in but before the backend
  // finished initializing the last time.
  if (!HasSyncSetupCompleted() && !setup_in_progress_ && !auto_start_enabled_)
    return;

  // All systems Go for launch.
  StartUp();
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
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
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
  if (data_type_controllers_.find(syncer::SESSIONS) ==
      data_type_controllers_.end() ||
      data_type_controllers_.find(syncer::SESSIONS)->second->state() !=
      DataTypeController::RUNNING) {
    return NULL;
  }
  return static_cast<browser_sync::SessionDataTypeController*>(
      data_type_controllers_.find(
      syncer::SESSIONS)->second.get())->GetModelAssociator();
}

scoped_ptr<browser_sync::DeviceInfo>
ProfileSyncService::GetLocalDeviceInfo() const {
  DCHECK(sync_initialized());
  browser_sync::SyncedDeviceTracker* device_tracker =
      backend_->GetSyncedDeviceTracker();
  if (device_tracker)
    return device_tracker->ReadLocalDeviceInfo();
  else
    return scoped_ptr<browser_sync::DeviceInfo>();
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
  if (service->HasTokenForService(GaiaConstants::kSyncService)) {
      credentials.sync_token = service->GetTokenForService(
          GaiaConstants::kSyncService);
    UMA_HISTOGRAM_BOOLEAN("Sync.CredentialsLost", false);
  } else {
    // We've lost our sync credentials (crbug.com/121755), so just make up some
    // invalid credentials so the backend will generate an auth error.
    UMA_HISTOGRAM_BOOLEAN("Sync.CredentialsLost", true);
    credentials.sync_token = "credentials_lost";
  }
  return credentials;
}

void ProfileSyncService::InitializeBackend(bool delete_stale_data) {
  if (!backend_.get()) {
    NOTREACHED();
    return;
  }

  SyncCredentials credentials = GetCredentials();

  scoped_refptr<net::URLRequestContextGetter> request_context_getter(
      profile_->GetRequestContext());

  if (delete_stale_data)
    ClearStaleErrors();

  backend_unrecoverable_error_handler_.reset(
    new browser_sync::BackendUnrecoverableErrorHandler(
        MakeWeakHandle(weak_factory_.GetWeakPtr())));

  backend_->Initialize(
      this,
      MakeWeakHandle(sync_js_controller_.AsWeakPtr()),
      sync_service_url_,
      credentials,
      delete_stale_data,
      &sync_manager_factory_,
      backend_unrecoverable_error_handler_.get(),
      &browser_sync::ChromeReportUnrecoverableError);
}

void ProfileSyncService::CreateBackend() {
  backend_.reset(
      new SyncBackendHost(profile_->GetDebugName(),
                          profile_, sync_prefs_.AsWeakPtr(),
                          invalidator_storage_.AsWeakPtr()));
}

bool ProfileSyncService::IsEncryptedDatatypeEnabled() const {
  if (encryption_pending())
    return true;
  const syncer::ModelTypeSet preferred_types = GetPreferredDataTypes();
  const syncer::ModelTypeSet encrypted_types = GetEncryptedDataTypes();
  DCHECK(encrypted_types.Has(syncer::PASSWORDS));
  return !Intersection(preferred_types, encrypted_types).Empty();
}

void ProfileSyncService::OnSyncConfigureDone(
    DataTypeManager::ConfigureResult result) {
  if (failed_datatypes_handler_.UpdateFailedDatatypes(result.failed_data_types,
          FailedDatatypesHandler::STARTUP)) {
    ReconfigureDatatypeManager();
  }
}

void ProfileSyncService::OnSyncConfigureRetry() {
  // Note: in order to handle auth failures that arise before the backend is
  // initialized (e.g. from invalidation notifier, or downloading new control
  // types), we have to gracefully handle configuration retries at all times.
  // At this point an auth error badge should be shown, which once resolved
  // will trigger a new sync cycle.
  NotifyObservers();
}


void ProfileSyncService::StartUp() {
  // Don't start up multiple times.
  if (backend_.get()) {
    DVLOG(1) << "Skipping bringing up backend host.";
    return;
  }

  DCHECK(IsSyncEnabledAndLoggedIn());

  last_synced_time_ = sync_prefs_.GetLastSyncedTime();
  start_up_time_ = base::Time::Now();

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

  // |backend_| may end up being NULL here in tests (in synchronous
  // initialization mode).
  //
  // TODO(akalin): Fix this horribly non-intuitive behavior (see
  // http://crbug.com/140354).
  if (backend_.get()) {
    backend_->UpdateRegisteredInvalidationIds(
        invalidator_registrar_->GetAllRegisteredIds());
    for (AckHandleReplayQueue::const_iterator it = ack_replay_queue_.begin();
         it != ack_replay_queue_.end(); ++it) {
      backend_->AcknowledgeInvalidation(it->first, it->second);
    }
    ack_replay_queue_.clear();
  }

  if (!sync_global_error_.get()) {
#if !defined(OS_ANDROID)
    sync_global_error_.reset(new SyncGlobalError(this, signin()));
#endif
    GlobalErrorServiceFactory::GetForProfile(profile_)->AddGlobalError(
        sync_global_error_.get());
    AddObserver(sync_global_error_.get());
  }
}

void ProfileSyncService::RegisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  invalidator_registrar_->RegisterHandler(handler);
}

void ProfileSyncService::UpdateRegisteredInvalidationIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  invalidator_registrar_->UpdateRegisteredIds(handler, ids);

  // If |backend_| is NULL, its registered IDs will be updated when
  // it's created and initialized.
  if (backend_.get()) {
    backend_->UpdateRegisteredInvalidationIds(
        invalidator_registrar_->GetAllRegisteredIds());
  }
}

void ProfileSyncService::UnregisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  invalidator_registrar_->UnregisterHandler(handler);
}

void ProfileSyncService::AcknowledgeInvalidation(
    const invalidation::ObjectId& id,
    const syncer::AckHandle& ack_handle) {
  if (backend_.get()) {
    backend_->AcknowledgeInvalidation(id, ack_handle);
  } else {
    // If |backend_| is NULL, save the acknowledgements to replay when
    // it's created and initialized.
    ack_replay_queue_.push_back(std::make_pair(id, ack_handle));
  }
}

syncer::InvalidatorState ProfileSyncService::GetInvalidatorState() const {
  return invalidator_registrar_->GetInvalidatorState();
}

void ProfileSyncService::EmitInvalidationForTest(
    const invalidation::ObjectId& id,
    const std::string& payload) {
  syncer::ObjectIdSet notify_ids;
  notify_ids.insert(id);

  const syncer::ObjectIdInvalidationMap& invalidation_map =
      ObjectIdSetToInvalidationMap(notify_ids, payload);
  OnIncomingInvalidation(invalidation_map);
}

void ProfileSyncService::Shutdown() {
  DCHECK(invalidator_registrar_.get());
  // Reset |invalidator_registrar_| first so that ShutdownImpl cannot
  // use it.
  invalidator_registrar_.reset();

  if (signin_)
    signin_->signin_global_error()->RemoveProvider(this);

  ShutdownImpl(false);
}

void ProfileSyncService::ShutdownImpl(bool sync_disabled) {
  // First, we spin down the backend and wait for it to stop syncing completely
  // before we Stop the data type manager.  This is to avoid a late sync cycle
  // applying changes to the sync db that wouldn't get applied via
  // ChangeProcessors, leading to back-from-the-dead bugs.
  base::Time shutdown_start_time = base::Time::Now();
  if (backend_.get()) {
    backend_->StopSyncingForShutdown();
  }

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
    data_type_manager_.reset();
  }

  // Shutdown the migrator before the backend to ensure it doesn't pull a null
  // snapshot.
  migrator_.reset();
  sync_js_controller_.AttachJsBackend(WeakHandle<syncer::JsBackend>());

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
  // NULL if we're called from Shutdown().
  if (invalidator_registrar_.get())
    UpdateInvalidatorRegistrarState();
  cached_passphrase_.clear();
  encryption_pending_ = false;
  encrypt_everything_ = false;
  encrypted_types_ = syncer::SyncEncryptionHandler::SensitiveTypes();
  passphrase_required_reason_ = syncer::REASON_PASSPHRASE_NOT_REQUIRED;
  // Revert to "no auth error".
  if (last_auth_error_.state() != GoogleServiceAuthError::NONE)
    UpdateAuthErrorState(GoogleServiceAuthError::None());

  if (sync_global_error_.get()) {
    GlobalErrorServiceFactory::GetForProfile(profile_)->RemoveGlobalError(
        sync_global_error_.get());
    RemoveObserver(sync_global_error_.get());
    sync_global_error_.reset(NULL);
  }
}

void ProfileSyncService::DisableForUser() {
  // Clear prefs (including SyncSetupHasCompleted) before shutting down so
  // PSS clients don't think we're set up while we're shutting down.
  sync_prefs_.ClearPreferences();
  invalidator_storage_.Clear();
  ClearUnrecoverableError();
  ShutdownImpl(true);
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
  unrecoverable_error_reason_ = ERROR_REASON_UNSET;
  unrecoverable_error_message_.clear();
  unrecoverable_error_location_ = tracked_objects::Location();
}

// static
// TODO(sync): Consider having syncer::Experiments provide this.
std::string ProfileSyncService::GetExperimentNameForDataType(
    syncer::ModelType data_type) {
  NOTREACHED();
  return "";
}

void ProfileSyncService::RegisterNewDataType(syncer::ModelType data_type) {
  if (data_type_controllers_.count(data_type) > 0)
    return;
  NOTREACHED();
}

// An invariant has been violated.  Transition to an error state where we try
// to do as little work as possible, to avoid further corruption or crashes.
void ProfileSyncService::OnUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  // Unrecoverable errors that arrive via the syncer::UnrecoverableErrorHandler
  // interface are assumed to originate within the syncer.
  unrecoverable_error_reason_ = ERROR_REASON_SYNCER;
  OnUnrecoverableErrorImpl(from_here, message, true);
}

void ProfileSyncService::OnUnrecoverableErrorImpl(
    const tracked_objects::Location& from_here,
    const std::string& message,
    bool delete_sync_database) {
  DCHECK(HasUnrecoverableError());
  unrecoverable_error_message_ = message;
  unrecoverable_error_location_ = from_here;

  UMA_HISTOGRAM_ENUMERATION(kSyncUnrecoverableErrorHistogram,
                            unrecoverable_error_reason_,
                            ERROR_REASON_LIMIT);
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

void ProfileSyncService::DisableBrokenDatatype(
    syncer::ModelType type,
    const tracked_objects::Location& from_here,
    std::string message) {
  // First deactivate the type so that no further server changes are
  // passed onto the change processor.
  DeactivateDataType(type);

  syncer::SyncError error(from_here, message, type);

  std::list<syncer::SyncError> errors;
  errors.push_back(error);

  // Update this before posting a task. So if a configure happens before
  // the task that we are going to post, this type would still be disabled.
  failed_datatypes_handler_.UpdateFailedDatatypes(errors,
      FailedDatatypesHandler::RUNTIME);

  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&ProfileSyncService::ReconfigureDatatypeManager,
                 weak_factory_.GetWeakPtr()));
}

void ProfileSyncService::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  invalidator_state_ = state;
  UpdateInvalidatorRegistrarState();
}

void ProfileSyncService::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  invalidator_registrar_->DispatchInvalidationsToHandlers(invalidation_map);
}

void ProfileSyncService::OnBackendInitialized(
    const syncer::WeakHandle<syncer::JsBackend>& js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    bool success) {
  is_first_time_sync_configure_ = !HasSyncSetupCompleted();

  if (is_first_time_sync_configure_) {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeFirstTimeSuccess", success);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeRestoreSuccess", success);
  }

  if (!start_up_time_.is_null()) {
    base::Time on_backend_initialized_time = base::Time::Now();
    base::TimeDelta delta = on_backend_initialized_time - start_up_time_;
    if (is_first_time_sync_configure_) {
      UMA_HISTOGRAM_LONG_TIMES("Sync.BackendInitializeFirstTime", delta);
    } else {
      UMA_HISTOGRAM_LONG_TIMES("Sync.BackendInitializeRestoreTime", delta);
    }
    start_up_time_ = base::Time();
  }

  if (!success) {
    // Something went unexpectedly wrong.  Play it safe: stop syncing at once
    // and surface error UI to alert the user sync has stopped.
    // Keep the directory around for now so that on restart we will retry
    // again and potentially succeed in presence of transient file IO failures
    // or permissions issues, etc.
    //
    // TODO(rlarocque): Consider making this UnrecoverableError less special.
    // Unlike every other UnrecoverableError, it does not delete our sync data.
    // This exception made sense at the time it was implemented, but our new
    // directory corruption recovery mechanism makes it obsolete.  By the time
    // we get here, we will have already tried and failed to delete the
    // directory.  It would be no big deal if we tried to delete it again.
    OnInternalUnrecoverableError(FROM_HERE,
                                 "BackendInitialize failure",
                                 false,
                                 ERROR_REASON_BACKEND_INIT_FAILURE);
    return;
  }

  backend_initialized_ = true;
  UpdateInvalidatorRegistrarState();

  sync_js_controller_.AttachJsBackend(js_backend);
  debug_info_listener_ = debug_info_listener;

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
    const syncer::Experiments& experiments) {
  if (current_experiments_.Matches(experiments))
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

  const syncer::ModelTypeSet registered_types = GetRegisteredDataTypes();
  syncer::ModelTypeSet to_add;
  const syncer::ModelTypeSet to_register =
      Difference(to_add, registered_types);
  DVLOG(2) << "OnExperimentsChanged called with types: "
           << syncer::ModelTypeSetToString(to_add);
  DVLOG(2) << "Enabling types: " << syncer::ModelTypeSetToString(to_register);

  for (syncer::ModelTypeSet::Iterator it = to_register.First();
       it.Good(); it.Inc()) {
    // Received notice to enable experimental type. Check if the type is
    // registered, and if not register a new datatype controller.
    RegisterNewDataType(it.Get());
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
               << syncer::ModelTypeSetToString(to_register);
      OnMigrationNeededForTypes(to_register);
    }
  }

  // Now enable any non-datatype features.
  if (experiments.sync_tab_favicons) {
    DVLOG(1) << "Enabling syncing of tab favicons.";
    about_flags::SetExperimentEnabled(g_browser_process->local_state(),
                                      "sync-tab-favicons",
                                      true);
#if defined(OS_ANDROID)
    // Android does not support about:flags and experiments, so we need to force
    // setting the experiments as command line switches.
    CommandLine::ForCurrentProcess()->AppendSwitch(switches::kSyncTabFavicons);
#endif
  }

  if (experiments.keystore_encryption) {
    about_flags::SetExperimentEnabled(g_browser_process->local_state(),
                                      syncer::kKeystoreEncryptionFlag,
                                      true);
  }

  if (experiments.full_history_sync) {
    about_flags::SetExperimentEnabled(g_browser_process->local_state(),
                                      syncer::kFullHistorySyncFlag,
                                      true);
  }

  current_experiments_ = experiments;
}

void ProfileSyncService::UpdateAuthErrorState(const AuthError& error) {
  is_auth_in_progress_ = false;
  last_auth_error_ = error;

  // Fan the notification out to interested UI-thread components. Notify the
  // SigninGlobalError first so it reflects the latest auth state before we
  // notify observers.
  if (signin())
    signin()->signin_global_error()->AuthStatusChanged();
  NotifyObservers();
}

namespace {

AuthError ConnectionStatusToAuthError(
    syncer::ConnectionStatus status) {
  switch (status) {
    case syncer::CONNECTION_OK:
      return AuthError::None();
      break;
    case syncer::CONNECTION_AUTH_ERROR:
      return AuthError(AuthError::INVALID_GAIA_CREDENTIALS);
      break;
    case syncer::CONNECTION_SERVER_ERROR:
      return AuthError(AuthError::CONNECTION_FAILED);
      break;
    default:
      NOTREACHED();
      return AuthError(AuthError::CONNECTION_FAILED);
  }
}

}  // namespace

void ProfileSyncService::OnConnectionStatusChange(
    syncer::ConnectionStatus status) {
  const GoogleServiceAuthError auth_error =
      ConnectionStatusToAuthError(status);
  DVLOG(1) << "Connection status change: " << auth_error.ToString();
  UpdateAuthErrorState(auth_error);
}

void ProfileSyncService::OnStopSyncingPermanently() {
  UpdateAuthErrorState(AuthError(AuthError::SERVICE_UNAVAILABLE));
  sync_prefs_.SetStartSuppressed(true);
  DisableForUser();
  // If signout is allowed, signout the user on a dashboard clear.
  if (!auto_start_enabled_)  // Skip signout on ChromeOS/Android.
    signin_->SignOut();
}

void ProfileSyncService::OnPassphraseRequired(
    syncer::PassphraseRequiredReason reason,
    const sync_pb::EncryptedData& pending_keys) {
  DCHECK(backend_.get());
  DCHECK(backend_->IsNigoriEnabled());

  // TODO(lipalani) : add this check to other locations as well.
  if (HasUnrecoverableError()) {
    // When unrecoverable error is detected we post a task to shutdown the
    // backend. The task might not have executed yet.
    return;
  }

  DVLOG(1) << "Passphrase required with reason: "
           << syncer::PassphraseRequiredReasonToString(reason);
  passphrase_required_reason_ = reason;

  // Notify observers that the passphrase status may have changed.
  NotifyObservers();
}

void ProfileSyncService::OnPassphraseAccepted() {
  DVLOG(1) << "Received OnPassphraseAccepted.";

  // If the pending keys were resolved via keystore, it's possible we never
  // consumed our cached passphrase. Clear it now.
  if (!cached_passphrase_.empty())
    cached_passphrase_.clear();

  // Reset passphrase_required_reason_ since we know we no longer require the
  // passphrase. We do this here rather than down in ResolvePassphraseRequired()
  // because that can be called by OnPassphraseRequired() if no encrypted data
  // types are enabled, and we don't want to clobber the true passphrase error.
  passphrase_required_reason_ = syncer::REASON_PASSPHRASE_NOT_REQUIRED;

#if defined(OS_ANDROID)
  // Re-enable passwords if we have disabled them.
  if (failed_datatypes_handler_.GetFailedTypes().Has(syncer::PASSWORDS) &&
      ShouldEnablePasswordSyncForAndroid()) {
    // Clear the data type errors.
    failed_datatypes_handler_.OnUserChoseDatatypes();
  }
#endif

  // Make sure the data types that depend on the passphrase are started at
  // this time.
  const syncer::ModelTypeSet types = GetPreferredDataTypes();

  if (data_type_manager_.get()) {
    // Unblock the data type manager if necessary.
    data_type_manager_->Configure(types,
                                  syncer::CONFIGURE_REASON_RECONFIGURATION);
  }

  NotifyObservers();
}

void ProfileSyncService::OnEncryptedTypesChanged(
    syncer::ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  encrypted_types_ = encrypted_types;
  encrypt_everything_ = encrypt_everything;
  DVLOG(1) << "Encrypted types changed to "
           << syncer::ModelTypeSetToString(encrypted_types_)
           << " (encrypt everything is set to "
           << (encrypt_everything_ ? "true" : "false") << ")";
  DCHECK(encrypted_types_.Has(syncer::PASSWORDS));
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
    syncer::ModelTypeSet types) {
  DCHECK(backend_initialized_);
  DCHECK(data_type_manager_.get());

  // Migrator must be valid, because we don't sync until it is created and this
  // callback originates from a sync cycle.
  migrator_->MigrateTypes(types);
}

void ProfileSyncService::OnActionableError(const SyncProtocolError& error) {
  last_actionable_error_ = error;
  DCHECK_NE(last_actionable_error_.action,
            syncer::UNKNOWN_ACTION);
  switch (error.action) {
    case syncer::UPGRADE_CLIENT:
    case syncer::CLEAR_USER_DATA_AND_RESYNC:
    case syncer::ENABLE_SYNC_ON_ACCOUNT:
    case syncer::STOP_AND_RESTART_SYNC:
      // TODO(lipalani) : if setup in progress we want to display these
      // actions in the popup. The current experience might not be optimal for
      // the user. We just dismiss the dialog.
      if (setup_in_progress_) {
        OnStopSyncingPermanently();
        expect_sync_configuration_aborted_ = true;
      }
      // Trigger an unrecoverable error to stop syncing.
      OnInternalUnrecoverableError(FROM_HERE,
                                   last_actionable_error_.error_description,
                                   true,
                                   ERROR_REASON_ACTIONABLE_ERROR);
      break;
    case syncer::DISABLE_SYNC_ON_CLIENT:
      OnStopSyncingPermanently();
      break;
    default:
      NOTREACHED();
  }
  NotifyObservers();
}

void ProfileSyncService::OnConfigureBlocked() {
  NotifyObservers();
}

void ProfileSyncService::OnConfigureDone(
    const browser_sync::DataTypeManager::ConfigureResult& result) {
  // We should have cleared our cached passphrase before we get here (in
  // OnBackendInitialized()).
  DCHECK(cached_passphrase_.empty());

  if (!sync_configure_start_time_.is_null()) {
    if (result.status == DataTypeManager::OK ||
        result.status == DataTypeManager::PARTIAL_SUCCESS) {
      base::Time sync_configure_stop_time = base::Time::Now();
      base::TimeDelta delta = sync_configure_stop_time -
          sync_configure_start_time_;
      if (is_first_time_sync_configure_) {
        UMA_HISTOGRAM_LONG_TIMES("Sync.ServiceInitialConfigureTime", delta);
      } else {
        UMA_HISTOGRAM_LONG_TIMES("Sync.ServiceSubsequentConfigureTime",
                                  delta);
      }
    }
    sync_configure_start_time_ = base::Time();
  }

  // Notify listeners that configuration is done.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
      content::Source<ProfileSyncService>(this),
      content::NotificationService::NoDetails());

  configure_status_ = result.status;
  DVLOG(1) << "PSS OnConfigureDone called with status: " << configure_status_;
  // The possible status values:
  //    ABORT - Configuration was aborted. This is not an error, if
  //            initiated by user.
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

  // Handle unrecoverable error.
  if (configure_status_ != DataTypeManager::OK &&
      configure_status_ != DataTypeManager::PARTIAL_SUCCESS) {
    // Something catastrophic had happened. We should only have one
    // error representing it.
    DCHECK_EQ(result.failed_data_types.size(),
              static_cast<unsigned int>(1));
    syncer::SyncError error = result.failed_data_types.front();
    DCHECK(error.IsSet());
    std::string message =
        "Sync configuration failed with status " +
        DataTypeManager::ConfigureStatusToString(configure_status_) +
        " during " + syncer::ModelTypeToString(error.type()) +
        ": " + error.message();
    LOG(ERROR) << "ProfileSyncService error: "
               << message;
    OnInternalUnrecoverableError(error.location(),
                                 message,
                                 true,
                                 ERROR_REASON_CONFIGURATION_FAILURE);
    return;
  }

  // Now handle partial success and full success.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&ProfileSyncService::OnSyncConfigureDone,
                 weak_factory_.GetWeakPtr(), result));

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
    migrator_->OnConfigureDone(result);
  } else {
    StartSyncingWithServer();
  }
}

void ProfileSyncService::OnConfigureRetry() {
  // We should have cleared our cached passphrase before we get here (in
  // OnBackendInitialized()).
  DCHECK(cached_passphrase_.empty());

  OnSyncConfigureRetry();
}

void ProfileSyncService::OnConfigureStart() {
  sync_configure_start_time_ = base::Time::Now();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SYNC_CONFIGURE_START,
      content::Source<ProfileSyncService>(this),
      content::NotificationService::NoDetails());
  NotifyObservers();
}

std::string ProfileSyncService::QuerySyncStatusSummary() {
  if (HasUnrecoverableError()) {
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

bool ProfileSyncService::QueryDetailedSyncStatus(
    SyncBackendHost::Status* result) {
  if (backend_.get() && backend_initialized_) {
    *result = backend_->GetDetailedStatus();
    return true;
  } else {
    SyncBackendHost::Status status;
    status.sync_protocol_error = last_actionable_error_;
    *result = status;
    return false;
  }
}

const AuthError& ProfileSyncService::GetAuthError() const {
  return last_auth_error_;
}

GoogleServiceAuthError ProfileSyncService::GetAuthStatus() const {
  return GetAuthError();
}

bool ProfileSyncService::FirstSetupInProgress() const {
  return !HasSyncSetupCompleted() && setup_in_progress_;
}

void ProfileSyncService::SetSetupInProgress(bool setup_in_progress) {
  bool was_in_progress = setup_in_progress_;
  setup_in_progress_ = setup_in_progress;
  if (!setup_in_progress && was_in_progress) {
    if (sync_initialized()) {
      ReconfigureDatatypeManager();
    }
  }
  NotifyObservers();
}

bool ProfileSyncService::sync_initialized() const {
  return backend_initialized_;
}

bool ProfileSyncService::waiting_for_auth() const {
  return is_auth_in_progress_;
}

const syncer::Experiments& ProfileSyncService::current_experiments() const {
  return current_experiments_;
}

bool ProfileSyncService::HasUnrecoverableError() const {
  return unrecoverable_error_reason_ != ERROR_REASON_UNSET;
}

bool ProfileSyncService::IsPassphraseRequired() const {
  return passphrase_required_reason_ !=
      syncer::REASON_PASSPHRASE_NOT_REQUIRED;
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
    bool sync_everything, const syncer::ModelTypeSet chosen_types) const {
  if (!HasSyncSetupCompleted() ||
      sync_everything != sync_prefs_.HasKeepEverythingSynced()) {
    UMA_HISTOGRAM_BOOLEAN("Sync.SyncEverything", sync_everything);
  }

  // Only log the data types that are shown in the sync settings ui.
  // Note: the order of these types must match the ordering of
  // the respective types in ModelType
const browser_sync::user_selectable_type::UserSelectableSyncType
      user_selectable_types[] = {
    browser_sync::user_selectable_type::BOOKMARKS,
    browser_sync::user_selectable_type::PREFERENCES,
    browser_sync::user_selectable_type::PASSWORDS,
    browser_sync::user_selectable_type::AUTOFILL,
    browser_sync::user_selectable_type::THEMES,
    browser_sync::user_selectable_type::TYPED_URLS,
    browser_sync::user_selectable_type::EXTENSIONS,
    browser_sync::user_selectable_type::APPS,
    browser_sync::user_selectable_type::PROXY_TABS
  };

  COMPILE_ASSERT(26 == syncer::MODEL_TYPE_COUNT, UpdateCustomConfigHistogram);

  if (!sync_everything) {
    const syncer::ModelTypeSet current_types = GetPreferredDataTypes();

    syncer::ModelTypeSet type_set = syncer::UserSelectableTypes();
    syncer::ModelTypeSet::Iterator it = type_set.First();

    DCHECK_EQ(arraysize(user_selectable_types), type_set.Size());

    for (size_t i = 0; i < arraysize(user_selectable_types) && it.Good();
         ++i, it.Inc()) {
      const syncer::ModelType type = it.Get();
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
  syncer::Cryptographer temp_cryptographer(&encryptor);
  // The first 2 params (hostname and username) doesn't have any effect here.
  syncer::KeyParams key_params = {"localhost", "dummy", passphrase};

  std::string bootstrap_token;
  if (!temp_cryptographer.AddKey(key_params)) {
    NOTREACHED() << "Failed to add key to cryptographer.";
  }
  temp_cryptographer.GetBootstrapToken(&bootstrap_token);
  sync_prefs_.SetSpareBootstrapToken(bootstrap_token);
}
#endif

void ProfileSyncService::OnUserChoseDatatypes(
    bool sync_everything,
    syncer::ModelTypeSet chosen_types) {
  if (!backend_.get() && !HasUnrecoverableError()) {
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
    syncer::ModelTypeSet preferred_types) {

  DVLOG(1) << "ChangePreferredDataTypes invoked";
  const syncer::ModelTypeSet registered_types = GetRegisteredDataTypes();
  const syncer::ModelTypeSet registered_preferred_types =
      Intersection(registered_types, preferred_types);
  sync_prefs_.SetPreferredDataTypes(registered_types,
                                    registered_preferred_types);

  // Now reconfigure the DTM.
  ReconfigureDatatypeManager();
}

syncer::ModelTypeSet ProfileSyncService::GetPreferredDataTypes() const {
  const syncer::ModelTypeSet registered_types = GetRegisteredDataTypes();
  const syncer::ModelTypeSet preferred_types =
      sync_prefs_.GetPreferredDataTypes(registered_types);
  const syncer::ModelTypeSet failed_types =
      failed_datatypes_handler_.GetFailedTypes();
  return Difference(preferred_types, failed_types);
}

syncer::ModelTypeSet ProfileSyncService::GetRegisteredDataTypes() const {
  syncer::ModelTypeSet registered_types;
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
  syncer::PassphraseType passphrase_type = GetPassphraseType();
  return passphrase_type == syncer::FROZEN_IMPLICIT_PASSPHRASE ||
         passphrase_type == syncer::CUSTOM_PASSPHRASE;
}

syncer::PassphraseType ProfileSyncService::GetPassphraseType() const {
  return backend_->GetPassphraseType();
}

base::Time ProfileSyncService::GetExplicitPassphraseTime() const {
  return backend_->GetExplicitPassphraseTime();
}

bool ProfileSyncService::IsCryptographerReady(
    const syncer::BaseTransaction* trans) const {
  return backend_.get() && backend_->IsCryptographerReady(trans);
}

SyncBackendHost* ProfileSyncService::GetBackendForTest() {
  // We don't check |backend_initialized_|; we assume the test class
  // knows what it's doing.
  return backend_.get();
}

void ProfileSyncService::ConfigureDataTypeManager() {
  // Don't configure datatypes if the setup UI is still on the screen - this
  // is to help multi-screen setting UIs (like iOS) where they don't want to
  // start syncing data until the user is done configuring encryption options,
  // etc. ReconfigureDatatypeManager() will get called again once the UI calls
  // SetSetupInProgress(false).
  if (setup_in_progress_)
    return;

  bool restart = false;
  if (!data_type_manager_.get()) {
    restart = true;
    data_type_manager_.reset(
        factory_->CreateDataTypeManager(debug_info_listener_,
                                        backend_.get(),
                                        &data_type_controllers_,
                                        this,
                                        &failed_datatypes_handler_));

    // We create the migrator at the same time.
    migrator_.reset(
        new browser_sync::BackendMigrator(
            profile_->GetDebugName(), GetUserShare(),
            this, data_type_manager_.get(),
            base::Bind(&ProfileSyncService::StartSyncingWithServer,
                       base::Unretained(this))));
  }

#if defined(OS_ANDROID)
  if (GetPreferredDataTypes().Has(syncer::PASSWORDS) &&
      !ShouldEnablePasswordSyncForAndroid()) {
    DisableBrokenDatatype(syncer::PASSWORDS, FROM_HERE, "Not supported.");
  }
#endif

  const syncer::ModelTypeSet types = GetPreferredDataTypes();
  if (IsPassphraseRequiredForDecryption()) {
    // We need a passphrase still. We don't bother to attempt to configure
    // until we receive an OnPassphraseAccepted (which triggers a configure).
    DVLOG(1) << "ProfileSyncService::ConfigureDataTypeManager bailing out "
             << "because a passphrase required";
    NotifyObservers();
    return;
  }
  syncer::ConfigureReason reason = syncer::CONFIGURE_REASON_UNKNOWN;
  if (!HasSyncSetupCompleted()) {
    reason = syncer::CONFIGURE_REASON_NEW_CLIENT;
  } else if (restart) {
    // Datatype downloads on restart are generally due to newly supported
    // datatypes (although it's also possible we're picking up where a failed
    // previous configuration left off).
    // TODO(sync): consider detecting configuration recovery and setting
    // the reason here appropriately.
    reason = syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE;
  } else {
    // The user initiated a reconfiguration (either to add or remove types).
    reason = syncer::CONFIGURE_REASON_RECONFIGURATION;
  }

  data_type_manager_->Configure(types, reason);
}

syncer::UserShare* ProfileSyncService::GetUserShare() const {
  if (backend_.get() && backend_initialized_) {
    return backend_->GetUserShare();
  }
  NOTREACHED();
  return NULL;
}

syncer::sessions::SyncSessionSnapshot
    ProfileSyncService::GetLastSessionSnapshot() const {
  if (backend_.get() && backend_initialized_) {
    return backend_->GetLastSessionSnapshot();
  }
  NOTREACHED();
  return syncer::sessions::SyncSessionSnapshot();
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
    syncer::ModelSafeRoutingInfo* out) const {
  if (backend_.get() && backend_initialized_) {
    backend_->GetModelSafeRoutingInfo(out);
  } else {
    NOTREACHED();
  }
}

Value* ProfileSyncService::GetTypeStatusMap() const {
  scoped_ptr<ListValue> result(new ListValue());

  if (!backend_.get() || !backend_initialized_) {
    return result.release();
  }

  std::vector<syncer::SyncError> errors =
      failed_datatypes_handler_.GetAllErrors();
  std::map<ModelType, syncer::SyncError> error_map;
  for (std::vector<syncer::SyncError>::iterator it = errors.begin();
       it != errors.end(); ++it) {
    error_map[it->type()] = *it;
  }

  ModelTypeSet active_types;
  ModelTypeSet passive_types;
  ModelSafeRoutingInfo routing_info;
  backend_->GetModelSafeRoutingInfo(&routing_info);
  for (ModelSafeRoutingInfo::const_iterator it = routing_info.begin();
       it != routing_info.end(); ++it) {
    if (it->second == syncer::GROUP_PASSIVE) {
      passive_types.Put(it->first);
    } else {
      active_types.Put(it->first);
    }
  }

  SyncBackendHost::Status detailed_status = backend_->GetDetailedStatus();
  ModelTypeSet &throttled_types(detailed_status.throttled_types);
  ModelTypeSet registered = GetRegisteredDataTypes();
  scoped_ptr<DictionaryValue> type_status_header(new DictionaryValue());

  type_status_header->SetString("name", "Model Type");
  type_status_header->SetString("status", "header");
  type_status_header->SetString("value", "Group Type");
  type_status_header->SetString("num_entries", "Total Entries");
  type_status_header->SetString("num_live", "Live Entries");
  result->Append(type_status_header.release());

  scoped_ptr<DictionaryValue> type_status;
  for (ModelTypeSet::Iterator it = registered.First(); it.Good(); it.Inc()) {
    ModelType type = it.Get();

    type_status.reset(new DictionaryValue());
    type_status->SetString("name", ModelTypeToString(type));

    if (error_map.find(type) != error_map.end()) {
      const syncer::SyncError &error = error_map.find(type)->second;
      DCHECK(error.IsSet());
      std::string error_text = "Error: " + error.location().ToString() +
          ", " + error.message();
      type_status->SetString("status", "error");
      type_status->SetString("value", error_text);
    } else if (throttled_types.Has(type) && passive_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("value", "Passive, Throttled");
    } else if (passive_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("value", "Passive");
    } else if (throttled_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("value", "Throttled");
    } else if (active_types.Has(type)) {
      type_status->SetString("status", "ok");
      type_status->SetString("value", "Active: " +
                             ModelSafeGroupToString(routing_info[type]));
    } else {
      type_status->SetString("status", "warning");
      type_status->SetString("value", "Disabled by User");
    }

    int live_count = detailed_status.num_entries_by_type[type] -
        detailed_status.num_to_delete_entries_by_type[type];
    type_status->SetInteger("num_entries",
                            detailed_status.num_entries_by_type[type]);
    type_status->SetInteger("num_live", live_count);

    result->Append(type_status.release());
  }
  return result.release();
}

void ProfileSyncService::ActivateDataType(
    syncer::ModelType type, syncer::ModelSafeGroup group,
    ChangeProcessor* change_processor) {
  if (!backend_.get()) {
    NOTREACHED();
    return;
  }
  DCHECK(backend_initialized_);
  backend_->ActivateDataType(type, group, change_processor);
}

void ProfileSyncService::DeactivateDataType(syncer::ModelType type) {
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
  if (passphrase_required_reason() == syncer::REASON_DECRYPTION) {
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
  DCHECK(!(type == EXPLICIT &&
           passphrase_required_reason_ == syncer::REASON_DECRYPTION)) <<
         "Can not set explicit passphrase when decryption is needed.";

  DVLOG(1) << "Setting " << (type == EXPLICIT ? "explicit" : "implicit")
           << " passphrase for encryption.";
  if (passphrase_required_reason_ == syncer::REASON_ENCRYPTION) {
    // REASON_ENCRYPTION implies that the cryptographer does not have pending
    // keys. Hence, as long as we're not trying to do an invalid passphrase
    // change (e.g. explicit -> explicit or explicit -> implicit), we know this
    // will succeed. If for some reason a new encryption key arrives via
    // sync later, the SBH will trigger another OnPassphraseRequired().
    passphrase_required_reason_ = syncer::REASON_PASSPHRASE_NOT_REQUIRED;
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

syncer::ModelTypeSet ProfileSyncService::GetEncryptedDataTypes() const {
  DCHECK(encrypted_types_.Has(syncer::PASSWORDS));
  // We may be called during the setup process before we're
  // initialized.  In this case, we default to the sensitive types.
  return encrypted_types_;
}

void ProfileSyncService::OnSyncManagedPrefChange(bool is_sync_managed) {
  NotifyObservers();
  if (is_sync_managed) {
    DisableForUser();
  } else {
    // Sync is no longer disabled by policy. Try starting it up if appropriate.
    TryStart();
  }
}

void ProfileSyncService::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL: {
      const GoogleServiceSigninSuccessDetails* successful =
          content::Details<const GoogleServiceSigninSuccessDetails>(
              details).ptr();
      if (!sync_prefs_.IsStartSuppressed() &&
          !successful->password.empty()) {
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
          GetAuthError().state() != AuthError::NONE) {
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
          !IsSyncTokenAvailable()) {
        // The additional check around IsSyncTokenAvailable() above prevents us
        // sounding the alarm if we actually have a valid token but a refresh
        // attempt by TokenService failed for any variety of reasons (e.g. flaky
        // network). It's possible the token we do have is also invalid, but in
        // that case we should already have (or can expect) an auth error sent
        // from the sync backend.
        AuthError error(AuthError::INVALID_GAIA_CREDENTIALS);
        UpdateAuthErrorState(error);
      }
      break;
    }
    case chrome::NOTIFICATION_TOKEN_AVAILABLE: {
      const TokenService::TokenAvailableDetails& token_details =
          *(content::Details<const TokenService::TokenAvailableDetails>(
              details).ptr());
      if (!IsTokenServiceRelevant(token_details.service()))
        break;
    } // Fall through.
    case chrome::NOTIFICATION_TOKEN_LOADING_FINISHED: {
      // This notification gets fired when TokenService loads the tokens
      // from storage.
      // Initialize the backend if sync is enabled. If the sync token was
      // not loaded, GetCredentials() will generate invalid credentials to
      // cause the backend to generate an auth error (crbug.com/121755).
      if (backend_.get())
        backend_->UpdateCredentials(GetCredentials());
      else
        TryStart();
      break;
    }
    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT:
      // Disable sync if the user is signed out.
      DisableForUser();
      break;
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

base::WeakPtr<syncer::JsController> ProfileSyncService::GetJsController() {
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
  if (HasUnrecoverableError())
    return false;

  if (!data_type_manager_.get())
    return false;

  return data_type_manager_->state() == DataTypeManager::CONFIGURED;
}

void ProfileSyncService::StopAndSuppress() {
  sync_prefs_.SetStartSuppressed(true);
  ShutdownImpl(false);
}

bool ProfileSyncService::IsStartSuppressed() const {
  return sync_prefs_.IsStartSuppressed();
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
  } else if (HasUnrecoverableError()) {
    // There is nothing more to configure. So inform the listeners,
    NotifyObservers();

    DVLOG(1) << "ConfigureDataTypeManager not invoked because of an "
             << "Unrecoverable error.";
  } else {
    DVLOG(0) << "ConfigureDataTypeManager not invoked because backend is not "
             << "initialized";
  }
}

const FailedDatatypesHandler& ProfileSyncService::failed_datatypes_handler()
    const {
  return failed_datatypes_handler_;
}

void ProfileSyncService::OnInternalUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message,
    bool delete_sync_database,
    UnrecoverableErrorReason reason) {
  DCHECK(!HasUnrecoverableError());
  unrecoverable_error_reason_ = reason;
  OnUnrecoverableErrorImpl(from_here, message, delete_sync_database);
}

void ProfileSyncService::UpdateInvalidatorRegistrarState() {
  const syncer::InvalidatorState effective_state =
      backend_initialized_ ?
      invalidator_state_ : syncer::TRANSIENT_INVALIDATION_ERROR;
  DVLOG(1) << "New invalidator state: "
           << syncer::InvalidatorStateToString(invalidator_state_)
           << ", effective state: "
           << syncer::InvalidatorStateToString(effective_state);
  invalidator_registrar_->UpdateInvalidatorState(effective_state);
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
