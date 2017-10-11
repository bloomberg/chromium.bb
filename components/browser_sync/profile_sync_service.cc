// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/profile_sync_service.h"

#include <stddef.h>

#include <cstddef>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/history/core/browser/typed_url_data_type_controller.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/reading_list/features/reading_list_enable_flags.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/base/cryptographer.h"
#include "components/sync/base/passphrase_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/base/stop_source.h"
#include "components/sync/base/system_encryptor.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/device_info/device_info_sync_bridge.h"
#include "components/sync/device_info/device_info_tracker.h"
#include "components/sync/driver/backend_migrator.h"
#include "components/sync/driver/directory_data_type_controller.h"
#include "components/sync/driver/glue/sync_backend_host_impl.h"
#include "components/sync/driver/signin_manager_wrapper.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_error_controller.h"
#include "components/sync/driver/sync_type_preference_provider.h"
#include "components/sync/driver/sync_util.h"
#include "components/sync/driver/user_selectable_sync_type.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/cycle/model_neutral_state.h"
#include "components/sync/engine/cycle/type_debug_info_observer.h"
#include "components/sync/engine/net/http_bridge_network_resources.h"
#include "components/sync/engine/net/network_resources.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/js/js_event_details.h"
#include "components/sync/model/change_processor.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/sync_db_util.h"
#include "components/sync/syncable/syncable_read_transaction.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_sessions/favicon_cache.h"
#include "components/sync_sessions/session_data_type_controller.h"
#include "components/sync_sessions/sessions_sync_manager.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/version_info/version_info_values.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_ANDROID)
#include "components/sync/syncable/read_transaction.h"
#endif

using sync_sessions::SessionsSyncManager;
using syncer::BackendMigrator;
using syncer::ChangeProcessor;
using syncer::DataTypeController;
using syncer::DataTypeManager;
using syncer::DataTypeStatusTable;
using syncer::DeviceInfoSyncBridge;
using syncer::JsBackend;
using syncer::JsController;
using syncer::JsEventDetails;
using syncer::JsEventHandler;
using syncer::ModelSafeRoutingInfo;
using syncer::ModelType;
using syncer::ModelTypeChangeProcessor;
using syncer::ModelTypeSet;
using syncer::ModelTypeStore;
using syncer::ProtocolEventObserver;
using syncer::SyncEngine;
using syncer::SyncCredentials;
using syncer::SyncProtocolError;
using syncer::WeakHandle;

namespace browser_sync {

namespace {

using AuthError = GoogleServiceAuthError;

const char kSyncUnrecoverableErrorHistogram[] = "Sync.UnrecoverableErrors";

const net::BackoffEntry::Policy kRequestAccessTokenBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    2000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    // TODO(pavely): crbug.com/246686 ProfileSyncService should retry
    // RequestAccessToken on connection state change after backoff
    1000 * 3600 * 4,  // 4 hours.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

}  // namespace

ProfileSyncService::InitParams::InitParams() = default;
ProfileSyncService::InitParams::InitParams(InitParams&& other) = default;
ProfileSyncService::InitParams::~InitParams() = default;

ProfileSyncService::ProfileSyncService(InitParams init_params)
    : SyncServiceBase(std::move(init_params.sync_client),
                      std::move(init_params.signin_wrapper),
                      init_params.channel,
                      init_params.base_directory,
                      init_params.debug_identifier),
      OAuth2TokenService::Consumer("sync"),
      last_auth_error_(AuthError::AuthErrorNone()),
      sync_service_url_(
          syncer::GetSyncServiceURL(*base::CommandLine::ForCurrentProcess(),
                                    init_params.channel)),
      network_time_update_callback_(
          std::move(init_params.network_time_update_callback)),
      url_request_context_(init_params.url_request_context),
      is_first_time_sync_configure_(false),
      engine_initialized_(false),
      sync_disabled_by_admin_(false),
      is_auth_in_progress_(false),
      unrecoverable_error_reason_(ERROR_REASON_UNSET),
      expect_sync_configuration_aborted_(false),
      configure_status_(DataTypeManager::UNKNOWN),
      oauth2_token_service_(init_params.oauth2_token_service),
      request_access_token_backoff_(&kRequestAccessTokenBackoffPolicy),
      connection_status_(syncer::CONNECTION_NOT_ATTEMPTED),
      last_get_token_error_(GoogleServiceAuthError::AuthErrorNone()),
      gaia_cookie_manager_service_(init_params.gaia_cookie_manager_service),
      network_resources_(new syncer::HttpBridgeNetworkResources),
      start_behavior_(init_params.start_behavior),
      passphrase_prompt_triggered_by_version_(false),
      sync_enabled_weak_factory_(this),
      weak_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(sync_client_);
  std::string last_version = sync_prefs_.GetLastRunVersion();
  std::string current_version = PRODUCT_VERSION;
  sync_prefs_.SetLastRunVersion(current_version);

  // Check for a major version change. Note that the versions have format
  // MAJOR.MINOR.BUILD.PATCH.
  if (last_version.substr(0, last_version.find('.')) !=
      current_version.substr(0, current_version.find('.'))) {
    passphrase_prompt_triggered_by_version_ = true;
  }
}

ProfileSyncService::~ProfileSyncService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (gaia_cookie_manager_service_)
    gaia_cookie_manager_service_->RemoveObserver(this);
  sync_prefs_.RemoveSyncPrefObserver(this);
  // Shutdown() should have been called before destruction.
  DCHECK(!engine_initialized_);
}

bool ProfileSyncService::CanSyncStart() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (IsSyncAllowed() && IsSyncRequested() &&
          (IsLocalSyncEnabled() || IsSignedIn()));
}

void ProfileSyncService::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_client_->Initialize();

  // We don't pass StartupController an Unretained reference to future-proof
  // against the controller impl changing to post tasks.
  startup_controller_ = std::make_unique<syncer::StartupController>(
      &sync_prefs_,
      base::Bind(&ProfileSyncService::CanEngineStart, base::Unretained(this)),
      base::Bind(&ProfileSyncService::StartUpSlowEngineComponents,
                 weak_factory_.GetWeakPtr()));
  sync_sessions::LocalSessionEventRouter* router =
      sync_client_->GetSyncSessionsClient()->GetLocalSessionEventRouter();
  local_device_ = sync_client_->GetSyncApiComponentFactory()
                      ->CreateLocalDeviceInfoProvider();
  sync_stopped_reporter_ = std::make_unique<syncer::SyncStoppedReporter>(
      sync_service_url_, local_device_->GetSyncUserAgent(),
      url_request_context_, syncer::SyncStoppedReporter::ResultCallback());
  sessions_sync_manager_ = std::make_unique<SessionsSyncManager>(
      sync_client_->GetSyncSessionsClient(), &sync_prefs_, local_device_.get(),
      router,
      base::Bind(&ProfileSyncService::NotifyForeignSessionUpdated,
                 sync_enabled_weak_factory_.GetWeakPtr()),
      base::Bind(&ProfileSyncService::TriggerRefresh,
                 sync_enabled_weak_factory_.GetWeakPtr(),
                 syncer::ModelTypeSet(syncer::SESSIONS)));

  const syncer::ModelTypeStoreFactory& store_factory =
      GetModelTypeStoreFactory(syncer::DEVICE_INFO, base_directory_);
  device_info_sync_bridge_ = std::make_unique<DeviceInfoSyncBridge>(
      local_device_.get(), store_factory,
      base::BindRepeating(
          &ModelTypeChangeProcessor::Create,
          base::BindRepeating(&syncer::ReportUnrecoverableError, channel_)));

  syncer::SyncApiComponentFactory::RegisterDataTypesMethod
      register_platform_types_callback =
          sync_client_->GetRegisterPlatformTypesCallback();
  sync_client_->GetSyncApiComponentFactory()->RegisterDataTypes(
      this, register_platform_types_callback);

  if (gaia_cookie_manager_service_)
    gaia_cookie_manager_service_->AddObserver(this);

  // We clear this here (vs Shutdown) because we want to remember that an error
  // happened on shutdown so we can display details (message, location) about it
  // in about:sync.
  ClearStaleErrors();

  sync_prefs_.AddSyncPrefObserver(this);

  SyncInitialState sync_state = CAN_START;
  if (!IsLocalSyncEnabled() && !IsSignedIn()) {
    sync_state = NOT_SIGNED_IN;
  } else if (IsManaged()) {
    sync_state = IS_MANAGED;
  } else if (!IsSyncAllowedByPlatform()) {
    // This case should currently never be hit, as Android's master sync isn't
    // plumbed into PSS until after this function. See http://crbug.com/568771.
    sync_state = NOT_ALLOWED_BY_PLATFORM;
  } else if (!IsSyncRequested()) {
    if (IsFirstSetupComplete()) {
      sync_state = NOT_REQUESTED;
    } else {
      sync_state = NOT_REQUESTED_NOT_SETUP;
    }
  } else if (!IsFirstSetupComplete()) {
    sync_state = NEEDS_CONFIRMATION;
  }
  UMA_HISTOGRAM_ENUMERATION("Sync.InitialState", sync_state,
                            SYNC_INITIAL_STATE_LIMIT);

  // If sync isn't allowed, the only thing to do is to turn it off.
  if (!IsSyncAllowed()) {
    // Only clear data if disallowed by policy.
    RequestStop(IsManaged() ? CLEAR_DATA : KEEP_DATA);
    return;
  }

  if (!IsLocalSyncEnabled()) {
    RegisterAuthNotifications();

    if (!IsSignedIn()) {
      // Clean up in case of previous crash during signout.
      StopImpl(CLEAR_DATA);
    }
  }

#if defined(OS_CHROMEOS)
  std::string bootstrap_token = sync_prefs_.GetEncryptionBootstrapToken();
  if (bootstrap_token.empty()) {
    sync_prefs_.SetEncryptionBootstrapToken(
        sync_prefs_.GetSpareBootstrapToken());
  }
#endif

#if !defined(OS_ANDROID)
  DCHECK(sync_error_controller_ == nullptr)
      << "Initialize() called more than once.";
  sync_error_controller_ = std::make_unique<syncer::SyncErrorController>(this);
  AddObserver(sync_error_controller_.get());
#endif

  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      base::Bind(&ProfileSyncService::OnMemoryPressure,
                 sync_enabled_weak_factory_.GetWeakPtr()));
  startup_controller_->Reset(GetRegisteredDataTypes());

  // Auto-start means the first time the profile starts up, sync should start up
  // immediately.
  if (start_behavior_ == AUTO_START && IsSyncRequested() &&
      !IsFirstSetupComplete()) {
    startup_controller_->TryStartImmediately();
  } else {
    startup_controller_->TryStart();
  }
}

void ProfileSyncService::StartSyncingWithServer() {
  if (base::FeatureList::IsEnabled(
          switches::kSyncClearDataOnPassphraseEncryption) &&
      sync_prefs_.GetPassphraseEncryptionTransitionInProgress()) {
    // We are restarting catchup configuration after browser restart.
    UMA_HISTOGRAM_ENUMERATION("Sync.ClearServerDataEvents",
                              syncer::CLEAR_SERVER_DATA_RETRIED,
                              syncer::CLEAR_SERVER_DATA_MAX);

    crypto_->BeginConfigureCatchUpBeforeClear();
    return;
  }

  if (engine_)
    engine_->StartSyncingWithServer();
}

void ProfileSyncService::RegisterAuthNotifications() {
  DCHECK(thread_checker_.CalledOnValidThread());
  oauth2_token_service_->AddObserver(this);
  if (signin())
    signin()->AddObserver(this);
}

void ProfileSyncService::UnregisterAuthNotifications() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (signin())
    signin()->RemoveObserver(this);
  if (oauth2_token_service_)
    oauth2_token_service_->RemoveObserver(this);
}

void ProfileSyncService::RegisterDataTypeController(
    std::unique_ptr<syncer::DataTypeController> data_type_controller) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(data_type_controllers_.count(data_type_controller->type()), 0U);
  data_type_controllers_[data_type_controller->type()] =
      std::move(data_type_controller);
}

bool ProfileSyncService::IsDataTypeControllerRunning(
    syncer::ModelType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DataTypeController::TypeMap::const_iterator iter =
      data_type_controllers_.find(type);
  if (iter == data_type_controllers_.end()) {
    return false;
  }
  return iter->second->state() == DataTypeController::RUNNING;
}

sync_sessions::OpenTabsUIDelegate* ProfileSyncService::GetOpenTabsUIDelegate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Although the backing data actually is of type |SESSIONS|, the desire to use
  // open tabs functionality is tracked by the state of the |PROXY_TABS| type.
  return IsDataTypeControllerRunning(syncer::PROXY_TABS)
             ? sessions_sync_manager_.get()
             : nullptr;
}

sync_sessions::FaviconCache* ProfileSyncService::GetFaviconCache() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return sessions_sync_manager_->GetFaviconCache();
}

syncer::DeviceInfoTracker* ProfileSyncService::GetDeviceInfoTracker() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return device_info_sync_bridge_.get();
}

syncer::LocalDeviceInfoProvider*
ProfileSyncService::GetLocalDeviceInfoProvider() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return local_device_.get();
}

void ProfileSyncService::GetDataTypeControllerStates(
    DataTypeController::StateMap* state_map) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (DataTypeController::TypeMap::const_iterator iter =
           data_type_controllers_.begin();
       iter != data_type_controllers_.end(); ++iter)
    (*state_map)[iter->first] = iter->second.get()->state();
}

void ProfileSyncService::OnSessionRestoreComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DataTypeController::TypeMap::const_iterator iter =
      data_type_controllers_.find(syncer::SESSIONS);
  if (iter == data_type_controllers_.end()) {
    return;
  }
  DCHECK(iter->second);

  static_cast<sync_sessions::SessionDataTypeController*>(iter->second.get())
      ->OnSessionRestoreComplete();
}

SyncCredentials ProfileSyncService::GetCredentials() {
  SyncCredentials credentials;

  // No credentials exist or are needed for the local sync backend.
  if (IsLocalSyncEnabled())
    return credentials;

  credentials.account_id = signin_->GetAccountIdToUse();
  DCHECK(!credentials.account_id.empty());
  credentials.email = signin_->GetEffectiveUsername();
  credentials.sync_token = access_token_;

  if (credentials.sync_token.empty())
    credentials.sync_token = "credentials_lost";

  credentials.scope_set.insert(signin_->GetSyncScopeToUse());

  return credentials;
}

syncer::WeakHandle<syncer::JsEventHandler>
ProfileSyncService::GetJsEventHandler() {
  return syncer::MakeWeakHandle(sync_js_controller_.AsWeakPtr());
}

syncer::SyncEngine::HttpPostProviderFactoryGetter
ProfileSyncService::MakeHttpPostProviderFactoryGetter() {
  return base::Bind(&syncer::NetworkResources::GetHttpPostProviderFactory,
                    base::Unretained(network_resources_.get()),
                    url_request_context_, network_time_update_callback_);
}

syncer::WeakHandle<syncer::UnrecoverableErrorHandler>
ProfileSyncService::GetUnrecoverableErrorHandler() {
  return syncer::MakeWeakHandle(sync_enabled_weak_factory_.GetWeakPtr());
}

bool ProfileSyncService::IsEncryptedDatatypeEnabled() const {
  if (encryption_pending())
    return true;
  const syncer::ModelTypeSet preferred_types = GetPreferredDataTypes();
  const syncer::ModelTypeSet encrypted_types = GetEncryptedDataTypes();
  DCHECK(encrypted_types.Has(syncer::PASSWORDS));
  return !Intersection(preferred_types, encrypted_types).Empty();
}

void ProfileSyncService::OnProtocolEvent(const syncer::ProtocolEvent& event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : protocol_event_observers_)
    observer.OnProtocolEvent(event);
}

void ProfileSyncService::OnDirectoryTypeCommitCounterUpdated(
    syncer::ModelType type,
    const syncer::CommitCounters& counters) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : type_debug_info_observers_)
    observer.OnCommitCountersUpdated(type, counters);
}

void ProfileSyncService::OnDirectoryTypeUpdateCounterUpdated(
    syncer::ModelType type,
    const syncer::UpdateCounters& counters) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : type_debug_info_observers_)
    observer.OnUpdateCountersUpdated(type, counters);
}

void ProfileSyncService::OnDatatypeStatusCounterUpdated(
    syncer::ModelType type,
    const syncer::StatusCounters& counters) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : type_debug_info_observers_)
    observer.OnStatusCountersUpdated(type, counters);
}

void ProfileSyncService::OnDataTypeRequestsSyncStartup(syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(syncer::UserTypes().Has(type));

  if (!GetPreferredDataTypes().Has(type)) {
    // We can get here as datatype SyncableServices are typically wired up
    // to the native datatype even if sync isn't enabled.
    DVLOG(1) << "Dropping sync startup request because type "
             << syncer::ModelTypeToString(type) << "not enabled.";
    return;
  }

  // If this is a data type change after a major version update, reset the
  // passphrase prompted state and notify observers.
  if (IsPassphraseRequired() && passphrase_prompt_triggered_by_version_) {
    // The major version has changed and a local syncable change was made.
    // Reset the passphrase prompt state.
    passphrase_prompt_triggered_by_version_ = false;
    sync_prefs_.SetPassphrasePrompted(false);
    NotifyObservers();
  }

  if (engine_) {
    DVLOG(1) << "A data type requested sync startup, but it looks like "
                "something else beat it to the punch.";
    return;
  }

  startup_controller_->OnDataTypeRequestsSyncStartup(type);
}

void ProfileSyncService::StartUpSlowEngineComponents() {
  invalidation::InvalidationService* invalidator =
      sync_client_->GetInvalidationService();

  engine_.reset(sync_client_->GetSyncApiComponentFactory()->CreateSyncEngine(
      debug_identifier_, invalidator, sync_prefs_.AsWeakPtr(),
      FormatSyncDataPath(base_directory_)));

  // Clear any old errors the first time sync starts.
  if (!IsFirstSetupComplete())
    ClearStaleErrors();

  InitializeEngine();

  UpdateFirstSyncTimePref();

  ReportPreviousSessionMemoryWarningCount();
}

void ProfileSyncService::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(access_token_request_.get(), request);
  access_token_request_.reset();
  access_token_ = access_token;
  token_receive_time_ = base::Time::Now();
  last_get_token_error_ = GoogleServiceAuthError::AuthErrorNone();

  if (sync_prefs_.SyncHasAuthError()) {
    sync_prefs_.SetSyncAuthError(false);
  }

  if (HasSyncingEngine())
    engine_->UpdateCredentials(GetCredentials());
  else
    startup_controller_->TryStart();
}

void ProfileSyncService::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(access_token_request_.get(), request);
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);
  access_token_request_.reset();
  last_get_token_error_ = error;
  switch (error.state()) {
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE: {
      // Transient error. Retry after some time.
      request_access_token_backoff_.InformOfRequest(false);
      next_token_request_time_ =
          base::Time::Now() +
          request_access_token_backoff_.GetTimeUntilRelease();
      request_access_token_retry_timer_.Start(
          FROM_HERE, request_access_token_backoff_.GetTimeUntilRelease(),
          base::Bind(&ProfileSyncService::RequestAccessToken,
                     sync_enabled_weak_factory_.GetWeakPtr()));
      NotifyObservers();
      break;
    }
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS: {
      if (!sync_prefs_.SyncHasAuthError()) {
        sync_prefs_.SetSyncAuthError(true);
        UMA_HISTOGRAM_ENUMERATION("Sync.SyncAuthError", AUTH_ERROR_ENCOUNTERED,
                                  AUTH_ERROR_LIMIT);
      }
      // Fallthrough.
    }
    default: {
      if (error.state() != GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
        LOG(ERROR) << "Unexpected persistent error: " << error.ToString();
      }
      // Show error to user.
      UpdateAuthErrorState(error);
    }
  }
}

void ProfileSyncService::OnRefreshTokenAvailable(
    const std::string& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (account_id == signin_->GetAccountIdToUse())
    OnRefreshTokensLoaded();
}

void ProfileSyncService::OnRefreshTokenRevoked(const std::string& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (account_id == signin_->GetAccountIdToUse()) {
    access_token_.clear();
    UpdateAuthErrorState(
        GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
  }
}

void ProfileSyncService::OnRefreshTokensLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // This notification gets fired when OAuth2TokenService loads the tokens from
  // storage. Initialize the engine if sync is enabled. If the sync token was
  // not loaded, GetCredentials() will generate invalid credentials to cause the
  // engine to generate an auth error (crbug.com/121755).
  if (HasSyncingEngine()) {
    RequestAccessToken();
  } else {
    startup_controller_->TryStart();
  }
}

void ProfileSyncService::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  UnregisterAuthNotifications();

  ShutdownImpl(syncer::BROWSER_SHUTDOWN);
  NotifyShutdown();
  if (sync_error_controller_) {
    // Destroy the SyncErrorController when the service shuts down for good.
    RemoveObserver(sync_error_controller_.get());
    sync_error_controller_.reset();
  }

  if (sync_thread_)
    sync_thread_->Stop();
}

void ProfileSyncService::ShutdownImpl(syncer::ShutdownReason reason) {
  if (!engine_) {
    if (reason == syncer::ShutdownReason::DISABLE_SYNC && sync_thread_) {
      // If the engine is already shut down when a DISABLE_SYNC happens,
      // the data directory needs to be cleaned up here.
      sync_thread_->task_runner()->PostTask(
          FROM_HERE,
          base::Bind(&syncer::syncable::Directory::DeleteDirectoryFiles,
                     FormatSyncDataPath(base_directory_)));
    }
    return;
  }

  if (reason == syncer::ShutdownReason::STOP_SYNC ||
      reason == syncer::ShutdownReason::DISABLE_SYNC) {
    RemoveClientFromServer();
  }

  // First, we spin down the engine to stop change processing as soon as
  // possible.
  base::Time shutdown_start_time = base::Time::Now();
  engine_->StopSyncingForShutdown();

  // Stop all data type controllers, if needed. Note that until Stop completes,
  // it is possible in theory to have a ChangeProcessor apply a change from a
  // native model. In that case, it will get applied to the sync database (which
  // doesn't get destroyed until we destroy the engine below) as an unsynced
  // change. That will be persisted, and committed on restart.
  if (data_type_manager_) {
    if (data_type_manager_->state() != DataTypeManager::STOPPED) {
      // When aborting as part of shutdown, we should expect an aborted sync
      // configure result, else we'll dcheck when we try to read the sync error.
      expect_sync_configuration_aborted_ = true;
      data_type_manager_->Stop();
    }
    data_type_manager_.reset();
  }

  // Shutdown the migrator before the engine to ensure it doesn't pull a null
  // snapshot.
  migrator_.reset();
  sync_js_controller_.AttachJsBackend(WeakHandle<syncer::JsBackend>());

  engine_->Shutdown(reason);
  engine_.reset();

  base::TimeDelta shutdown_time = base::Time::Now() - shutdown_start_time;
  UMA_HISTOGRAM_TIMES("Sync.Shutdown.BackendDestroyedTime", shutdown_time);

  sync_enabled_weak_factory_.InvalidateWeakPtrs();

  startup_controller_->Reset(GetRegisteredDataTypes());

  // If the sync DB is getting destroyed, the local DeviceInfo is no longer
  // valid and should be cleared from the cache.
  if (reason == syncer::ShutdownReason::DISABLE_SYNC) {
    local_device_->Clear();
  }

  // Clear various state.
  ResetCryptoState();
  expect_sync_configuration_aborted_ = false;
  is_auth_in_progress_ = false;
  engine_initialized_ = false;
  access_token_.clear();
  request_access_token_retry_timer_.Stop();
  last_snapshot_ = syncer::SyncCycleSnapshot();
  // Revert to "no auth error".
  if (last_auth_error_.state() != GoogleServiceAuthError::NONE)
    UpdateAuthErrorState(GoogleServiceAuthError::AuthErrorNone());

  NotifyObservers();

  // Mark this as a clean shutdown(without crash).
  sync_prefs_.SetCleanShutdown(true);
}

void ProfileSyncService::StopImpl(SyncStopDataFate data_fate) {
  switch (data_fate) {
    case KEEP_DATA:
      ShutdownImpl(syncer::STOP_SYNC);
      break;
    case CLEAR_DATA:
      // Clear prefs (including SyncSetupHasCompleted) before shutting down so
      // PSS clients don't think we're set up while we're shutting down.
      sync_prefs_.ClearPreferences();
      ClearUnrecoverableError();
      ShutdownImpl(syncer::DISABLE_SYNC);
      break;
  }
}

bool ProfileSyncService::IsFirstSetupComplete() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return sync_prefs_.IsFirstSetupComplete();
}

void ProfileSyncService::SetFirstSetupComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_prefs_.SetFirstSetupComplete();
  if (IsEngineInitialized()) {
    ReconfigureDatatypeManager();
  }
}

bool ProfileSyncService::IsSyncConfirmationNeeded() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (!IsLocalSyncEnabled() && IsSignedIn()) && !IsFirstSetupInProgress() &&
         !IsFirstSetupComplete() && IsSyncRequested();
}

void ProfileSyncService::UpdateLastSyncedTime() {
  sync_prefs_.SetLastSyncedTime(base::Time::Now());
}

void ProfileSyncService::NotifySyncCycleCompleted() {
  for (auto& observer : observers_)
    observer.OnSyncCycleCompleted(this);
}

void ProfileSyncService::NotifyForeignSessionUpdated() {
  for (auto& observer : observers_)
    observer.OnForeignSessionUpdated(this);
}

void ProfileSyncService::NotifyShutdown() {
  for (auto& observer : observers_)
    observer.OnSyncShutdown(this);
}

void ProfileSyncService::ClearStaleErrors() {
  ClearUnrecoverableError();
  last_actionable_error_ = SyncProtocolError();
  // Clear the data type errors as well.
  if (data_type_manager_.get())
    data_type_manager_->ResetDataTypeErrors();
}

void ProfileSyncService::ClearUnrecoverableError() {
  unrecoverable_error_reason_ = ERROR_REASON_UNSET;
  unrecoverable_error_message_.clear();
  unrecoverable_error_location_ = base::Location();
}

// An invariant has been violated.  Transition to an error state where we try
// to do as little work as possible, to avoid further corruption or crashes.
void ProfileSyncService::OnUnrecoverableError(const base::Location& from_here,
                                              const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Unrecoverable errors that arrive via the syncer::UnrecoverableErrorHandler
  // interface are assumed to originate within the syncer.
  unrecoverable_error_reason_ = ERROR_REASON_SYNCER;
  OnUnrecoverableErrorImpl(from_here, message, true);
}

void ProfileSyncService::OnUnrecoverableErrorImpl(
    const base::Location& from_here,
    const std::string& message,
    bool delete_sync_database) {
  DCHECK(HasUnrecoverableError());
  unrecoverable_error_message_ = message;
  unrecoverable_error_location_ = from_here;

  UMA_HISTOGRAM_ENUMERATION(kSyncUnrecoverableErrorHistogram,
                            unrecoverable_error_reason_, ERROR_REASON_LIMIT);
  LOG(ERROR) << "Unrecoverable error detected at " << from_here.ToString()
             << " -- ProfileSyncService unusable: " << message;

  // Shut all data types down.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ProfileSyncService::ShutdownImpl,
                            sync_enabled_weak_factory_.GetWeakPtr(),
                            delete_sync_database ? syncer::DISABLE_SYNC
                                                 : syncer::STOP_SYNC));
}

void ProfileSyncService::ReenableDatatype(syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!engine_initialized_ || !data_type_manager_)
    return;
  data_type_manager_->ReenableType(type);
}

void ProfileSyncService::UpdateEngineInitUMA(bool success) {
  is_first_time_sync_configure_ = !IsFirstSetupComplete();

  if (is_first_time_sync_configure_) {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeFirstTimeSuccess", success);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeRestoreSuccess", success);
  }

  base::Time on_engine_initialized_time = base::Time::Now();
  base::TimeDelta delta =
      on_engine_initialized_time - startup_controller_->start_engine_time();
  if (is_first_time_sync_configure_) {
    UMA_HISTOGRAM_LONG_TIMES("Sync.BackendInitializeFirstTime", delta);
  } else {
    UMA_HISTOGRAM_LONG_TIMES("Sync.BackendInitializeRestoreTime", delta);
  }
}

void ProfileSyncService::OnEngineInitialized(
    ModelTypeSet initial_types,
    const syncer::WeakHandle<syncer::JsBackend>& js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    const std::string& cache_guid,
    bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateEngineInitUMA(success);

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
    OnInternalUnrecoverableError(FROM_HERE, "BackendInitialize failure", false,
                                 ERROR_REASON_ENGINE_INIT_FAILURE);
    return;
  }

  engine_initialized_ = true;

  sync_js_controller_.AttachJsBackend(js_backend);
  debug_info_listener_ = debug_info_listener;

  std::string signin_scoped_device_id;
  if (IsLocalSyncEnabled()) {
    signin_scoped_device_id = "local_device";
  } else {
    SigninClient* signin_client = signin_->GetOriginal()->signin_client();
    DCHECK(signin_client);
    std::string signin_scoped_device_id =
        signin_client->GetSigninScopedDeviceId();
  }

  // Initialize local device info.
  local_device_->Initialize(cache_guid, signin_scoped_device_id);

  if (protocol_event_observers_.might_have_observers()) {
    engine_->RequestBufferedProtocolEventsAndEnableForwarding();
  }

  if (type_debug_info_observers_.might_have_observers()) {
    engine_->EnableDirectoryTypeDebugInfoForwarding();
  }

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  LastSyncedTime will only be null in
  // this case, because the pref wasn't restored on StartUp.
  if (sync_prefs_.GetLastSyncedTime().is_null()) {
    UpdateLastSyncedTime();
  }

  data_type_manager_.reset(
      sync_client_->GetSyncApiComponentFactory()->CreateDataTypeManager(
          initial_types, debug_info_listener_, &data_type_controllers_, this,
          engine_.get(), this));

  crypto_->SetSyncEngine(engine_.get());
  crypto_->SetDataTypeManager(data_type_manager_.get());

  // Auto-start means IsFirstSetupComplete gets set automatically.
  if (start_behavior_ == AUTO_START && !IsFirstSetupComplete()) {
    // This will trigger a configure if it completes setup.
    SetFirstSetupComplete();
  } else if (CanConfigureDataTypes()) {
    ConfigureDataTypeManager();
  }

  // Check for a cookie jar mismatch.
  std::vector<gaia::ListedAccount> accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  if (gaia_cookie_manager_service_ &&
      gaia_cookie_manager_service_->ListAccounts(
          &accounts, &signed_out_accounts, "ChromiumProfileSyncService")) {
    OnGaiaAccountsInCookieUpdated(accounts, signed_out_accounts, error);
  }

  NotifyObservers();

  // Nobody will call us to start if no sign in is going to happen.
  if (IsLocalSyncEnabled())
    RequestStart();
}

void ProfileSyncService::OnSyncCycleCompleted(
    const syncer::SyncCycleSnapshot& snapshot) {
  DCHECK(thread_checker_.CalledOnValidThread());

  last_snapshot_ = snapshot;

  UpdateLastSyncedTime();
  if (!snapshot.poll_finish_time().is_null())
    sync_prefs_.SetLastPollTime(snapshot.poll_finish_time());

  if (IsDataTypeControllerRunning(syncer::SESSIONS) &&
      snapshot.model_neutral_state().get_updates_request_types.Has(
          syncer::SESSIONS) &&
      !syncer::HasSyncerError(snapshot.model_neutral_state())) {
    // Trigger garbage collection of old sessions now that we've downloaded
    // any new session data.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&SessionsSyncManager::DoGarbageCollection,
                              base::AsWeakPtr(sessions_sync_manager_.get())));
  }
  DVLOG(2) << "Notifying observers sync cycle completed";
  NotifySyncCycleCompleted();
}

void ProfileSyncService::OnExperimentsChanged(
    const syncer::Experiments& experiments) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (current_experiments_.Matches(experiments))
    return;

  current_experiments_ = experiments;

  sync_client_->GetPrefService()->SetBoolean(
      invalidation::prefs::kInvalidationServiceUseGCMChannel,
      experiments.gcm_invalidations_enabled);
}

void ProfileSyncService::UpdateAuthErrorState(const AuthError& error) {
  is_auth_in_progress_ = false;
  last_auth_error_ = error;

  NotifyObservers();
}

namespace {

AuthError ConnectionStatusToAuthError(syncer::ConnectionStatus status) {
  switch (status) {
    case syncer::CONNECTION_OK:
      return AuthError::AuthErrorNone();
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
  DCHECK(thread_checker_.CalledOnValidThread());
  connection_status_update_time_ = base::Time::Now();
  connection_status_ = status;
  if (status == syncer::CONNECTION_AUTH_ERROR) {
    // Sync server returned error indicating that access token is invalid. It
    // could be either expired or access is revoked. Let's request another
    // access token and if access is revoked then request for token will fail
    // with corresponding error. If access token is repeatedly reported
    // invalid, there may be some issues with server, e.g. authentication
    // state is inconsistent on sync and token server. In that case, we
    // backoff token requests exponentially to avoid hammering token server
    // too much and to avoid getting same token due to token server's caching
    // policy. |request_access_token_retry_timer_| is used to backoff request
    // triggered by both auth error and failure talking to GAIA server.
    // Therefore, we're likely to reach the backoff ceiling more quickly than
    // you would expect from looking at the BackoffPolicy if both types of
    // errors happen. We shouldn't receive two errors back-to-back without
    // attempting a token/sync request in between, thus crank up request delay
    // unnecessary. This is because we won't make a sync request if we hit an
    // error until GAIA succeeds at sending a new token, and we won't request
    // a new token unless sync reports a token failure. But to be safe, don't
    // schedule request if this happens.
    if (request_access_token_retry_timer_.IsRunning()) {
      // The timer to perform a request later is already running; nothing
      // further needs to be done at this point.
    } else if (request_access_token_backoff_.failure_count() == 0) {
      // First time request without delay. Currently invalid token is used
      // to initialize sync engine and we'll always end up here. We don't
      // want to delay initialization.
      request_access_token_backoff_.InformOfRequest(false);
      RequestAccessToken();
    } else {
      request_access_token_backoff_.InformOfRequest(false);
      request_access_token_retry_timer_.Start(
          FROM_HERE, request_access_token_backoff_.GetTimeUntilRelease(),
          base::Bind(&ProfileSyncService::RequestAccessToken,
                     sync_enabled_weak_factory_.GetWeakPtr()));
    }
  } else {
    // Reset backoff time after successful connection.
    if (status == syncer::CONNECTION_OK) {
      // Request shouldn't be scheduled at this time. But if it is, it's
      // possible that sync flips between OK and auth error states rapidly,
      // thus hammers token server. To be safe, only reset backoff delay when
      // no scheduled request.
      if (!request_access_token_retry_timer_.IsRunning()) {
        request_access_token_backoff_.Reset();
      }
    }

    const GoogleServiceAuthError auth_error =
        ConnectionStatusToAuthError(status);
    DVLOG(1) << "Connection status change: " << auth_error.ToString();
    UpdateAuthErrorState(auth_error);
  }
}

void ProfileSyncService::OnMigrationNeededForTypes(syncer::ModelTypeSet types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(engine_initialized_);
  DCHECK(data_type_manager_);

  // Migrator must be valid, because we don't sync until it is created and this
  // callback originates from a sync cycle.
  migrator_->MigrateTypes(types);
}

void ProfileSyncService::OnActionableError(const SyncProtocolError& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  last_actionable_error_ = error;
  DCHECK_NE(last_actionable_error_.action, syncer::UNKNOWN_ACTION);
  switch (error.action) {
    case syncer::UPGRADE_CLIENT:
    case syncer::CLEAR_USER_DATA_AND_RESYNC:
    case syncer::ENABLE_SYNC_ON_ACCOUNT:
    case syncer::STOP_AND_RESTART_SYNC:
      // TODO(lipalani) : if setup in progress we want to display these
      // actions in the popup. The current experience might not be optimal for
      // the user. We just dismiss the dialog.
      if (startup_controller_->IsSetupInProgress()) {
        RequestStop(CLEAR_DATA);
        expect_sync_configuration_aborted_ = true;
      }
      // Trigger an unrecoverable error to stop syncing.
      OnInternalUnrecoverableError(FROM_HERE,
                                   last_actionable_error_.error_description,
                                   true, ERROR_REASON_ACTIONABLE_ERROR);
      break;
    case syncer::DISABLE_SYNC_ON_CLIENT:
      if (error.error_type == syncer::NOT_MY_BIRTHDAY) {
        UMA_HISTOGRAM_ENUMERATION("Sync.StopSource", syncer::BIRTHDAY_ERROR,
                                  syncer::STOP_SOURCE_LIMIT);
      }
      RequestStop(CLEAR_DATA);
#if !defined(OS_CHROMEOS)
      // On every platform except ChromeOS, sign out the user after a dashboard
      // clear.
      if (!IsLocalSyncEnabled()) {
        static_cast<SigninManager*>(signin_->GetOriginal())
            ->SignOut(signin_metrics::SERVER_FORCED_DISABLE,
                      signin_metrics::SignoutDelete::IGNORE_METRIC);
      }
#endif
      break;
    case syncer::STOP_SYNC_FOR_DISABLED_ACCOUNT:
      // Sync disabled by domain admin. we should stop syncing until next
      // restart.
      sync_disabled_by_admin_ = true;
      ShutdownImpl(syncer::DISABLE_SYNC);
      break;
    case syncer::RESET_LOCAL_SYNC_DATA:
      ShutdownImpl(syncer::DISABLE_SYNC);
      startup_controller_->TryStart();
      UMA_HISTOGRAM_ENUMERATION(
          "Sync.ClearServerDataEvents",
          syncer::CLEAR_SERVER_DATA_RESET_LOCAL_DATA_RECEIVED,
          syncer::CLEAR_SERVER_DATA_MAX);
      break;
    default:
      NOTREACHED();
  }
  NotifyObservers();
}

void ProfileSyncService::ClearAndRestartSyncForPassphraseEncryption() {
  DCHECK(thread_checker_.CalledOnValidThread());
  engine_->ClearServerData(
      base::Bind(&ProfileSyncService::OnClearServerDataDone,
                 sync_enabled_weak_factory_.GetWeakPtr()));
}

void ProfileSyncService::OnClearServerDataDone() {
  DCHECK(sync_prefs_.GetPassphraseEncryptionTransitionInProgress());
  sync_prefs_.SetPassphraseEncryptionTransitionInProgress(false);

  // Call to ClearServerData generates new keystore key on the server. This
  // makes keystore bootstrap token invalid. Let's clear it from preferences.
  sync_prefs_.SetKeystoreEncryptionBootstrapToken(std::string());

  // Shutdown sync, delete the Directory, then restart, restoring the cached
  // nigori state.
  ShutdownImpl(syncer::DISABLE_SYNC);
  startup_controller_->TryStart();
  UMA_HISTOGRAM_ENUMERATION("Sync.ClearServerDataEvents",
                            syncer::CLEAR_SERVER_DATA_SUCCEEDED,
                            syncer::CLEAR_SERVER_DATA_MAX);
}

void ProfileSyncService::ClearServerDataForTest(const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Sync has a restriction that the engine must be in configuration mode
  // in order to run clear server data.
  engine_->StartConfiguration();
  engine_->ClearServerData(callback);
}

void ProfileSyncService::OnConfigureDone(
    const DataTypeManager::ConfigureResult& result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  configure_status_ = result.status;
  data_type_status_table_ = result.data_type_status_table;

  if (!sync_configure_start_time_.is_null()) {
    if (configure_status_ == DataTypeManager::OK) {
      base::Time sync_configure_stop_time = base::Time::Now();
      base::TimeDelta delta =
          sync_configure_stop_time - sync_configure_start_time_;
      if (is_first_time_sync_configure_) {
        UMA_HISTOGRAM_LONG_TIMES("Sync.ServiceInitialConfigureTime", delta);
      } else {
        UMA_HISTOGRAM_LONG_TIMES("Sync.ServiceSubsequentConfigureTime", delta);
      }
    }
    sync_configure_start_time_ = base::Time();
  }

  // Notify listeners that configuration is done.
  for (auto& observer : observers_)
    observer.OnSyncConfigurationCompleted(this);

  DVLOG(1) << "PSS OnConfigureDone called with status: " << configure_status_;
  // The possible status values:
  //    ABORT - Configuration was aborted. This is not an error, if
  //            initiated by user.
  //    OK - Some or all types succeeded.
  //    Everything else is an UnrecoverableError. So treat it as such.

  // First handle the abort case.
  if (configure_status_ == DataTypeManager::ABORTED &&
      expect_sync_configuration_aborted_) {
    DVLOG(0) << "ProfileSyncService::Observe Sync Configure aborted";
    expect_sync_configuration_aborted_ = false;
    return;
  }

  // Handle unrecoverable error.
  if (configure_status_ != DataTypeManager::OK) {
    if (result.was_catch_up_configure) {
      // Record catchup configuration failure.
      UMA_HISTOGRAM_ENUMERATION("Sync.ClearServerDataEvents",
                                syncer::CLEAR_SERVER_DATA_CATCHUP_FAILED,
                                syncer::CLEAR_SERVER_DATA_MAX);
    }
    // Something catastrophic had happened. We should only have one
    // error representing it.
    syncer::SyncError error = data_type_status_table_.GetUnrecoverableError();
    DCHECK(error.IsSet());
    std::string message =
        "Sync configuration failed with status " +
        DataTypeManager::ConfigureStatusToString(configure_status_) +
        " caused by " +
        syncer::ModelTypeSetToString(
            data_type_status_table_.GetUnrecoverableErrorTypes()) +
        ": " + error.message();
    LOG(ERROR) << "ProfileSyncService error: " << message;
    OnInternalUnrecoverableError(error.location(), message, true,
                                 ERROR_REASON_CONFIGURATION_FAILURE);
    return;
  }

  DCHECK_EQ(DataTypeManager::OK, configure_status_);

  // We should never get in a state where we have no encrypted datatypes
  // enabled, and yet we still think we require a passphrase for decryption.
  DCHECK(
      !(IsPassphraseRequiredForDecryption() && !IsEncryptedDatatypeEnabled()));

  // This must be done before we start syncing with the server to avoid
  // sending unencrypted data up on a first time sync.
  if (crypto_->encryption_pending())
    engine_->EnableEncryptEverything();
  NotifyObservers();

  if (migrator_.get() && migrator_->state() != BackendMigrator::IDLE) {
    // Migration in progress.  Let the migrator know we just finished
    // configuring something.  It will be up to the migrator to call
    // StartSyncingWithServer() if migration is now finished.
    migrator_->OnConfigureDone(result);
    return;
  }

  if (result.was_catch_up_configure) {
    ClearAndRestartSyncForPassphraseEncryption();
    return;
  }

  RecordMemoryUsageHistograms();

  StartSyncingWithServer();
}

void ProfileSyncService::OnConfigureStart() {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_configure_start_time_ = base::Time::Now();
  engine_->StartConfiguration();
  NotifyObservers();
}

ProfileSyncService::SyncStatusSummary
ProfileSyncService::QuerySyncStatusSummary() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (HasUnrecoverableError()) {
    return UNRECOVERABLE_ERROR;
  } else if (!engine_) {
    return NOT_ENABLED;
  } else if (engine_ && !IsFirstSetupComplete()) {
    return SETUP_INCOMPLETE;
  } else if (engine_ && IsFirstSetupComplete() && data_type_manager_ &&
             data_type_manager_->state() == DataTypeManager::STOPPED) {
    return DATATYPES_NOT_INITIALIZED;
  } else if (IsSyncActive()) {
    return INITIALIZED;
  }
  return UNKNOWN_ERROR;
}

std::string ProfileSyncService::QuerySyncStatusSummaryString() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SyncStatusSummary status = QuerySyncStatusSummary();

  std::string config_status_str =
      configure_status_ != DataTypeManager::UNKNOWN
          ? DataTypeManager::ConfigureStatusToString(configure_status_)
          : "";

  switch (status) {
    case UNRECOVERABLE_ERROR:
      return "Unrecoverable error detected";
    case NOT_ENABLED:
      return "Syncing not enabled";
    case SETUP_INCOMPLETE:
      return "First time sync setup incomplete";
    case DATATYPES_NOT_INITIALIZED:
      return "Datatypes not fully initialized";
    case INITIALIZED:
      return "Sync service initialized";
    default:
      return "Status unknown: Internal error?";
  }
}

std::string ProfileSyncService::GetEngineInitializationStateString() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return startup_controller_->GetEngineInitializationStateString();
}

bool ProfileSyncService::IsSetupInProgress() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return startup_controller_->IsSetupInProgress();
}

bool ProfileSyncService::QueryDetailedSyncStatus(SyncEngine::Status* result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (engine_ && engine_initialized_) {
    *result = engine_->GetDetailedStatus();
    return true;
  } else {
    SyncEngine::Status status;
    status.sync_protocol_error = last_actionable_error_;
    *result = status;
    return false;
  }
}

const AuthError& ProfileSyncService::GetAuthError() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return last_auth_error_;
}

bool ProfileSyncService::CanConfigureDataTypes() const {
  return IsFirstSetupComplete() && !IsSetupInProgress();
}

bool ProfileSyncService::IsFirstSetupInProgress() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return !IsFirstSetupComplete() && startup_controller_->IsSetupInProgress();
}

std::unique_ptr<syncer::SyncSetupInProgressHandle>
ProfileSyncService::GetSetupInProgressHandle() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (++outstanding_setup_in_progress_handles_ == 1) {
    DCHECK(!startup_controller_->IsSetupInProgress());
    startup_controller_->SetSetupInProgress(true);

    NotifyObservers();
  }

  return std::unique_ptr<syncer::SyncSetupInProgressHandle>(
      new syncer::SyncSetupInProgressHandle(
          base::Bind(&ProfileSyncService::OnSetupInProgressHandleDestroyed,
                     weak_factory_.GetWeakPtr())));
}

bool ProfileSyncService::IsSyncAllowed() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return IsSyncAllowedByFlag() && !IsManaged() && IsSyncAllowedByPlatform();
}

bool ProfileSyncService::IsSyncActive() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return engine_initialized_ && data_type_manager_ &&
         data_type_manager_->state() != DataTypeManager::STOPPED;
}

bool ProfileSyncService::IsLocalSyncEnabled() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return sync_prefs_.IsLocalSyncEnabled();
}

void ProfileSyncService::TriggerRefresh(const syncer::ModelTypeSet& types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (engine_initialized_)
    engine_->TriggerRefresh(types);
}

bool ProfileSyncService::IsSignedIn() const {
  // Sync is logged in if there is a non-empty effective account id.
  return !signin_->GetAccountIdToUse().empty();
}

bool ProfileSyncService::CanEngineStart() const {
  if (IsLocalSyncEnabled())
    return true;
  return CanSyncStart() && oauth2_token_service_ &&
         oauth2_token_service_->RefreshTokenIsAvailable(
             signin_->GetAccountIdToUse());
}

bool ProfileSyncService::IsEngineInitialized() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return engine_initialized_;
}

bool ProfileSyncService::ConfigurationDone() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return data_type_manager_ &&
         data_type_manager_->state() == DataTypeManager::CONFIGURED;
}

bool ProfileSyncService::waiting_for_auth() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return is_auth_in_progress_;
}

const syncer::Experiments& ProfileSyncService::current_experiments() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return current_experiments_;
}

bool ProfileSyncService::HasUnrecoverableError() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return unrecoverable_error_reason_ != ERROR_REASON_UNSET;
}

bool ProfileSyncService::IsPassphraseRequired() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return crypto_->passphrase_required_reason() !=
         syncer::REASON_PASSPHRASE_NOT_REQUIRED;
}

bool ProfileSyncService::IsPassphraseRequiredForDecryption() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If there is an encrypted datatype enabled and we don't have the proper
  // passphrase, we must prompt the user for a passphrase. The only way for the
  // user to avoid entering their passphrase is to disable the encrypted types.
  return IsEncryptedDatatypeEnabled() && IsPassphraseRequired();
}

base::Time ProfileSyncService::GetLastSyncedTime() const {
  return sync_prefs_.GetLastSyncedTime();
}

void ProfileSyncService::UpdateSelectedTypesHistogram(
    bool sync_everything,
    const syncer::ModelTypeSet chosen_types) const {
  if (!IsFirstSetupComplete() ||
      sync_everything != sync_prefs_.HasKeepEverythingSynced()) {
    UMA_HISTOGRAM_BOOLEAN("Sync.SyncEverything", sync_everything);
  }

  // Only log the data types that are shown in the sync settings ui.
  // Note: the order of these types must match the ordering of
  // the respective types in ModelType
  const syncer::user_selectable_type::UserSelectableSyncType
      user_selectable_types[] = {
        syncer::user_selectable_type::BOOKMARKS,
        syncer::user_selectable_type::PREFERENCES,
        syncer::user_selectable_type::PASSWORDS,
        syncer::user_selectable_type::AUTOFILL,
        syncer::user_selectable_type::THEMES,
        syncer::user_selectable_type::TYPED_URLS,
        syncer::user_selectable_type::EXTENSIONS,
        syncer::user_selectable_type::APPS,
#if BUILDFLAG(ENABLE_READING_LIST)
        syncer::user_selectable_type::READING_LIST,
#endif
        syncer::user_selectable_type::PROXY_TABS,
      };

  static_assert(40 == syncer::MODEL_TYPE_COUNT,
                "If adding a user selectable type, update "
                "UserSelectableSyncType in user_selectable_sync_type.h and "
                "histograms.xml.");

  if (!sync_everything) {
    const syncer::ModelTypeSet current_types = GetPreferredDataTypes();

    syncer::ModelTypeSet type_set = syncer::UserSelectableTypes();
    syncer::ModelTypeSet::Iterator it = type_set.First();

    DCHECK_EQ(arraysize(user_selectable_types), type_set.Size());

    for (size_t i = 0; i < arraysize(user_selectable_types) && it.Good();
         ++i, it.Inc()) {
      const syncer::ModelType type = it.Get();
      if (chosen_types.Has(type) &&
          (!IsFirstSetupComplete() || !current_types.Has(type))) {
        // Selected type has changed - log it.
        UMA_HISTOGRAM_ENUMERATION(
            "Sync.CustomSync", user_selectable_types[i],
            syncer::user_selectable_type::SELECTABLE_DATATYPE_COUNT + 1);
      }
    }
  }
}

void ProfileSyncService::OnUserChoseDatatypes(
    bool sync_everything,
    syncer::ModelTypeSet chosen_types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(syncer::UserSelectableTypes().HasAll(chosen_types));

  if (!engine_ && !HasUnrecoverableError()) {
    NOTREACHED();
    return;
  }

  UpdateSelectedTypesHistogram(sync_everything, chosen_types);
  sync_prefs_.SetKeepEverythingSynced(sync_everything);

  if (data_type_manager_)
    data_type_manager_->ResetDataTypeErrors();
  ChangePreferredDataTypes(chosen_types);
}

void ProfileSyncService::ChangePreferredDataTypes(
    syncer::ModelTypeSet preferred_types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "ChangePreferredDataTypes invoked";
  const syncer::ModelTypeSet registered_types = GetRegisteredDataTypes();
  // Will only enable those types that are registered and preferred.
  sync_prefs_.SetPreferredDataTypes(registered_types, preferred_types);

  // Now reconfigure the DTM.
  ReconfigureDatatypeManager();
}

syncer::ModelTypeSet ProfileSyncService::GetActiveDataTypes() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!data_type_manager_)
    return syncer::ModelTypeSet();
  return data_type_manager_->GetActiveDataTypes();
}

syncer::SyncClient* ProfileSyncService::GetSyncClient() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return sync_client_.get();
}

syncer::ModelTypeSet ProfileSyncService::GetPreferredDataTypes() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  const syncer::ModelTypeSet registered_types = GetRegisteredDataTypes();
  const syncer::ModelTypeSet preferred_types =
      Union(sync_prefs_.GetPreferredDataTypes(registered_types),
            syncer::ControlTypes());
  const syncer::ModelTypeSet enforced_types =
      Intersection(GetDataTypesFromPreferenceProviders(), registered_types);
  return Union(preferred_types, enforced_types);
}

syncer::ModelTypeSet ProfileSyncService::GetForcedDataTypes() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(treib,zea): When SyncPrefs also implements SyncTypePreferenceProvider,
  // we'll need another way to distinguish user-choosable types from
  // programmatically-enabled types.
  return GetDataTypesFromPreferenceProviders();
}

syncer::ModelTypeSet ProfileSyncService::GetRegisteredDataTypes() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  syncer::ModelTypeSet registered_types;
  // The data_type_controllers_ are determined by command-line flags;
  // that's effectively what controls the values returned here.
  for (DataTypeController::TypeMap::const_iterator it =
           data_type_controllers_.begin();
       it != data_type_controllers_.end(); ++it) {
    registered_types.Put(it->first);
  }
  return registered_types;
}

bool ProfileSyncService::IsUsingSecondaryPassphrase() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return crypto_->IsUsingSecondaryPassphrase();
}

std::string ProfileSyncService::GetCustomPassphraseKey() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  syncer::SystemEncryptor encryptor;
  syncer::Cryptographer cryptographer(&encryptor);
  cryptographer.Bootstrap(sync_prefs_.GetEncryptionBootstrapToken());
  return cryptographer.GetDefaultNigoriKeyData();
}

syncer::PassphraseType ProfileSyncService::GetPassphraseType() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return crypto_->GetPassphraseType();
}

base::Time ProfileSyncService::GetExplicitPassphraseTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return crypto_->GetExplicitPassphraseTime();
}

bool ProfileSyncService::IsCryptographerReady(
    const syncer::BaseTransaction* trans) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return engine_ && engine_->IsCryptographerReady(trans);
}

void ProfileSyncService::SetPlatformSyncAllowedProvider(
    const PlatformSyncAllowedProvider& platform_sync_allowed_provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  platform_sync_allowed_provider_ = platform_sync_allowed_provider;
}

// static
syncer::ModelTypeStoreFactory ProfileSyncService::GetModelTypeStoreFactory(
    ModelType type,
    const base::FilePath& base_path) {
  // TODO(skym): Verify using AsUTF8Unsafe is okay here. Should work as long
  // as the Local State file is guaranteed to be UTF-8.
  const std::string path =
      FormatSharedModelTypeStorePath(base_path).AsUTF8Unsafe();
  return base::Bind(&ModelTypeStore::CreateStore, type, path);
}

void ProfileSyncService::ConfigureDataTypeManager() {
  // Don't configure datatypes if the setup UI is still on the screen - this
  // is to help multi-screen setting UIs (like iOS) where they don't want to
  // start syncing data until the user is done configuring encryption options,
  // etc. ReconfigureDatatypeManager() will get called again once the UI calls
  // SetSetupInProgress(false).
  if (!CanConfigureDataTypes()) {
    // If we can't configure the data type manager yet, we should still notify
    // observers. This is to support multiple setup UIs being open at once.
    NotifyObservers();
    return;
  }

  bool restart = false;
  if (!migrator_) {
    restart = true;

    // We create the migrator at the same time.
    migrator_ = std::make_unique<BackendMigrator>(
        debug_identifier_, GetUserShare(), this, data_type_manager_.get(),
        base::Bind(&ProfileSyncService::StartSyncingWithServer,
                   base::Unretained(this)));
  }

  syncer::ModelTypeSet types;
  syncer::ConfigureReason reason = syncer::CONFIGURE_REASON_UNKNOWN;
  types = GetPreferredDataTypes();
  if (restart) {
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
  DCHECK(thread_checker_.CalledOnValidThread());
  if (engine_ && engine_initialized_) {
    return engine_->GetUserShare();
  }
  NOTREACHED();
  return nullptr;
}

syncer::SyncCycleSnapshot ProfileSyncService::GetLastCycleSnapshot() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return last_snapshot_;
}

bool ProfileSyncService::HasUnsyncedItems() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (HasSyncingEngine() && engine_initialized_) {
    return engine_->HasUnsyncedItems();
  }
  NOTREACHED();
  return false;
}

BackendMigrator* ProfileSyncService::GetBackendMigratorForTest() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return migrator_.get();
}

void ProfileSyncService::GetModelSafeRoutingInfo(
    syncer::ModelSafeRoutingInfo* out) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (engine_ && engine_initialized_) {
    engine_->GetModelSafeRoutingInfo(out);
  } else {
    NOTREACHED();
  }
}

std::unique_ptr<base::Value> ProfileSyncService::GetTypeStatusMap() {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto result = std::make_unique<base::ListValue>();

  if (!engine_ || !engine_initialized_) {
    return std::move(result);
  }

  SyncEngine::Status detailed_status = engine_->GetDetailedStatus();
  const ModelTypeSet& throttled_types(detailed_status.throttled_types);
  const ModelTypeSet& backed_off_types(detailed_status.backed_off_types);

  std::unique_ptr<base::DictionaryValue> type_status_header(
      new base::DictionaryValue());
  type_status_header->SetString("status", "header");
  type_status_header->SetString("name", "Model Type");
  type_status_header->SetString("num_entries", "Total Entries");
  type_status_header->SetString("num_live", "Live Entries");
  type_status_header->SetString("message", "Message");
  type_status_header->SetString("state", "State");
  type_status_header->SetString("group_type", "Group Type");
  result->Append(std::move(type_status_header));

  const DataTypeStatusTable::TypeErrorMap error_map =
      data_type_status_table_.GetAllErrors();
  ModelSafeRoutingInfo routing_info;
  engine_->GetModelSafeRoutingInfo(&routing_info);
  const ModelTypeSet registered = GetRegisteredDataTypes();
  for (ModelTypeSet::Iterator it = registered.First(); it.Good(); it.Inc()) {
    ModelType type = it.Get();

    auto type_status = std::make_unique<base::DictionaryValue>();
    type_status->SetString("name", ModelTypeToString(type));
    type_status->SetString("group_type",
                           ModelSafeGroupToString(routing_info[type]));

    if (error_map.find(type) != error_map.end()) {
      const syncer::SyncError& error = error_map.find(type)->second;
      DCHECK(error.IsSet());
      switch (error.GetSeverity()) {
        case syncer::SyncError::SYNC_ERROR_SEVERITY_ERROR:
          type_status->SetString("status", "error");
          type_status->SetString(
              "message", "Error: " + error.location().ToString() + ", " +
                             error.GetMessagePrefix() + error.message());
          break;
        case syncer::SyncError::SYNC_ERROR_SEVERITY_INFO:
          type_status->SetString("status", "disabled");
          type_status->SetString("message", error.message());
          break;
      }
    } else if (throttled_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("message", " Throttled");
    } else if (backed_off_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("message", "Backed off");
    } else if (routing_info.find(type) != routing_info.end()) {
      type_status->SetString("status", "ok");
      type_status->SetString("message", "");
    } else {
      type_status->SetString("status", "warning");
      type_status->SetString("message", "Disabled by User");
    }

    const auto& dtc_iter = data_type_controllers_.find(type);
    if (dtc_iter != data_type_controllers_.end()) {
      // OnDatatypeStatusCounterUpdated that posts back to the UI thread so that
      // real results can't get overwritten by the empty counters set at the end
      // of this method.
      dtc_iter->second->GetStatusCounters(BindToCurrentThread(
          base::Bind(&ProfileSyncService::OnDatatypeStatusCounterUpdated,
                     base::Unretained(this))));
      type_status->SetString("state", DataTypeController::StateToString(
                                          dtc_iter->second->state()));
    }

    result->Append(std::move(type_status));
  }
  return std::move(result);
}

void ProfileSyncService::RequestAccessToken() {
  // Only one active request at a time.
  if (access_token_request_ != nullptr)
    return;
  request_access_token_retry_timer_.Stop();
  OAuth2TokenService::ScopeSet oauth2_scopes;
  oauth2_scopes.insert(signin_->GetSyncScopeToUse());

  // Invalidate previous token, otherwise token service will return the same
  // token again.
  const std::string& account_id = signin_->GetAccountIdToUse();
  if (!access_token_.empty()) {
    oauth2_token_service_->InvalidateAccessToken(account_id, oauth2_scopes,
                                                 access_token_);
  }

  access_token_.clear();

  token_request_time_ = base::Time::Now();
  token_receive_time_ = base::Time();
  next_token_request_time_ = base::Time();
  access_token_request_ =
      oauth2_token_service_->StartRequest(account_id, oauth2_scopes, this);
}

void ProfileSyncService::SetEncryptionPassphrase(const std::string& passphrase,
                                                 PassphraseType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  crypto_->SetEncryptionPassphrase(passphrase, type == EXPLICIT);
}

bool ProfileSyncService::SetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (IsPassphraseRequired()) {
    DVLOG(1) << "Setting passphrase for decryption.";
    bool result = crypto_->SetDecryptionPassphrase(passphrase);
    UMA_HISTOGRAM_BOOLEAN("Sync.PassphraseDecryptionSucceeded", result);
    return result;
  } else {
    NOTREACHED() << "SetDecryptionPassphrase must not be called when "
                    "IsPassphraseRequired() is false.";
    return false;
  }
}

bool ProfileSyncService::IsEncryptEverythingAllowed() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return crypto_->IsEncryptEverythingAllowed();
}

void ProfileSyncService::SetEncryptEverythingAllowed(bool allowed) {
  DCHECK(thread_checker_.CalledOnValidThread());
  crypto_->SetEncryptEverythingAllowed(allowed);
}

void ProfileSyncService::EnableEncryptEverything() {
  DCHECK(thread_checker_.CalledOnValidThread());
  crypto_->EnableEncryptEverything();
}

bool ProfileSyncService::encryption_pending() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // We may be called during the setup process before we're
  // initialized (via IsEncryptedDatatypeEnabled and
  // IsPassphraseRequiredForDecryption).
  return crypto_->encryption_pending();
}

bool ProfileSyncService::IsEncryptEverythingEnabled() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return crypto_->IsEncryptEverythingEnabled();
}

syncer::ModelTypeSet ProfileSyncService::GetEncryptedDataTypes() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return crypto_->GetEncryptedDataTypes();
}

void ProfileSyncService::OnSyncManagedPrefChange(bool is_sync_managed) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_sync_managed) {
    StopImpl(CLEAR_DATA);
  } else {
    // Sync is no longer disabled by policy. Try starting it up if appropriate.
    startup_controller_->TryStart();
  }
}

void ProfileSyncService::GoogleSigninSucceeded(const std::string& account_id,
                                               const std::string& username) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsEngineInitialized() || GetAuthError().state() != AuthError::NONE) {
    // Track the fact that we're still waiting for auth to complete.
    is_auth_in_progress_ = true;
  }

  if (signin::IsAccountConsistencyDiceEnabled() &&
      oauth2_token_service_->RefreshTokenIsAvailable(account_id)) {
    // When Dice is enabled, the refresh token may be available before the user
    // enables sync. Start sync if the refresh token is already available in the
    // token service when the authenticated account is set.
    OnRefreshTokenAvailable(account_id);
  }
}

void ProfileSyncService::GoogleSignedOut(const std::string& account_id,
                                         const std::string& username) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_disabled_by_admin_ = false;
  UMA_HISTOGRAM_ENUMERATION("Sync.StopSource", syncer::SIGN_OUT,
                            syncer::STOP_SOURCE_LIMIT);
  RequestStop(CLEAR_DATA);
}

void ProfileSyncService::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& accounts,
    const std::vector<gaia::ListedAccount>& signed_out_accounts,
    const GoogleServiceAuthError& error) {
  OnGaiaAccountsInCookieUpdatedWithCallback(accounts, signed_out_accounts,
                                            error, base::Closure());
}

void ProfileSyncService::OnGaiaAccountsInCookieUpdatedWithCallback(
    const std::vector<gaia::ListedAccount>& accounts,
    const std::vector<gaia::ListedAccount>& signed_out_accounts,
    const GoogleServiceAuthError& error,
    const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsEngineInitialized())
    return;

  bool cookie_jar_mismatch = true;
  bool cookie_jar_empty = accounts.size() == 0;
  std::string account_id = signin_->GetAccountIdToUse();

  // Iterate through list of accounts, looking for current sync account.
  for (const auto& account : accounts) {
    if (account.id == account_id) {
      cookie_jar_mismatch = false;
      break;
    }
  }

  DVLOG(1) << "Cookie jar mismatch: " << cookie_jar_mismatch;
  DVLOG(1) << "Cookie jar empty: " << cookie_jar_empty;
  engine_->OnCookieJarChanged(cookie_jar_mismatch, cookie_jar_empty, callback);
}

void ProfileSyncService::AddProtocolEventObserver(
    ProtocolEventObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  protocol_event_observers_.AddObserver(observer);
  if (HasSyncingEngine()) {
    engine_->RequestBufferedProtocolEventsAndEnableForwarding();
  }
}

void ProfileSyncService::RemoveProtocolEventObserver(
    ProtocolEventObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  protocol_event_observers_.RemoveObserver(observer);
  if (HasSyncingEngine() && !protocol_event_observers_.might_have_observers()) {
    engine_->DisableProtocolEventForwarding();
  }
}

void ProfileSyncService::AddTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* type_debug_info_observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  type_debug_info_observers_.AddObserver(type_debug_info_observer);
  if (type_debug_info_observers_.might_have_observers() &&
      engine_initialized_) {
    engine_->EnableDirectoryTypeDebugInfoForwarding();
  }
}

void ProfileSyncService::RemoveTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* type_debug_info_observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  type_debug_info_observers_.RemoveObserver(type_debug_info_observer);
  if (!type_debug_info_observers_.might_have_observers() &&
      engine_initialized_) {
    engine_->DisableDirectoryTypeDebugInfoForwarding();
  }
}

void ProfileSyncService::AddPreferenceProvider(
    syncer::SyncTypePreferenceProvider* provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!HasPreferenceProvider(provider))
      << "Providers may only be added once!";
  preference_providers_.insert(provider);
}

void ProfileSyncService::RemovePreferenceProvider(
    syncer::SyncTypePreferenceProvider* provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(HasPreferenceProvider(provider))
      << "Only providers that have been added before can be removed!";
  preference_providers_.erase(provider);
}

bool ProfileSyncService::HasPreferenceProvider(
    syncer::SyncTypePreferenceProvider* provider) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return preference_providers_.count(provider) > 0;
}

namespace {

class GetAllNodesRequestHelper
    : public base::RefCountedThreadSafe<GetAllNodesRequestHelper> {
 public:
  GetAllNodesRequestHelper(
      syncer::ModelTypeSet requested_types,
      const base::Callback<void(std::unique_ptr<base::ListValue>)>& callback);

  void OnReceivedNodesForType(const syncer::ModelType type,
                              std::unique_ptr<base::ListValue> node_list);

 private:
  friend class base::RefCountedThreadSafe<GetAllNodesRequestHelper>;
  virtual ~GetAllNodesRequestHelper();

  std::unique_ptr<base::ListValue> result_accumulator_;
  syncer::ModelTypeSet awaiting_types_;
  base::Callback<void(std::unique_ptr<base::ListValue>)> callback_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(GetAllNodesRequestHelper);
};

GetAllNodesRequestHelper::GetAllNodesRequestHelper(
    syncer::ModelTypeSet requested_types,
    const base::Callback<void(std::unique_ptr<base::ListValue>)>& callback)
    : result_accumulator_(new base::ListValue()),
      awaiting_types_(requested_types),
      callback_(callback) {}

GetAllNodesRequestHelper::~GetAllNodesRequestHelper() {
  if (!awaiting_types_.Empty()) {
    DLOG(WARNING)
        << "GetAllNodesRequest deleted before request was fulfilled.  "
        << "Missing types are: " << ModelTypeSetToString(awaiting_types_);
  }
}

// Called when the set of nodes for a type has been returned.
// Only return one type of nodes each time.
void GetAllNodesRequestHelper::OnReceivedNodesForType(
    const syncer::ModelType type,
    std::unique_ptr<base::ListValue> node_list) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Add these results to our list.
  std::unique_ptr<base::DictionaryValue> type_dict(new base::DictionaryValue());
  type_dict->SetString("type", ModelTypeToString(type));
  type_dict->Set("nodes", std::move(node_list));
  result_accumulator_->Append(std::move(type_dict));

  // Remember that this part of the request is satisfied.
  awaiting_types_.Remove(type);

  if (awaiting_types_.Empty()) {
    callback_.Run(std::move(result_accumulator_));
    callback_.Reset();
  }
}

}  // namespace

void ProfileSyncService::GetAllNodes(
    const base::Callback<void(std::unique_ptr<base::ListValue>)>& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ModelTypeSet all_types = GetActiveDataTypes();
  all_types.PutAll(syncer::ControlTypes());
  scoped_refptr<GetAllNodesRequestHelper> helper =
      new GetAllNodesRequestHelper(all_types, callback);

  if (!engine_initialized_) {
    // If there's no engine available to fulfill the request, handle it here.
    for (ModelTypeSet::Iterator it = all_types.First(); it.Good(); it.Inc()) {
      helper->OnReceivedNodesForType(it.Get(),
                                     std::make_unique<base::ListValue>());
    }
    return;
  }

  for (ModelTypeSet::Iterator it = all_types.First(); it.Good(); it.Inc()) {
    const auto& dtc_iter = data_type_controllers_.find(it.Get());
    if (dtc_iter != data_type_controllers_.end()) {
      dtc_iter->second->GetAllNodes(base::Bind(
          &GetAllNodesRequestHelper::OnReceivedNodesForType, helper));
    } else {
      // Control Types
      helper->OnReceivedNodesForType(
          it.Get(),
          syncer::DirectoryDataTypeController::GetAllNodesForTypeFromDirectory(
              it.Get(), GetUserShare()->directory.get()));
    }
  }
}

syncer::GlobalIdMapper* ProfileSyncService::GetGlobalIdMapper() const {
  return sessions_sync_manager_.get();
}

base::WeakPtr<syncer::JsController> ProfileSyncService::GetJsController() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return sync_js_controller_.AsWeakPtr();
}

// static
void ProfileSyncService::SyncEvent(SyncEventCodes code) {
  UMA_HISTOGRAM_ENUMERATION("Sync.EventCodes", code, MAX_SYNC_EVENT_CODE);
}

// static
bool ProfileSyncService::IsSyncAllowedByFlag() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSync);
}

bool ProfileSyncService::IsSyncAllowedByPlatform() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return platform_sync_allowed_provider_.is_null() ||
         platform_sync_allowed_provider_.Run();
}

bool ProfileSyncService::IsManaged() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return sync_prefs_.IsManaged() || sync_disabled_by_admin_;
}

void ProfileSyncService::RequestStop(SyncStopDataFate data_fate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_prefs_.SetSyncRequested(false);
  StopImpl(data_fate);
}

bool ProfileSyncService::IsSyncRequested() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // When local sync is on sync should be considered requsted or otherwise it
  // will not resume after the policy or the flag has been removed.
  return sync_prefs_.IsSyncRequested() || sync_prefs_.IsLocalSyncEnabled();
}

void ProfileSyncService::RequestStart() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsSyncAllowed()) {
    // Sync cannot be requested if it's not allowed.
    return;
  }
  DCHECK(sync_client_);
  if (!IsSyncRequested()) {
    sync_prefs_.SetSyncRequested(true);
    NotifyObservers();
  }
  startup_controller_->TryStartImmediately();
}

void ProfileSyncService::ReconfigureDatatypeManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If we haven't initialized yet, don't configure the DTM as it could cause
  // association to start before a Directory has even been created.
  if (engine_initialized_) {
    DCHECK(engine_);
    ConfigureDataTypeManager();
  } else if (HasUnrecoverableError()) {
    // There is nothing more to configure. So inform the listeners,
    NotifyObservers();

    DVLOG(1) << "ConfigureDataTypeManager not invoked because of an "
             << "Unrecoverable error.";
  } else {
    DVLOG(0) << "ConfigureDataTypeManager not invoked because engine is not "
             << "initialized";
  }
}

syncer::ModelTypeSet ProfileSyncService::GetDataTypesFromPreferenceProviders()
    const {
  syncer::ModelTypeSet types;
  for (std::set<syncer::SyncTypePreferenceProvider*>::const_iterator it =
           preference_providers_.begin();
       it != preference_providers_.end(); ++it) {
    types.PutAll((*it)->GetPreferredDataTypes());
  }
  return types;
}

const DataTypeStatusTable& ProfileSyncService::data_type_status_table() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return data_type_status_table_;
}

void ProfileSyncService::OnInternalUnrecoverableError(
    const base::Location& from_here,
    const std::string& message,
    bool delete_sync_database,
    UnrecoverableErrorReason reason) {
  DCHECK(!HasUnrecoverableError());
  unrecoverable_error_reason_ = reason;
  OnUnrecoverableErrorImpl(from_here, message, delete_sync_database);
}

bool ProfileSyncService::IsRetryingAccessTokenFetchForTest() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return request_access_token_retry_timer_.IsRunning();
}

std::string ProfileSyncService::GetAccessTokenForTest() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return access_token_;
}

syncer::SyncableService* ProfileSyncService::GetSessionsSyncableService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return sessions_sync_manager_.get();
}

syncer::ModelTypeSyncBridge* ProfileSyncService::GetDeviceInfoSyncBridge() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return device_info_sync_bridge_.get();
}

syncer::SyncService::SyncTokenStatus ProfileSyncService::GetSyncTokenStatus()
    const {
  DCHECK(thread_checker_.CalledOnValidThread());
  SyncTokenStatus status;
  status.connection_status_update_time = connection_status_update_time_;
  status.connection_status = connection_status_;
  status.token_request_time = token_request_time_;
  status.token_receive_time = token_receive_time_;
  status.last_get_token_error = last_get_token_error_;
  if (request_access_token_retry_timer_.IsRunning())
    status.next_token_request_time = next_token_request_time_;
  return status;
}

void ProfileSyncService::OverrideNetworkResourcesForTest(
    std::unique_ptr<syncer::NetworkResources> network_resources) {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_resources_ = std::move(network_resources);
}

bool ProfileSyncService::HasSyncingEngine() const {
  return engine_ != nullptr;
}

void ProfileSyncService::UpdateFirstSyncTimePref() {
  if (!IsLocalSyncEnabled() && !IsSignedIn()) {
    sync_prefs_.ClearFirstSyncTime();
  } else if (sync_prefs_.GetFirstSyncTime().is_null()) {
    // Set if not set before and it's syncing now.
    sync_prefs_.SetFirstSyncTime(base::Time::Now());
  }
}

void ProfileSyncService::FlushDirectory() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // engine_initialized_ implies engine_ isn't null and the manager exists.
  // If sync is not initialized yet, we fail silently.
  if (engine_initialized_)
    engine_->FlushDirectory();
}

base::FilePath ProfileSyncService::GetDirectoryPathForTest() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return FormatSyncDataPath(base_directory_);
}

base::MessageLoop* ProfileSyncService::GetSyncLoopForTest() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (sync_thread_) {
    return sync_thread_->message_loop();
  } else {
    return nullptr;
  }
}

syncer::SyncEncryptionHandler::Observer*
ProfileSyncService::GetEncryptionObserverForTest() const {
  return crypto_.get();
}

void ProfileSyncService::RefreshTypesForTest(syncer::ModelTypeSet types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (engine_initialized_)
    engine_->RefreshTypesForTest(types);
}

void ProfileSyncService::RemoveClientFromServer() const {
  if (!engine_initialized_)
    return;
  const std::string cache_guid = local_device_->GetLocalSyncCacheGUID();
  std::string birthday;
  syncer::UserShare* user_share = GetUserShare();
  if (user_share && user_share->directory.get()) {
    birthday = user_share->directory->store_birthday();
  }
  if (!access_token_.empty() && !cache_guid.empty() && !birthday.empty()) {
    sync_stopped_reporter_->ReportSyncStopped(access_token_, cache_guid,
                                              birthday);
  }
}

void ProfileSyncService::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    sync_prefs_.SetMemoryPressureWarningCount(
        sync_prefs_.GetMemoryPressureWarningCount() + 1);
  }
}

void ProfileSyncService::ReportPreviousSessionMemoryWarningCount() {
  int warning_received = sync_prefs_.GetMemoryPressureWarningCount();

  if (-1 != warning_received) {
    // -1 means it is new client.
    if (!sync_prefs_.DidSyncShutdownCleanly()) {
      UMA_HISTOGRAM_COUNTS("Sync.MemoryPressureWarningBeforeUncleanShutdown",
                           warning_received);
    } else {
      UMA_HISTOGRAM_COUNTS("Sync.MemoryPressureWarningBeforeCleanShutdown",
                           warning_received);
    }
  }
  sync_prefs_.SetMemoryPressureWarningCount(0);
  // Will set to true during a clean shutdown, so crash or something else will
  // remain this as false.
  sync_prefs_.SetCleanShutdown(false);
}

void ProfileSyncService::RecordMemoryUsageHistograms() {
  ModelTypeSet active_types = GetActiveDataTypes();
  for (ModelTypeSet::Iterator type_it = active_types.First(); type_it.Good();
       type_it.Inc()) {
    auto dtc_it = data_type_controllers_.find(type_it.Get());
    if (dtc_it != data_type_controllers_.end())
      dtc_it->second->RecordMemoryUsageHistogram();
  }
}

const GURL& ProfileSyncService::sync_service_url() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return sync_service_url_;
}

std::string ProfileSyncService::unrecoverable_error_message() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return unrecoverable_error_message_;
}

base::Location ProfileSyncService::unrecoverable_error_location() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return unrecoverable_error_location_;
}

void ProfileSyncService::OnSetupInProgressHandleDestroyed() {
  DCHECK_GT(outstanding_setup_in_progress_handles_, 0);

  // Don't re-start Sync until all outstanding handles are destroyed.
  if (--outstanding_setup_in_progress_handles_ != 0)
    return;

  DCHECK(startup_controller_->IsSetupInProgress());
  startup_controller_->SetSetupInProgress(false);

  if (IsEngineInitialized())
    ReconfigureDatatypeManager();
  NotifyObservers();
}
}  // namespace browser_sync
