// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service.h"

#include <stddef.h>
#include <map>
#include <set>
#include <utility>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/backend_migrator.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/session_data_type_controller.h"
#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/glue/session_data_type_controller.h"
#include "chrome/browser/sync/internal_api/configure_reason.h"
#include "chrome/browser/sync/internal_api/sync_manager.h"
#include "chrome/browser/sync/js/js_arg_list.h"
#include "chrome/browser/sync/js/js_event_details.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/signin_manager.h"
#include "chrome/browser/sync/sync_global_error.h"
#include "chrome/browser/sync/util/cryptographer.h"
#include "chrome/browser/sync/util/oauth.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "grit/generated_resources.h"
#include "net/base/cookie_monster.h"
#include "ui/base/l10n/l10n_util.h"

using browser_sync::ChangeProcessor;
using browser_sync::DataTypeController;
using browser_sync::DataTypeManager;
using browser_sync::JsBackend;
using browser_sync::JsController;
using browser_sync::JsEventDetails;
using browser_sync::JsEventHandler;
using browser_sync::SyncBackendHost;
using browser_sync::WeakHandle;
using browser_sync::SyncProtocolError;
using sync_api::SyncCredentials;

typedef GoogleServiceAuthError AuthError;

const char* ProfileSyncService::kSyncServerUrl =
    "https://clients4.google.com/chrome-sync";

const char* ProfileSyncService::kDevServerUrl =
    "https://clients4.google.com/chrome-sync/dev";

static const int kSyncClearDataTimeoutInSeconds = 60;  // 1 minute.


bool ShouldShowActionOnUI(
    const browser_sync::SyncProtocolError& error) {
  return (error.action != browser_sync::UNKNOWN_ACTION &&
          error.action != browser_sync::DISABLE_SYNC_ON_CLIENT);
}

ProfileSyncService::ProfileSyncService(ProfileSyncFactory* factory,
                                       Profile* profile,
                                       SigninManager* signin_manager,
                                       const std::string& cros_user)
    : last_auth_error_(AuthError::None()),
      passphrase_required_reason_(sync_api::REASON_PASSPHRASE_NOT_REQUIRED),
      factory_(factory),
      profile_(profile),
      cros_user_(cros_user),
      sync_service_url_(kDevServerUrl),
      backend_initialized_(false),
      is_auth_in_progress_(false),
      wizard_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      signin_(signin_manager),
      unrecoverable_error_detected_(false),
      scoped_runnable_method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      expect_sync_configuration_aborted_(false),
      clear_server_data_state_(CLEAR_NOT_STARTED),
      encryption_pending_(false),
      auto_start_enabled_(false) {
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

  // TODO(atwilson): Set auto_start_enabled_ for other platforms that want this
  // functionality.
  if (!cros_user_.empty())
    auto_start_enabled_ = true;
}

ProfileSyncService::~ProfileSyncService() {
  Shutdown(false);
}

bool ProfileSyncService::AreCredentialsAvailable() {
  if (IsManaged()) {
    return false;
  }

  // CrOS user is always logged in. Chrome uses signin_ to check logged in.
  if (!cros_user_.empty() || !signin_->GetUsername().empty()) {
    // TODO(chron): Verify CrOS unit test behavior.
    return profile()->GetTokenService() &&
        profile()->GetTokenService()->HasTokenForService(
            browser_sync::SyncServiceName());
  }
  return false;
}

void ProfileSyncService::Initialize() {
  InitSettings();
  RegisterPreferences();

  // We clear this here (vs Shutdown) because we want to remember that an error
  // happened on shutdown so we can display details (message, location) about it
  // in about:sync.
  ClearStaleErrors();

  // Watch the preference that indicates sync is managed so we can take
  // appropriate action.
  pref_sync_managed_.Init(prefs::kSyncManaged, profile_->GetPrefs(), this);

  // For now, the only thing we can do through policy is to turn sync off.
  if (IsManaged()) {
    DisableForUser();
    return;
  }

  RegisterAuthNotifications();

  if (!HasSyncSetupCompleted())
    DisableForUser();  // Clean up in case of previous crash / setup abort.

  // In Chrome, we integrate a SigninManager which works with the sync
  // setup wizard to kick off the TokenService. CrOS does its own plumbing
  // for the TokenService in login and does not normally rely on signin_,
  // so only initialize this if the token service has not been initialized
  // (e.g. the browser crashed or is being debugged).
  if (cros_user_.empty() ||
      !profile_->GetTokenService()->Initialized()) {
    // Will load tokens from DB and broadcast Token events after.
    // Note: We rely on signin_ != NULL unless !cros_user_.empty().
    signin_->Initialize(profile_);
  }

  if (!HasSyncSetupCompleted()) {
    // If autostart is enabled, but we haven't completed sync setup, try to
    // start sync anyway (it's possible we crashed/shutdown after logging in
    // but before the backend finished initializing the last time).
    if (auto_start_enabled_ &&
        !profile_->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart) &&
        AreCredentialsAvailable()) {
      StartUp();
    }
  } else if (AreCredentialsAvailable()) {
    // If we have credentials and sync setup finished, autostart the backend.
    // Note that if we haven't finished setting up sync, backend bring up will
    // be done by the wizard.
    StartUp();
  }
}

void ProfileSyncService::RegisterAuthNotifications() {
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 Source<TokenService>(profile_->GetTokenService()));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_LOADING_FINISHED,
                 Source<TokenService>(profile_->GetTokenService()));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                 Source<TokenService>(profile_->GetTokenService()));
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
                 Source<Profile>(profile_));
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

void ProfileSyncService::RegisterPreferences() {
  PrefService* pref_service = profile_->GetPrefs();
  if (pref_service->FindPreference(prefs::kSyncLastSyncedTime))
    return;
  pref_service->RegisterInt64Pref(prefs::kSyncLastSyncedTime,
                                  0,
                                  PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncHasSetupCompleted,
                                    false,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncSuppressStart,
                                    false,
                                    PrefService::UNSYNCABLE_PREF);

  // If you've never synced before, or if you're using Chrome OS, all datatypes
  // are on by default.
  // TODO(nick): Perhaps a better model would be to always default to false,
  // and explicitly call SetDataTypes() when the user shows the wizard.
#if defined(OS_CHROMEOS)
  bool enable_by_default = true;
#else
  bool enable_by_default =
      !pref_service->HasPrefPath(prefs::kSyncHasSetupCompleted);
#endif

  pref_service->RegisterBooleanPref(prefs::kSyncBookmarks,
                                    true,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncPasswords,
                                    enable_by_default,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncPreferences,
                                    enable_by_default,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncAutofill,
                                    enable_by_default,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncThemes,
                                    enable_by_default,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncTypedUrls,
                                    enable_by_default,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncExtensionSettings,
                                    enable_by_default,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncExtensions,
                                    enable_by_default,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncApps,
                                    enable_by_default,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncSearchEngines,
                                    enable_by_default,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncSessions,
                                    enable_by_default,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kKeepEverythingSynced,
                                    enable_by_default,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncManaged,
                                    false,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterStringPref(prefs::kEncryptionBootstrapToken,
                                   "",
                                   PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncAutofillProfile,
                                    enable_by_default,
                                    PrefService::UNSYNCABLE_PREF);

  // We will start prompting people about new data types after the launch of
  // SESSIONS - all previously launched data types are treated as if they are
  // already acknowledged.
  syncable::ModelTypeBitSet model_set;
  model_set.set(syncable::BOOKMARKS);
  model_set.set(syncable::PREFERENCES);
  model_set.set(syncable::PASSWORDS);
  model_set.set(syncable::AUTOFILL_PROFILE);
  model_set.set(syncable::AUTOFILL);
  model_set.set(syncable::THEMES);
  model_set.set(syncable::EXTENSIONS);
  model_set.set(syncable::NIGORI);
  model_set.set(syncable::SEARCH_ENGINES);
  model_set.set(syncable::APPS);
  model_set.set(syncable::TYPED_URLS);
  model_set.set(syncable::SESSIONS);
  pref_service->RegisterListPref(prefs::kAcknowledgedSyncTypes,
                                 syncable::ModelTypeBitSetToValue(model_set),
                                 PrefService::UNSYNCABLE_PREF);
}

void ProfileSyncService::ClearPreferences() {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->ClearPref(prefs::kSyncLastSyncedTime);
  pref_service->ClearPref(prefs::kSyncHasSetupCompleted);
  pref_service->ClearPref(prefs::kEncryptionBootstrapToken);

  // TODO(nick): The current behavior does not clear e.g. prefs::kSyncBookmarks.
  // Is that really what we want?
  pref_service->ScheduleSavePersistentPrefs();
}

SyncCredentials ProfileSyncService::GetCredentials() {
  SyncCredentials credentials;
  credentials.email = cros_user_.empty() ? signin_->GetUsername() : cros_user_;
  DCHECK(!credentials.email.empty());
  TokenService* service = profile_->GetTokenService();
  credentials.sync_token = service->GetTokenForService(
      browser_sync::SyncServiceName());
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
    GetPreferredDataTypes(&initial_types);
  }

  SyncCredentials credentials = GetCredentials();

  scoped_refptr<net::URLRequestContextGetter> request_context_getter(
      profile_->GetRequestContext());

  if (delete_stale_data)
    ClearStaleErrors();

  backend_->Initialize(
      this,
      WeakHandle<JsEventHandler>(sync_js_controller_.AsWeakPtr()),
      sync_service_url_,
      initial_types,
      credentials,
      delete_stale_data);
}

void ProfileSyncService::CreateBackend() {
  backend_.reset(new SyncBackendHost(profile_->GetDebugName(), profile_));
}

bool ProfileSyncService::IsEncryptedDatatypeEnabled() const {
  if (encryption_pending())
    return true;
  syncable::ModelTypeSet preferred_types;
  GetPreferredDataTypes(&preferred_types);
  syncable::ModelTypeSet encrypted_types;
  GetEncryptedDataTypes(&encrypted_types);
  syncable::ModelTypeBitSet preferred_types_bitset =
      syncable::ModelTypeBitSetFromSet(preferred_types);
  syncable::ModelTypeBitSet encrypted_types_bitset =
      syncable::ModelTypeBitSetFromSet(encrypted_types);
  DCHECK(encrypted_types.count(syncable::PASSWORDS));
  return (preferred_types_bitset & encrypted_types_bitset).any();
}

void ProfileSyncService::StartUp() {
  // Don't start up multiple times.
  if (backend_.get()) {
    VLOG(1) << "Skipping bringing up backend host.";
    return;
  }

  DCHECK(AreCredentialsAvailable());

  last_synced_time_ = base::Time::FromInternalValue(
      profile_->GetPrefs()->GetInt64(prefs::kSyncLastSyncedTime));

  CreateBackend();

  // Initialize the backend.  Every time we start up a new SyncBackendHost,
  // we'll want to start from a fresh SyncDB, so delete any old one that might
  // be there.
  InitializeBackend(!HasSyncSetupCompleted());

  if (!sync_global_error_.get()) {
    sync_global_error_.reset(new SyncGlobalError(this));
    GlobalErrorServiceFactory::GetForProfile(profile_)->AddGlobalError(
        sync_global_error_.get());
    AddObserver(sync_global_error_.get());
  }
}

void ProfileSyncService::Shutdown(bool sync_disabled) {
  // Stop all data type controllers, if needed.
  if (data_type_manager_.get()) {
    if (data_type_manager_->state() != DataTypeManager::STOPPED) {
      // When aborting as part of shutdown, we should expect an aborted sync
      // configure result, else we'll dcheck when we try to read the sync error.
      expect_sync_configuration_aborted_ = true;
      data_type_manager_->Stop();
    }

    registrar_.Remove(this,
                      chrome::NOTIFICATION_SYNC_CONFIGURE_START,
                      Source<DataTypeManager>(data_type_manager_.get()));
    registrar_.Remove(this,
                      chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
                      Source<DataTypeManager>(data_type_manager_.get()));
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

  scoped_runnable_method_factory_.RevokeAll();

  // Clear various flags.
  expect_sync_configuration_aborted_ = false;
  is_auth_in_progress_ = false;
  backend_initialized_ = false;
  cached_passphrases_ = CachedPassphrases();
  encryption_pending_ = false;
  passphrase_required_reason_ = sync_api::REASON_PASSPHRASE_NOT_REQUIRED;
  last_attempted_user_email_.clear();
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
  ClearPreferences();
  Shutdown(true);

  signin_->SignOut();

  NotifyObservers();
}

bool ProfileSyncService::HasSyncSetupCompleted() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted);
}

void ProfileSyncService::SetSyncSetupCompleted() {
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kSyncHasSetupCompleted, true);
  prefs->SetBoolean(prefs::kSyncSuppressStart, false);

  prefs->ScheduleSavePersistentPrefs();
}

void ProfileSyncService::UpdateLastSyncedTime() {
  last_synced_time_ = base::Time::Now();
  profile_->GetPrefs()->SetInt64(prefs::kSyncLastSyncedTime,
      last_synced_time_.ToInternalValue());
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();
}

void ProfileSyncService::NotifyObservers() {
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
  // TODO(akalin): Make an Observer subclass that listens and does the
  // event routing.
  sync_js_controller_.HandleJsEvent(
      "onServiceStateChanged", JsEventDetails());
}

void ProfileSyncService::ClearStaleErrors() {
  unrecoverable_error_detected_ = false;
  unrecoverable_error_message_.clear();
  unrecoverable_error_location_ = tracked_objects::Location();
  last_actionable_error_ = SyncProtocolError();
}

// static
const char* ProfileSyncService::GetPrefNameForDataType(
    syncable::ModelType data_type) {
  switch (data_type) {
    case syncable::BOOKMARKS:
      return prefs::kSyncBookmarks;
    case syncable::PASSWORDS:
      return prefs::kSyncPasswords;
    case syncable::PREFERENCES:
      return prefs::kSyncPreferences;
    case syncable::AUTOFILL:
      return prefs::kSyncAutofill;
    case syncable::AUTOFILL_PROFILE:
      return prefs::kSyncAutofillProfile;
    case syncable::THEMES:
      return prefs::kSyncThemes;
    case syncable::TYPED_URLS:
      return prefs::kSyncTypedUrls;
    case syncable::EXTENSION_SETTINGS:
      return prefs::kSyncExtensionSettings;
    case syncable::EXTENSIONS:
      return prefs::kSyncExtensions;
    case syncable::APPS:
      return prefs::kSyncApps;
    case syncable::SEARCH_ENGINES:
      return prefs::kSyncSearchEngines;
    case syncable::SESSIONS:
      return prefs::kSyncSessions;
    default:
      break;
  }
  NOTREACHED();
  return NULL;
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
      RegisterDataTypeController(
          new browser_sync::SessionDataTypeController(factory_,
                                                      profile_,
                                                      this));
      return;
    case syncable::TYPED_URLS:
      RegisterDataTypeController(
          new browser_sync::TypedUrlDataTypeController(factory_, profile_));
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
  unrecoverable_error_detected_ = true;
  unrecoverable_error_message_ = message;
  unrecoverable_error_location_ = from_here;

  // Tell the wizard so it can inform the user only if it is already open.
  wizard_.Step(SyncSetupWizard::FATAL_ERROR);

  NotifyObservers();
  std::string location;
  from_here.Write(true, true, &location);
  LOG(ERROR)
      << "Unrecoverable error detected at " << location
      << " -- ProfileSyncService unusable: " << message;

  // Shut all data types down.
  MessageLoop::current()->PostTask(FROM_HERE,
        scoped_runnable_method_factory_.NewRunnableMethod(
        &ProfileSyncService::Shutdown, true));
}

void ProfileSyncService::OnBackendInitialized(
    const WeakHandle<JsBackend>& js_backend, bool success) {
  if (!success) {
    // If backend initialization failed, abort.  We only want to blow away
    // state (DBs, etc) if this was a first-time scenario that failed.
    wizard_.Step(SyncSetupWizard::FATAL_ERROR);
    Shutdown(!HasSyncSetupCompleted());
    return;
  }

  backend_initialized_ = true;

  sync_js_controller_.AttachJsBackend(js_backend);

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  last_synced_time_ will only be null
  // in this case, because the pref wasn't restored on StartUp.
  if (last_synced_time_.is_null()) {
    UpdateLastSyncedTime();
  }
  NotifyObservers();

  if (auto_start_enabled_ && !SetupInProgress()) {
    // Backend is initialized but we're not in sync setup, so this must be an
    // autostart - mark our sync setup as completed.
    if (profile_->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart)) {
      // TODO(sync): This call to ShowConfigure() should go away in favor
      // of the code below that calls wizard_.Step() - http://crbug.com/95269.
      ShowConfigure(true);
      return;
    } else {
      SetSyncSetupCompleted();
    }
  }

  if (HasSyncSetupCompleted()) {
    ConfigureDataTypeManager();
  } else if (SetupInProgress()) {
    wizard_.Step(SyncSetupWizard::SYNC_EVERYTHING);
  } else {
    // This should only be hit during integration tests, but there's no good
    // way to assert this.
    DVLOG(1) << "Setup not complete, no wizard - integration tests?";
  }
}

void ProfileSyncService::OnSyncCycleCompleted() {
  UpdateLastSyncedTime();
  VLOG(2) << "Notifying observers sync cycle completed";
  NotifyObservers();
}

// TODO(sync): eventually support removing datatypes too.
void ProfileSyncService::OnDataTypesChanged(
    const syncable::ModelTypeSet& to_add) {
  // We don't bother doing anything if the migrator is busy.
  if (migrator_->state() != browser_sync::BackendMigrator::IDLE) {
    VLOG(1) << "Dropping OnDataTypesChanged due to migrator busy.";
    return;
  }

  syncable::ModelTypeSet new_types;
  syncable::ModelTypeSet preferred_types;
  GetPreferredDataTypes(&preferred_types);
  syncable::ModelTypeSet registered_types;
  GetRegisteredDataTypes(&registered_types);

  for (syncable::ModelTypeSet::const_iterator iter = to_add.begin();
       iter != to_add.end();
       ++iter) {
    // Received notice to enable session sync. Check if sessions are
    // registered, and if not register a new datatype controller.
    if (registered_types.count(*iter) == 0) {
      RegisterNewDataType(*iter);
      // Enable the about:flags switch for sessions so we don't have to always
      // perform this reconfiguration. Once we set this, sessions will remain
      // registered, so we will no longer go down this code path.
      std::string experiment_name = GetExperimentNameForDataType(*iter);
      if (experiment_name.empty())
        continue;
      about_flags::SetExperimentEnabled(g_browser_process->local_state(),
                                        experiment_name,
                                        true);

      // Check if the user has "Keep Everything Synced" enabled. If so, we want
      // to turn on sessions if it's not already on. Otherwise we leave it off.
      // Note: if sessions are already registered, we don't turn it on. This
      // covers the case where we're already in the process of reconfiguring
      // to turn sessions on.
      if (profile_->GetPrefs()->GetBoolean(prefs::kKeepEverythingSynced) &&
          preferred_types.count(*iter) == 0){
        std::string pref_name = GetPrefNameForDataType(*iter);
        if (pref_name.empty())
          continue;
        profile_->GetPrefs()->SetBoolean(pref_name.c_str(), true);
      }
    }
  }

  if (!new_types.empty()) {
    VLOG(1) << "Dynamically enabling new datatypes: "
            << syncable::ModelTypeSetToString(new_types);
    OnMigrationNeededForTypes(new_types);
  }
}

void ProfileSyncService::UpdateAuthErrorState(
    const GoogleServiceAuthError& error) {
  last_auth_error_ = error;
  // Protect against the in-your-face dialogs that pop out of nowhere.
  // Require the user to click somewhere to run the setup wizard in the case
  // of a steady-state auth failure.
  if (WizardIsVisible()) {
    wizard_.Step(last_auth_error_.state() == AuthError::NONE ?
        SyncSetupWizard::GAIA_SUCCESS : SyncSetupWizard::GetLoginState());
  } else {
    auth_error_time_ = base::TimeTicks::Now();
  }

  if (!auth_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("Sync.AuthorizationTimeInNetwork",
                    base::TimeTicks::Now() - auth_start_time_);
    auth_start_time_ = base::TimeTicks();
  }

  is_auth_in_progress_ = false;
  // Fan the notification out to interested UI-thread components.
  NotifyObservers();
}

void ProfileSyncService::OnAuthError() {
  UpdateAuthErrorState(backend_->GetAuthError());
}

void ProfileSyncService::OnStopSyncingPermanently() {
  if (SetupInProgress()) {
    wizard_.Step(SyncSetupWizard::SETUP_ABORTED_BY_PENDING_CLEAR);
    expect_sync_configuration_aborted_ = true;
  }
  profile_->GetPrefs()->SetBoolean(prefs::kSyncSuppressStart, true);
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
    sync_api::PassphraseRequiredReason reason) {
  DCHECK(backend_.get());
  DCHECK(backend_->IsNigoriEnabled());

  // TODO(lipalani) : add this check to other locations as well.
  if (unrecoverable_error_detected_) {
    // When unrecoverable error is detected we post a task to shutdown the
    // backend. The task might not have executed yet.
    return;
  }

  VLOG(1) << "Passphrase required with reason: "
          << sync_api::PassphraseRequiredReasonToString(reason);
  passphrase_required_reason_ = reason;

  // We will skip the passphrase prompt and suppress the warning if the
  // passphrase is needed for decryption but the user is not syncing an
  // encrypted data type on this machine. Otherwise we look for one.
  if (!IsEncryptedDatatypeEnabled() && IsPassphraseRequiredForDecryption()) {
    VLOG(1) << "Decrypting and no encrypted datatypes enabled"
            << ", accepted passphrase.";
    OnPassphraseAccepted();
  }

  // First try supplying gaia password as the passphrase.
  // TODO(atwilson): This logic seems odd here - we know what kind of passphrase
  // is required (explicit/gaia) so we should not bother setting the wrong kind
  // of passphrase - http://crbug.com/95269.
  if (!cached_passphrases_.gaia_passphrase.empty()) {
    std::string gaia_passphrase = cached_passphrases_.gaia_passphrase;
    cached_passphrases_.gaia_passphrase.clear();
    VLOG(1) << "Attempting gaia passphrase.";
    // SetPassphrase will re-cache this passphrase if the syncer isn't ready.
    SetPassphrase(gaia_passphrase, false);
    return;
  }

  // If the above failed then try the custom passphrase the user might have
  // entered in setup.
  if (!cached_passphrases_.explicit_passphrase.empty()) {
    std::string explicit_passphrase = cached_passphrases_.explicit_passphrase;
    cached_passphrases_.explicit_passphrase.clear();
    VLOG(1) << "Attempting explicit passphrase.";
    // SetPassphrase will re-cache this passphrase if the syncer isn't ready.
    SetPassphrase(explicit_passphrase, true);
    return;
  }

  // Prompt the user for a password.
  if (WizardIsVisible() && IsEncryptedDatatypeEnabled() &&
      IsPassphraseRequiredForDecryption()) {
    VLOG(1) << "Prompting user for passphrase.";
    wizard_.Step(SyncSetupWizard::ENTER_PASSPHRASE);
  }

  NotifyObservers();
}

void ProfileSyncService::OnPassphraseAccepted() {
  VLOG(1) << "Received OnPassphraseAccepted.";
  // Don't hold on to a passphrase in raw form longer than needed.
  cached_passphrases_ = CachedPassphrases();

  // Make sure the data types that depend on the passphrase are started at
  // this time.
  syncable::ModelTypeSet types;
  GetPreferredDataTypes(&types);

  // Reset passphrase_required_reason_ before configuring the DataTypeManager
  // since we know we no longer require the passphrase.
  passphrase_required_reason_ = sync_api::REASON_PASSPHRASE_NOT_REQUIRED;

  if (data_type_manager_.get()) {
    // Unblock the data type manager if necessary.
    // This will always trigger a SYNC_CONFIGURE_DONE on completion, which will
    // step the UI wizard into DONE state (even if no datatypes have changed).
    data_type_manager_->Configure(types,
                                  sync_api::CONFIGURE_REASON_RECONFIGURATION);
  }

  NotifyObservers();
}

void ProfileSyncService::OnEncryptionComplete(
    const syncable::ModelTypeSet& encrypted_types) {
  if (encryption_pending_) {
    syncable::ModelTypeSet registered_types;
    GetRegisteredDataTypes(&registered_types);
    bool encryption_complete = true;
    for (syncable::ModelTypeSet::const_iterator it = registered_types.begin();
         it != registered_types.end();
         ++it) {
      if (encrypted_types.count(*it) == 0) {
        // One of our types is not yet encrypted - keep waiting.
        encryption_complete = false;
        break;
      }
    }
    if (encryption_complete) {
      encryption_pending_ = false;
      // The user had chosen to encrypt datatypes. This is the last thing to
      // complete, so now that we're done notify the UI.
      wizard_.Step(SyncSetupWizard::DONE);
    }
  }
  NotifyObservers();
}

void ProfileSyncService::OnMigrationNeededForTypes(
    const syncable::ModelTypeSet& types) {
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
      if (SetupInProgress()) {
        wizard_.Step(SyncSetupWizard::ABORT);
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

void ProfileSyncService::ShowLoginDialog() {
  if (WizardIsVisible()) {
    wizard_.Focus();
    // Force the wizard to step to the login screen (which will only actually
    // happen if the transition is valid).
    wizard_.Step(SyncSetupWizard::GetLoginState());
    return;
  }

  if (!auth_error_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Sync.ReauthorizationTime",
                             base::TimeTicks::Now() - auth_error_time_);
    auth_error_time_ = base::TimeTicks();  // Reset auth_error_time_ to null.
  }

  ShowSyncSetupWithWizard(SyncSetupWizard::GetLoginState());

  NotifyObservers();
}

void ProfileSyncService::ShowErrorUI() {
  if (WizardIsVisible()) {
    wizard_.Focus();
    return;
  }

  if (last_auth_error_.state() != AuthError::NONE)
    ShowLoginDialog();
  else if (ShouldShowActionOnUI(last_actionable_error_))
    ShowSyncSetup(chrome::kPersonalOptionsSubPage);
  else
    ShowSyncSetupWithWizard(SyncSetupWizard::NONFATAL_ERROR);
}

void ProfileSyncService::ShowConfigure(bool sync_everything) {
  if (!sync_initialized()) {
    LOG(ERROR) << "Attempted to show sync configure before backend ready.";
    return;
  }
  if (WizardIsVisible()) {
    wizard_.Focus();
    return;
  }

  if (sync_everything)
    ShowSyncSetupWithWizard(SyncSetupWizard::SYNC_EVERYTHING);
  else
    ShowSyncSetupWithWizard(SyncSetupWizard::CONFIGURE);
}

void ProfileSyncService::ShowSyncSetup(const std::string& sub_page) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile());
  if (!browser) {
    browser = Browser::Create(profile());
    browser->ShowOptionsTab(sub_page);
    browser->window()->Show();
  } else {
    browser->ShowOptionsTab(sub_page);
  }
}


void ProfileSyncService::ShowSyncSetupWithWizard(SyncSetupWizard::State state) {
  wizard_.Step(state);
  ShowSyncSetup(chrome::kSyncSetupSubPage);
}

SyncBackendHost::StatusSummary ProfileSyncService::QuerySyncStatusSummary() {
  if (backend_.get() && backend_initialized_)
    return backend_->GetStatusSummary();
  else
    return SyncBackendHost::Status::OFFLINE_UNUSABLE;
}

SyncBackendHost::Status ProfileSyncService::QueryDetailedSyncStatus() {
  if (backend_.get() && backend_initialized_) {
    return backend_->GetDetailedStatus();
  } else {
    SyncBackendHost::Status status;
    status.summary = SyncBackendHost::Status::OFFLINE_UNUSABLE;
    status.sync_protocol_error = last_actionable_error_;
    return status;
  }
}

bool ProfileSyncService::SetupInProgress() const {
  return !HasSyncSetupCompleted() && WizardIsVisible();
}

std::string ProfileSyncService::BuildSyncStatusSummaryText(
    const sync_api::SyncManager::Status::Summary& summary) {
  const char* strings[] = {"INVALID", "OFFLINE", "OFFLINE_UNSYNCED", "SYNCING",
      "READY", "CONFLICT", "OFFLINE_UNUSABLE"};
  COMPILE_ASSERT(arraysize(strings) ==
                 sync_api::SyncManager::Status::SUMMARY_STATUS_COUNT,
                 enum_indexed_array);
  if (summary < 0 ||
      summary >= sync_api::SyncManager::Status::SUMMARY_STATUS_COUNT) {
    LOG(DFATAL) << "Illegal Summary Value: " << summary;
    return "UNKNOWN";
  }
  return strings[summary];
}

bool ProfileSyncService::sync_initialized() const {
  return backend_initialized_;
}

bool ProfileSyncService::unrecoverable_error_detected() const {
  return unrecoverable_error_detected_;
}

string16 ProfileSyncService::GetLastSyncedTimeString() const {
  if (last_synced_time_.is_null())
    return l10n_util::GetStringUTF16(IDS_SYNC_TIME_NEVER);

  base::TimeDelta last_synced = base::Time::Now() - last_synced_time_;

  if (last_synced < base::TimeDelta::FromMinutes(1))
    return l10n_util::GetStringUTF16(IDS_SYNC_TIME_JUST_NOW);

  return TimeFormat::TimeElapsed(last_synced);
}

string16 ProfileSyncService::GetAuthenticatedUsername() const {
  if (backend_.get() && backend_initialized_)
    return backend_->GetAuthenticatedUsername();
  else
    return string16();
}

void ProfileSyncService::OnUserSubmittedAuth(
    const std::string& username, const std::string& password,
    const std::string& captcha, const std::string& access_code) {
  last_attempted_user_email_ = username;
  is_auth_in_progress_ = true;
  NotifyObservers();

  auth_start_time_ = base::TimeTicks::Now();

  if (!signin_->IsInitialized()) {
    // In ChromeOS we sign in during login, so we do not initialize signin_.
    // If this function gets called, we need to re-authenticate (e.g. for
    // two factor signin), so initialize signin_ here.
    signin_->Initialize(profile_);
  }

  if (!access_code.empty()) {
    signin_->ProvideSecondFactorAccessCode(access_code);
    return;
  }

  if (!signin_->GetUsername().empty()) {
    signin_->SignOut();
  }

  // The user has submitted credentials, which indicates they don't
  // want to suppress start up anymore.
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kSyncSuppressStart, false);
  prefs->ScheduleSavePersistentPrefs();

  signin_->StartSignIn(username,
                       password,
                       last_auth_error_.captcha().token,
                       captcha);
}

void ProfileSyncService::OnUserSubmittedOAuth(
    const std::string& oauth1_request_token) {
  is_auth_in_progress_ = true;

  // The user has submitted credentials, which indicates they don't
  // want to suppress start up anymore.
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kSyncSuppressStart, false);
  prefs->ScheduleSavePersistentPrefs();

  signin_->StartOAuthSignIn(oauth1_request_token);
}

void ProfileSyncService::OnUserChoseDatatypes(bool sync_everything,
    const syncable::ModelTypeSet& chosen_types) {
  if (!backend_.get() &&
      unrecoverable_error_detected_ == false) {
    NOTREACHED();
    return;
  }

  profile_->GetPrefs()->SetBoolean(prefs::kKeepEverythingSynced,
      sync_everything);

  ChangePreferredDataTypes(chosen_types);
  AcknowledgeSyncedTypes();
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();
}

void ProfileSyncService::OnUserCancelledDialog() {
  if (!HasSyncSetupCompleted()) {
    // A sync dialog was aborted before authentication.
    // Rollback.
    expect_sync_configuration_aborted_ = true;
    DisableForUser();
  }

  // If the user attempted to encrypt datatypes, but was unable to do so, we
  // allow them to cancel out.
  encryption_pending_ = false;

  // Though an auth could still be in progress, once the dialog is closed we
  // don't want the UI to stay stuck in the "waiting for authentication" state
  // as that could take forever.  We set this to false so the buttons to re-
  // login will appear until either a) the original request finishes and
  // succeeds, calling OnAuthError(NONE), or b) the user clicks the button,
  // and tries to re-authenticate. (b) is a little awkward as this second
  // request will get queued behind the first and could wind up "undoing" the
  // good if invalid creds were provided, but it's an edge case and the user
  // can of course get themselves out of it.
  is_auth_in_progress_ = false;
  NotifyObservers();
}

void ProfileSyncService::ChangePreferredDataTypes(
    const syncable::ModelTypeSet& preferred_types) {

  VLOG(1) << "ChangePreferredDataTypes invoked";
  // Filter out any datatypes which aren't registered, or for which
  // the preference can't be set.
  syncable::ModelTypeSet registered_types;
  GetRegisteredDataTypes(&registered_types);
  for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType model_type = syncable::ModelTypeFromInt(i);
    if (!registered_types.count(model_type))
      continue;
    const char* pref_name = GetPrefNameForDataType(model_type);
    if (!pref_name)
      continue;
    profile_->GetPrefs()->SetBoolean(pref_name,
        preferred_types.count(model_type) != 0);
    if (syncable::AUTOFILL == model_type) {
      profile_->GetPrefs()->SetBoolean(prefs::kSyncAutofillProfile,
          preferred_types.count(model_type) != 0);
    }
  }

  // If we haven't initialized yet, don't configure the DTM as it could cause
  // association to start before a Directory has even been created.
  if (backend_initialized_) {
    DCHECK(backend_.get());
    ConfigureDataTypeManager();
  } else if (unrecoverable_error_detected()) {
    // TODO(tim): crbug.com/87575 . We should have per data type unrecoverable
    // errors.

    // Close the wizard.
    if (WizardIsVisible()) {
       wizard_.Step(SyncSetupWizard::DONE);
    }
    // There is nothing more to configure. So inform the listeners,
    NotifyObservers();

    VLOG(1) << "ConfigureDataTypeManager not invoked because of an "
            << "Unrecoverable error.";
  } else {
    VLOG(0) << "ConfigureDataTypeManager not invoked because backend is not "
            << "initialized";
  }
}

void ProfileSyncService::GetPreferredDataTypes(
    syncable::ModelTypeSet* preferred_types) const {
  preferred_types->clear();
  if (profile_->GetPrefs()->GetBoolean(prefs::kKeepEverythingSynced)) {
    GetRegisteredDataTypes(preferred_types);
  } else {
    // Filter out any datatypes which aren't registered, or for which
    // the preference can't be read.
    syncable::ModelTypeSet registered_types;
    GetRegisteredDataTypes(&registered_types);
    for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
      syncable::ModelType model_type = syncable::ModelTypeFromInt(i);
      if (!registered_types.count(model_type))
        continue;
      if (model_type == syncable::AUTOFILL_PROFILE)
        continue;
      const char* pref_name = GetPrefNameForDataType(model_type);
      if (!pref_name)
        continue;

      // We are trying to group autofill_profile tag with the same
      // enabled/disabled state as autofill. Because the UI only shows autofill.
      if (profile_->GetPrefs()->GetBoolean(pref_name)) {
        preferred_types->insert(model_type);
        if (model_type == syncable::AUTOFILL) {
          if (!registered_types.count(syncable::AUTOFILL_PROFILE))
            continue;
          preferred_types->insert(syncable::AUTOFILL_PROFILE);
        }
      }
    }
  }
}

void ProfileSyncService::GetRegisteredDataTypes(
    syncable::ModelTypeSet* registered_types) const {
  registered_types->clear();
  // The data_type_controllers_ are determined by command-line flags; that's
  // effectively what controls the values returned here.
  for (DataTypeController::TypeMap::const_iterator it =
       data_type_controllers_.begin();
       it != data_type_controllers_.end(); ++it) {
    registered_types->insert((*it).first);
  }
}

bool ProfileSyncService::IsUsingSecondaryPassphrase() const {
  return backend_.get() && backend_->IsUsingExplicitPassphrase();
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
                   Source<DataTypeManager>(data_type_manager_.get()));
    registrar_.Add(this,
                   chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
                   Source<DataTypeManager>(data_type_manager_.get()));

    // We create the migrator at the same time.
    migrator_.reset(
        new browser_sync::BackendMigrator(
            profile_->GetDebugName(), GetUserShare(),
            this, data_type_manager_.get()));
  }

  syncable::ModelTypeSet types;
  GetPreferredDataTypes(&types);
  if (IsPassphraseRequiredForDecryption()) {
    if (IsEncryptedDatatypeEnabled()) {
      // We need a passphrase still. We don't bother to attempt to configure
      // until we receive an OnPassphraseAccepted (which triggers a configure).
      VLOG(1) << "ProfileSyncService::ConfigureDataTypeManager bailing out "
              << "because a passphrase required";
      return;
    } else {
      // We've been informed that a passphrase is required for decryption, but
      // now there are no encrypted data types enabled, so change the value of
      // passphrase_required_reason_ to its default value. (NotifyObservers()
      // will be called when configuration completes).
      passphrase_required_reason_ = sync_api::REASON_PASSPHRASE_NOT_REQUIRED;
    }
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

const browser_sync::sessions::SyncSessionSnapshot*
    ProfileSyncService::GetLastSessionSnapshot() const {
  if (backend_.get() && backend_initialized_) {
    return backend_->GetLastSessionSnapshot();
  }
  NOTREACHED();
  return NULL;
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

void ProfileSyncService::SetPassphrase(const std::string& passphrase,
                                       bool is_explicit) {
  if (ShouldPushChanges() || IsPassphraseRequired()) {
    VLOG(1) << "Setting " << (is_explicit ? "explicit" : "implicit");
    backend_->SetPassphrase(passphrase, is_explicit);
  } else {
    if (is_explicit) {
      cached_passphrases_.explicit_passphrase = passphrase;
    } else {
      cached_passphrases_.gaia_passphrase = passphrase;
    }
  }
}

void ProfileSyncService::SetEncryptEverything(bool encrypt_everything) {
  encryption_pending_ = encrypt_everything;
}

bool ProfileSyncService::encryption_pending() const {
  return encryption_pending_;
}

bool ProfileSyncService::EncryptEverythingEnabled() const {
  if (!backend_.get() || !backend_initialized_) {
    NOTREACHED() << "Cannot check encryption without initialized backend.";
    return false;
  }
  return backend_->EncryptEverythingEnabled();
}

// This will open a transaction to get the encrypted types. Do not call this
// if you already have a transaction open.
void ProfileSyncService::GetEncryptedDataTypes(
    syncable::ModelTypeSet* encrypted_types) const {
  CHECK(encrypted_types);
  if (backend_.get()) {
    *encrypted_types = backend_->GetEncryptedDataTypes();
    DCHECK(encrypted_types->count(syncable::PASSWORDS));
  } else {
    // Either we are in an unrecoverable error or the sync is not yet done
    // initializing. In either case just return the sensitive types. During
    // sync initialization the UI might need to know what our encrypted
    // types are.
    *encrypted_types = browser_sync::Cryptographer::SensitiveTypes();
    DCHECK(encrypted_types->count(syncable::PASSWORDS));
  }
}

void ProfileSyncService::Observe(int type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_SYNC_CONFIGURE_START: {
      NotifyObservers();
      // TODO(sync): Maybe toast?
      break;
    }
    case chrome::NOTIFICATION_SYNC_CONFIGURE_DONE: {
      DataTypeManager::ConfigureResult* result =
          Details<DataTypeManager::ConfigureResult>(details).ptr();

      DataTypeManager::ConfigureStatus status = result->status;
      VLOG(1) << "PSS SYNC_CONFIGURE_DONE called with status: " << status;
      if (status == DataTypeManager::ABORTED &&
          expect_sync_configuration_aborted_) {
        VLOG(0) << "ProfileSyncService::Observe Sync Configure aborted";
        expect_sync_configuration_aborted_ = false;
        return;
      }
      if (status != DataTypeManager::OK) {
        DCHECK(result->error.IsSet());
        std::string message =
          "Sync configuration failed with status " +
          DataTypeManager::ConfigureStatusToString(status) +
          " during " + syncable::ModelTypeToString(result->error.type()) +
          ": " + result->error.message();
        LOG(ERROR) << "ProfileSyncService error: "
                   << message;
        // TODO: Don't
        OnUnrecoverableError(result->error.location(), message);
        return;
      }

      // We should never get in a state where we have no encrypted datatypes
      // enabled, and yet we still think we require a passphrase for decryption.
      DCHECK(!(IsPassphraseRequiredForDecryption() &&
               !IsEncryptedDatatypeEnabled()));

      // In the old world, this would be a no-op.  With new syncer thread,
      // this is the point where it is safe to switch from config-mode to
      // normal operation.
      backend_->StartSyncingWithServer();

      if (!encryption_pending_) {
        wizard_.Step(SyncSetupWizard::DONE);
        NotifyObservers();
      } else {
        backend_->EnableEncryptEverything();
      }
      break;
    }
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name = Details<std::string>(details).ptr();
      if (*pref_name == prefs::kSyncManaged) {
        NotifyObservers();
        if (*pref_sync_managed_) {
          DisableForUser();
        } else if (HasSyncSetupCompleted() && AreCredentialsAvailable()) {
          StartUp();
        }
      }
      break;
    }
    case chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED: {
      GoogleServiceAuthError error =
          *(Details<const GoogleServiceAuthError>(details).ptr());
      UpdateAuthErrorState(error);
      break;
    }
    case chrome::NOTIFICATION_TOKEN_REQUEST_FAILED: {
      GoogleServiceAuthError error(
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
      UpdateAuthErrorState(error);
      break;
    }
    case chrome::NOTIFICATION_TOKEN_AVAILABLE: {
      if (AreCredentialsAvailable()) {
        if (backend_initialized_) {
          backend_->UpdateCredentials(GetCredentials());
        }
        if (!profile_->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart))
          StartUp();
      }
      break;
    }
    case chrome::NOTIFICATION_TOKEN_LOADING_FINISHED: {
      // If not in Chrome OS, and we have a username without tokens,
      // the user will need to signin again, so sign out.
      if (cros_user_.empty() &&
          !signin_->GetUsername().empty() &&
          !AreCredentialsAvailable()) {
        DisableForUser();
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

bool ProfileSyncService::IsManaged() {
  // Some tests use ProfileSyncServiceMock which doesn't have a profile.
  return profile_ && profile_->GetPrefs()->GetBoolean(prefs::kSyncManaged);
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

void ProfileSyncService::AcknowledgeSyncedTypes() {
  syncable::ModelTypeBitSet acknowledged = syncable::ModelTypeBitSetFromValue(
      *profile_->GetPrefs()->GetList(prefs::kAcknowledgedSyncTypes));
  syncable::ModelTypeSet registered;
  GetRegisteredDataTypes(&registered);
  syncable::ModelTypeBitSet registered_bit_set =
      syncable::ModelTypeBitSetFromSet(registered);

  // Add the registered types to the current set of acknowledged types, and then
  // store the resulting set in prefs.
  acknowledged |= registered_bit_set;
  scoped_ptr<ListValue> value(syncable::ModelTypeBitSetToValue(acknowledged));
  profile_->GetPrefs()->Set(prefs::kAcknowledgedSyncTypes, *value);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();
}

// TODO(sync): When we add the next new data type, add code to the NTP to
// display a "new data type available" notification based on these
// unacknowledged types, or remove this code if we decide to just bake this
// in to the "new feature" canned text.
syncable::ModelTypeBitSet ProfileSyncService::GetUnacknowledgedTypes() const {
  syncable::ModelTypeBitSet unacknowledged;
  if (HasSyncSetupCompleted() &&
      profile_->GetPrefs()->GetBoolean(prefs::kKeepEverythingSynced)) {
    // User is "syncing everything" - see if we've added any new data types.
    syncable::ModelTypeBitSet acknowledged =
        syncable::ModelTypeBitSetFromValue(
            *profile_->GetPrefs()->GetList(prefs::kAcknowledgedSyncTypes));
    syncable::ModelTypeSet registered;
    GetRegisteredDataTypes(&registered);
    syncable::ModelTypeBitSet registered_bit_set =
        syncable::ModelTypeBitSetFromSet(registered);
    unacknowledged = registered_bit_set & ~acknowledged;
  }
  return unacknowledged;
}
