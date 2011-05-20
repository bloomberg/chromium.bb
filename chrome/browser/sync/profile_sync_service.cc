// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service.h"

#include <stddef.h>
#include <map>
#include <ostream>
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
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/backend_migrator.h"
#include "chrome/browser/sync/engine/configure_reason.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/glue/session_data_type_controller.h"
#include "chrome/browser/sync/js_arg_list.h"
#include "chrome/browser/sync/js_event_details.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/signin_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using browser_sync::ChangeProcessor;
using browser_sync::DataTypeController;
using browser_sync::DataTypeManager;
using browser_sync::SyncBackendHost;
using sync_api::SyncCredentials;

typedef GoogleServiceAuthError AuthError;

const char* ProfileSyncService::kSyncServerUrl =
    "https://clients4.google.com/chrome-sync";

const char* ProfileSyncService::kDevServerUrl =
    "https://clients4.google.com/chrome-sync/dev";

static const int kSyncClearDataTimeoutInSeconds = 60;  // 1 minute.

ProfileSyncService::ProfileSyncService(ProfileSyncFactory* factory,
                                       Profile* profile,
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
      unrecoverable_error_detected_(false),
      scoped_runnable_method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      expect_sync_configuration_aborted_(false),
      clear_server_data_state_(CLEAR_NOT_STARTED) {
  // By default, dev, canary, and unbranded Chromium users will go to the
  // development servers. Development servers have more features than standard
  // sync servers. Users with officially-branded Chrome stable and beta builds
  // will go to the standard sync servers.
  //
  // GetChannel hits the registry on Windows. See http://crbug.com/70380.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  platform_util::Channel channel = platform_util::GetChannel();
  if (channel == platform_util::CHANNEL_STABLE ||
      channel == platform_util::CHANNEL_BETA) {
    sync_service_url_ = GURL(kSyncServerUrl);
  }

  tried_implicit_gaia_remove_when_bug_62103_fixed_ = false;
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
    if (profile()->GetTokenService() &&
        profile()->GetTokenService()->HasTokenForService(
            GaiaConstants::kSyncService)) {
      return true;
    }
  }
  return false;
}

void ProfileSyncService::Initialize() {
  InitSettings();
  RegisterPreferences();

  // Watch the preference that indicates sync is managed so we can take
  // appropriate action.
  pref_sync_managed_.Init(prefs::kSyncManaged, profile_->GetPrefs(), this);

  // For now, the only thing we can do through policy is to turn sync off.
  if (IsManaged()) {
    DisableForUser();
    return;
  }

  RegisterAuthNotifications();

  // In Chrome, we integrate a SigninManager which works with the sync
  // setup wizard to kick off the TokenService. CrOS does its own plumbing
  // for the TokenService.
  if (cros_user_.empty()) {
    // Will load tokens from DB and broadcast Token events after.
    // Note: We rely on signin_ != NULL unless !cros_user_.empty().
    signin_.reset(new SigninManager());
    signin_->Initialize(profile_);
  }

  if (!HasSyncSetupCompleted()) {
    DisableForUser();  // Clean up in case of previous crash / setup abort.

    // Under ChromeOS, just autostart it anyway if creds are here and start
    // is not being suppressed by preferences.
    if (!cros_user_.empty() &&
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
                 NotificationType::TOKEN_AVAILABLE,
                 Source<TokenService>(profile_->GetTokenService()));
  registrar_.Add(this,
                 NotificationType::TOKEN_LOADING_FINISHED,
                 Source<TokenService>(profile_->GetTokenService()));
  registrar_.Add(this,
                 NotificationType::GOOGLE_SIGNIN_SUCCESSFUL,
                 Source<Profile>(profile_));
  registrar_.Add(this,
                 NotificationType::GOOGLE_SIGNIN_FAILED,
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
  pref_service->RegisterBooleanPref(prefs::kSyncExtensions,
                                    enable_by_default,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kSyncApps,
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
  credentials.email = !cros_user_.empty() ? cros_user_ : signin_->GetUsername();
  DCHECK(!credentials.email.empty());
  TokenService* service = profile_->GetTokenService();
  credentials.sync_token = service->GetTokenForService(
      GaiaConstants::kSyncService);
  return credentials;
}

void ProfileSyncService::InitializeBackend(bool delete_sync_data_folder) {
  if (!backend_.get()) {
    NOTREACHED();
    return;
  }

  syncable::ModelTypeSet types;
  // If sync setup hasn't finished, we don't want to initialize routing info
  // for any data types so that we don't download updates for types that the
  // user chooses not to sync on the first DownloadUpdatesCommand.
  if (HasSyncSetupCompleted()) {
    GetPreferredDataTypes(&types);
  }

  SyncCredentials credentials = GetCredentials();

  backend_->Initialize(this,
                       sync_service_url_,
                       types,
                       profile_->GetRequestContext(),
                       credentials,
                       delete_sync_data_folder);
}

void ProfileSyncService::CreateBackend() {
  backend_.reset(new SyncBackendHost(profile_));
}

bool ProfileSyncService::IsEncryptedDatatypeEnabled() const {
  syncable::ModelTypeSet preferred_types;
  GetPreferredDataTypes(&preferred_types);
  syncable::ModelTypeBitSet preferred_types_bitset =
      syncable::ModelTypeBitSetFromSet(preferred_types);
  syncable::ModelTypeBitSet encrypted_types_bitset =
      syncable::ModelTypeBitSetFromSet(encrypted_types_.current);
  DCHECK(encrypted_types_.current.count(syncable::PASSWORDS));
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
}

void ProfileSyncService::Shutdown(bool sync_disabled) {
  // Stop all data type controllers, if needed.
  if (data_type_manager_.get()) {
    if (data_type_manager_->state() != DataTypeManager::STOPPED) {
      data_type_manager_->Stop();
    }

    registrar_.Remove(this,
                      NotificationType::SYNC_CONFIGURE_START,
                      Source<DataTypeManager>(data_type_manager_.get()));
    registrar_.Remove(this,
                      NotificationType::SYNC_CONFIGURE_DONE,
                      Source<DataTypeManager>(data_type_manager_.get()));
    data_type_manager_.reset();
  }

  js_event_handlers_.RemoveBackend();

  // Move aside the backend so nobody else tries to use it while we are
  // shutting it down.
  scoped_ptr<SyncBackendHost> doomed_backend(backend_.release());
  if (doomed_backend.get()) {
    doomed_backend->Shutdown(sync_disabled);

    doomed_backend.reset();
  }

  // Clear various flags.
  is_auth_in_progress_ = false;
  backend_initialized_ = false;
  passphrase_required_reason_ = sync_api::REASON_PASSPHRASE_NOT_REQUIRED;
  last_attempted_user_email_.clear();
  last_auth_error_ = GoogleServiceAuthError::None();
}

void ProfileSyncService::ClearServerData() {
  clear_server_data_state_ = CLEAR_CLEARING;
  clear_server_data_timer_.Start(
      base::TimeDelta::FromSeconds(kSyncClearDataTimeoutInSeconds), this,
      &ProfileSyncService::OnClearServerDataTimeout);
  backend_->RequestClearServerData();
}

void ProfileSyncService::DisableForUser() {
  // Clear prefs (including SyncSetupHasCompleted) before shutting down so
  // PSS clients don't think we're set up while we're shutting down.
  ClearPreferences();
  Shutdown(true);

  if (signin_.get()) {
    signin_->SignOut();
  }

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
  js_event_handlers_.RouteJsEvent(
      "onServiceStateChanged", browser_sync::JsEventDetails());
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
    case syncable::EXTENSIONS:
      return prefs::kSyncExtensions;
    case syncable::APPS:
      return prefs::kSyncApps;
    case syncable::SESSIONS:
      return prefs::kSyncSessions;
    default:
      break;
  }
  NOTREACHED();
  return NULL;
}

// An invariant has been violated.  Transition to an error state where we try
// to do as little work as possible, to avoid further corruption or crashes.
void ProfileSyncService::OnUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  unrecoverable_error_detected_ = true;
  unrecoverable_error_message_ = message;
  unrecoverable_error_location_.reset(
      new tracked_objects::Location(from_here.function_name(),
                                    from_here.file_name(),
                                    from_here.line_number(),
                                    from_here.program_counter()));

  // Tell the wizard so it can inform the user only if it is already open.
  wizard_.Step(SyncSetupWizard::FATAL_ERROR);

  NotifyObservers();
  LOG(ERROR) << "Unrecoverable error detected -- ProfileSyncService unusable."
      << message;
  std::string location;
  from_here.Write(true, true, &location);
  LOG(ERROR) << location;

  // Shut all data types down.
  MessageLoop::current()->PostTask(FROM_HERE,
        scoped_runnable_method_factory_.NewRunnableMethod(
        &ProfileSyncService::Shutdown, true));
}

void ProfileSyncService::OnBackendInitialized() {
  backend_initialized_ = true;

  js_event_handlers_.SetBackend(backend_->GetJsBackend());

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  last_synced_time_ will only be null
  // in this case, because the pref wasn't restored on StartUp.
  if (last_synced_time_.is_null()) {
    UpdateLastSyncedTime();
  }
  NotifyObservers();

  if (!cros_user_.empty()) {
    if (profile_->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart)) {
      ShowConfigure(true);
    } else {
      SetSyncSetupCompleted();
    }
  }

  if (HasSyncSetupCompleted()) {
    ConfigureDataTypeManager();
  }
}

void ProfileSyncService::OnSyncCycleCompleted() {
  UpdateLastSyncedTime();
  VLOG(2) << "Notifying observers sync cycle completed";
  NotifyObservers();
}

void ProfileSyncService::UpdateAuthErrorState(
    const GoogleServiceAuthError& error) {
  last_auth_error_ = error;
  // Protect against the in-your-face dialogs that pop out of nowhere.
  // Require the user to click somewhere to run the setup wizard in the case
  // of a steady-state auth failure.
  if (WizardIsVisible()) {
    wizard_.Step(AuthError::NONE == last_auth_error_.state() ?
        SyncSetupWizard::GAIA_SUCCESS : SyncSetupWizard::GAIA_LOGIN);
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

  passphrase_required_reason_ = reason;

  // We will skip the passphrase prompt and suppress the warning if the
  // passphrase is needed for decryption but the user is not syncing an
  // encrypted data type on this machine. Otherwise we look for one.
  if (!IsEncryptedDatatypeEnabled() && IsPassphraseRequiredForDecryption()) {
    OnPassphraseAccepted();
  }

  // First try supplying gaia password as the passphrase.
  if (!gaia_password_.empty()) {
    SetPassphrase(gaia_password_, false, true);
    gaia_password_ = std::string();
    return;
  }

  // If the above failed then try the custom passphrase the user might have
  // entered in setup.
  if (!cached_passphrase_.value.empty()) {
    SetPassphrase(cached_passphrase_.value,
                  cached_passphrase_.is_explicit,
                  cached_passphrase_.is_creation);
    cached_passphrase_ = CachedPassphrase();
    return;
  }

  // Prompt the user for a password.
  if (WizardIsVisible() && IsEncryptedDatatypeEnabled() &&
      IsPassphraseRequiredForDecryption()) {
    wizard_.Step(SyncSetupWizard::ENTER_PASSPHRASE);
  }

  NotifyObservers();
}

void ProfileSyncService::OnPassphraseAccepted() {
  // Make sure the data types that depend on the passphrase are started at
  // this time.
  syncable::ModelTypeSet types;
  GetPreferredDataTypes(&types);

  // Reset passphrase_required_reason_ before configuring the DataTypeManager
  // since we know we no longer require the passphrase.
  passphrase_required_reason_ = sync_api::REASON_PASSPHRASE_NOT_REQUIRED;

  if (data_type_manager_.get())
    data_type_manager_->Configure(types,
                                  sync_api::CONFIGURE_REASON_RECONFIGURATION);

  NotifyObservers();

  wizard_.Step(SyncSetupWizard::DONE);
}

void ProfileSyncService::OnEncryptionComplete(
    const syncable::ModelTypeSet& encrypted_types) {
  if (encrypted_types_.current != encrypted_types) {
    encrypted_types_.current = encrypted_types;
    NotifyObservers();
  }
}

void ProfileSyncService::OnMigrationNeededForTypes(
    const syncable::ModelTypeSet& types) {
  DCHECK(backend_initialized_);
  DCHECK(data_type_manager_.get());

  // Migrator must be valid, because we don't sync until it is created and this
  // callback originates from a sync cycle.
  migrator_->MigrateTypes(types);
}

void ProfileSyncService::ShowLoginDialog() {
  if (WizardIsVisible()) {
    wizard_.Focus();
    // Force the wizard to step to the login screen (which will only actually
    // happen if the transition is valid).
    wizard_.Step(SyncSetupWizard::GAIA_LOGIN);
    return;
  }

  if (!auth_error_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Sync.ReauthorizationTime",
                             base::TimeTicks::Now() - auth_error_time_);
    auth_error_time_ = base::TimeTicks();  // Reset auth_error_time_ to null.
  }

  wizard_.Step(SyncSetupWizard::GAIA_LOGIN);

  NotifyObservers();
}

void ProfileSyncService::ShowErrorUI() {
  if (IsPassphraseRequired()) {
    if (IsUsingSecondaryPassphrase())
      PromptForExistingPassphrase();
    else
      NOTREACHED();  // Migration no longer supported.

    return;
  }

  const GoogleServiceAuthError& error = GetAuthError();
  if (error.state() == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
      error.state() == GoogleServiceAuthError::CAPTCHA_REQUIRED ||
      error.state() == GoogleServiceAuthError::ACCOUNT_DELETED ||
      error.state() == GoogleServiceAuthError::ACCOUNT_DISABLED ||
      error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE) {
    ShowLoginDialog();
  }
}


void ProfileSyncService::ShowConfigure(bool sync_everything) {
  if (WizardIsVisible()) {
    wizard_.Focus();
    return;
  }

  if (sync_everything)
    wizard_.Step(SyncSetupWizard::SYNC_EVERYTHING);
  else
    wizard_.Step(SyncSetupWizard::CONFIGURE);
}

void ProfileSyncService::PromptForExistingPassphrase() {
  if (WizardIsVisible()) {
    wizard_.Focus();
    return;
  }

  wizard_.Step(SyncSetupWizard::ENTER_PASSPHRASE);
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
    SyncBackendHost::Status status =
        { SyncBackendHost::Status::OFFLINE_UNUSABLE };
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

  if (!signin_.get()) {
    // In ChromeOS we sign in during login, so we do not instantiate signin_.
    // If this function gets called, we need to re-authenticate (e.g. for
    // two factor signin), so instantiate signin_ here.
    signin_.reset(new SigninManager());
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

void ProfileSyncService::OnUserChoseDatatypes(bool sync_everything,
    const syncable::ModelTypeSet& chosen_types) {
  if (!backend_.get()) {
    NOTREACHED();
    return;
  }

  profile_->GetPrefs()->SetBoolean(prefs::kKeepEverythingSynced,
      sync_everything);

  ChangePreferredDataTypes(chosen_types);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();
}

void ProfileSyncService::OnUserCancelledDialog() {
  if (!HasSyncSetupCompleted()) {
    // A sync dialog was aborted before authentication.
    // Rollback.
    expect_sync_configuration_aborted_ = true;
    DisableForUser();
  }

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
    ConfigureDataTypeManager();
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
  return backend_.get() && (backend_->IsUsingExplicitPassphrase() ||
      (tried_implicit_gaia_remove_when_bug_62103_fixed_ &&
       IsPassphraseRequired()));
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
  if (!data_type_manager_.get()) {
    data_type_manager_.reset(
        factory_->CreateDataTypeManager(backend_.get(),
                                        data_type_controllers_));
    registrar_.Add(this,
                   NotificationType::SYNC_CONFIGURE_START,
                   Source<DataTypeManager>(data_type_manager_.get()));
    registrar_.Add(this,
                   NotificationType::SYNC_CONFIGURE_DONE,
                   Source<DataTypeManager>(data_type_manager_.get()));

    // We create the migrator at the same time.
    migrator_.reset(
        new browser_sync::BackendMigrator(this, data_type_manager_.get()));
  }

  syncable::ModelTypeSet types;
  GetPreferredDataTypes(&types);
  if (IsPassphraseRequiredForDecryption()) {
    if (IsEncryptedDatatypeEnabled()) {
      // We need a passphrase still. Prompt the user for a passphrase, and
      // DataTypeManager::Configure() will get called once the passphrase is
      // accepted.
      VLOG(0) << "ProfileSyncService::ConfigureDataTypeManager bailing out "
              << "because a passphrase required";
      OnPassphraseRequired(passphrase_required_reason_);
      return;
    } else {
      // We've been informed that a passphrase is required for decryption, but
      // now there are no encrypted data types enabled, so change the value of
      // passphrase_required_reason_ to its default value. (NotifyObservers()
      // will be called when configuration completes).
      passphrase_required_reason_ = sync_api::REASON_PASSPHRASE_NOT_REQUIRED;
    }
  }
  data_type_manager_->Configure(
      types, sync_api::CONFIGURE_REASON_RECONFIGURATION);
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

void ProfileSyncService::LogUnsyncedItems(int level) const {
  if (backend_.get() && backend_initialized_) {
    return backend_->LogUnsyncedItems(level);
  }
  VLOG(level) << "Unsynced items not printed because backend is not "
              << "initialized";
}

bool ProfileSyncService::HasPendingBackendMigration() const {
  return migrator_.get() &&
      migrator_->state() != browser_sync::BackendMigrator::IDLE;
}

void ProfileSyncService::GetModelSafeRoutingInfo(
    browser_sync::ModelSafeRoutingInfo* out) {
  if (backend_.get() && backend_initialized_) {
    backend_->GetModelSafeRoutingInfo(out);
  } else {
    NOTREACHED();
  }
}

syncable::AutofillMigrationState
    ProfileSyncService::GetAutofillMigrationState() {
  if (backend_.get() && backend_initialized_) {
    return backend_->GetAutofillMigrationState();
  }
  NOTREACHED();
  return syncable::NOT_DETERMINED;
}

void ProfileSyncService::SetAutofillMigrationState(
    syncable::AutofillMigrationState state) {
  if (backend_.get() && backend_initialized_) {
    backend_->SetAutofillMigrationState(state);
  } else {
    NOTREACHED();
  }
}

syncable::AutofillMigrationDebugInfo
    ProfileSyncService::GetAutofillMigrationDebugInfo() {
  if (backend_.get() && backend_initialized_) {
    return backend_->GetAutofillMigrationDebugInfo();
  }
  NOTREACHED();
  syncable::AutofillMigrationDebugInfo debug_info = { 0 };
  return debug_info;
}

void ProfileSyncService::SetAutofillMigrationDebugInfo(
    syncable::AutofillMigrationDebugInfo::PropertyToSet property_to_set,
    const syncable::AutofillMigrationDebugInfo& info) {
  if (backend_.get() && backend_initialized_) {
    backend_->SetAutofillMigrationDebugInfo(property_to_set, info);
  } else {
    NOTREACHED();
  }
}

void ProfileSyncService::ActivateDataType(
    DataTypeController* data_type_controller,
    ChangeProcessor* change_processor) {
  if (!backend_.get()) {
    NOTREACHED();
    return;
  }
  DCHECK(backend_initialized_);
  backend_->ActivateDataType(data_type_controller, change_processor);
}

void ProfileSyncService::DeactivateDataType(
    DataTypeController* data_type_controller,
    ChangeProcessor* change_processor) {
  if (!backend_.get())
    return;
  backend_->DeactivateDataType(data_type_controller, change_processor);
}

void ProfileSyncService::SetPassphrase(const std::string& passphrase,
                                       bool is_explicit,
                                       bool is_creation) {
  if (ShouldPushChanges() || IsPassphraseRequired()) {
    backend_->SetPassphrase(passphrase, is_explicit);
  } else {
    if (is_explicit) {
      cached_passphrase_.value = passphrase;
      cached_passphrase_.is_explicit = is_explicit;
      cached_passphrase_.is_creation = is_creation;
    } else {
      gaia_password_ = passphrase;
    }
  }
}

void ProfileSyncService::EncryptDataTypes(
    const syncable::ModelTypeSet& encrypted_types) {
  if (HasSyncSetupCompleted()) {
    backend_->EncryptDataTypes(encrypted_types);
    encrypted_types_.pending.clear();
  } else {
    encrypted_types_.pending = encrypted_types;
  }
}

void ProfileSyncService::GetEncryptedDataTypes(
    syncable::ModelTypeSet* encrypted_types) const {
  *encrypted_types = encrypted_types_.current;
  DCHECK(encrypted_types->count(syncable::PASSWORDS));
}

void ProfileSyncService::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::SYNC_CONFIGURE_START: {
      NotifyObservers();
      // TODO(sync): Maybe toast?
      break;
    }
    case NotificationType::SYNC_CONFIGURE_DONE: {
      DataTypeManager::ConfigureResultWithErrorLocation* result_with_location =
          Details<DataTypeManager::ConfigureResultWithErrorLocation>(
              details).ptr();

      DataTypeManager::ConfigureResult result = result_with_location->result;
      VLOG(1) << "PSS SYNC_CONFIGURE_DONE called with result: " << result;
      if (result == DataTypeManager::ABORTED &&
          expect_sync_configuration_aborted_) {
        VLOG(0) << "ProfileSyncService::Observe Sync Configure aborted";
        expect_sync_configuration_aborted_ = false;
        return;
      }
      // Clear out the gaia password if it is already there.
      gaia_password_ = std::string();
      if (result != DataTypeManager::OK) {
        VLOG(0) << "ProfileSyncService::Observe: Unrecoverable error detected";
        std::string message = StringPrintf("Sync Configuration failed with %d",
                                            result);
        OnUnrecoverableError(*(result_with_location->location), message);
        cached_passphrase_ = CachedPassphrase();
        return;
      }

      // If the user had entered a custom passphrase use it now.
      if (!cached_passphrase_.value.empty()) {
        // Don't hold on to the passphrase in raw form longer than needed.
        SetPassphrase(cached_passphrase_.value,
                      cached_passphrase_.is_explicit,
                      cached_passphrase_.is_creation);
        cached_passphrase_ = CachedPassphrase();
      }

      // We should never get in a state where we have no encrypted datatypes
      // enabled, and yet we still think we require a passphrase for decryption.
      DCHECK(!(IsPassphraseRequiredForDecryption() &&
               !IsEncryptedDatatypeEnabled()));

      // TODO(sync): Less wizard, more toast.
      wizard_.Step(SyncSetupWizard::DONE);
      NotifyObservers();

      // In the old world, this would be a no-op.  With new syncer thread,
      // this is the point where it is safe to switch from config-mode to
      // normal operation.
      backend_->StartSyncingWithServer();

      if (!encrypted_types_.pending.empty())
        EncryptDataTypes(encrypted_types_.pending);
      break;
    }
    case NotificationType::PREF_CHANGED: {
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
    case NotificationType::GOOGLE_SIGNIN_SUCCESSFUL: {
      const GoogleServiceSigninSuccessDetails* successful =
          (Details<const GoogleServiceSigninSuccessDetails>(details).ptr());
      // We pass 'false' to SetPassphrase to denote that this is an implicit
      // request and shouldn't override an explicit one.  Thus, we either
      // update the implicit passphrase (idempotent if the passphrase didn't
      // actually change), or the user has an explicit passphrase set so this
      // becomes a no-op.
      tried_implicit_gaia_remove_when_bug_62103_fixed_ = true;
      SetPassphrase(successful->password, false, true);
      break;
    }
    case NotificationType::GOOGLE_SIGNIN_FAILED: {
      GoogleServiceAuthError error =
          *(Details<const GoogleServiceAuthError>(details).ptr());
      UpdateAuthErrorState(error);
      break;
    }
    case NotificationType::TOKEN_AVAILABLE: {
      if (AreCredentialsAvailable()) {
        if (backend_initialized_) {
          backend_->UpdateCredentials(GetCredentials());
        }

        if (!profile_->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart))
          StartUp();
      }
      break;
    }
    case NotificationType::TOKEN_LOADING_FINISHED: {
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

browser_sync::JsFrontend* ProfileSyncService::GetJsFrontend() {
  return &js_event_handlers_;
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

ProfileSyncService::EncryptedTypes::EncryptedTypes() {
  current.insert(syncable::PASSWORDS);
}

ProfileSyncService::EncryptedTypes::~EncryptedTypes() {}
