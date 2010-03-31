// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service.h"

#include <map>
#include <set>

#include "app/l10n_util.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#if defined(OS_WIN)
#include "chrome/browser/views/options/customize_sync_window_view.h"
#endif
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"
#include "net/base/cookie_monster.h"

using browser_sync::ChangeProcessor;
using browser_sync::DataTypeController;
using browser_sync::DataTypeManager;
using browser_sync::SyncBackendHost;

typedef GoogleServiceAuthError AuthError;

// Default sync server URL.
static const char kSyncServerUrl[] = "https://clients4.google.com/chrome-sync";

ProfileSyncService::ProfileSyncService(ProfileSyncFactory* factory,
                                       Profile* profile,
                                       bool bootstrap_sync_authentication)
    : last_auth_error_(AuthError::None()),
      factory_(factory),
      profile_(profile),
      bootstrap_sync_authentication_(bootstrap_sync_authentication),
      sync_service_url_(kSyncServerUrl),
      backend_initialized_(false),
      expecting_first_run_auth_needed_event_(false),
      is_auth_in_progress_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(wizard_(this)),
      unrecoverable_error_detected_(false),
      notification_method_(browser_sync::kDefaultNotificationMethod) {
  registrar_.Add(this,
                 NotificationType::SYNC_CONFIGURE_START,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 NotificationType::SYNC_CONFIGURE_DONE,
                 NotificationService::AllSources());
}

ProfileSyncService::~ProfileSyncService() {
  Shutdown(false);
}

void ProfileSyncService::Initialize() {
  InitSettings();
  RegisterPreferences();

  if (!profile()->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted)) {
    DisableForUser();  // Clean up in case of previous crash / setup abort.
    // If the LSID is empty, we're in a UI test that is not testing sync
    // behavior, so we don't want the sync service to start.
    if (bootstrap_sync_authentication_ &&
        !profile()->GetPrefs()->GetBoolean(prefs::kSyncBootstrappedAuth) &&
        !GetLsidForAuthBootstraping().empty()) {
      // If we're under Chromium OS and have never bootstrapped the
      // authentication (ie. this is the first time we start sync for this
      // profile,) then bootstrap it.
      StartUp();
      profile()->GetPrefs()->SetBoolean(prefs::kSyncBootstrappedAuth, true);
    }
  } else {
    StartUp();
  }
}

void ProfileSyncService::RegisterDataTypeController(
    DataTypeController* data_type_controller) {
  DCHECK_EQ(data_type_controllers_.count(data_type_controller->type()), 0U);
  data_type_controllers_[data_type_controller->type()] =
      data_type_controller;
}

void ProfileSyncService::GetDataTypeControllerStates(
  browser_sync::DataTypeController::StateMap* state_map) const {
    browser_sync::DataTypeController::TypeMap::const_iterator iter
        = data_type_controllers_.begin();
    for ( ; iter != data_type_controllers_.end(); ++iter ) {
      (*state_map)[iter->first] = iter->second.get()->state();
    }
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

  if (command_line.HasSwitch(switches::kSyncNotificationMethod)) {
    const std::string notification_method_str(
        command_line.GetSwitchValueASCII(switches::kSyncNotificationMethod));
    notification_method_ =
        browser_sync::StringToNotificationMethod(notification_method_str);
  }
}

void ProfileSyncService::RegisterPreferences() {
  PrefService* pref_service = profile_->GetPrefs();
  if (pref_service->FindPreference(prefs::kSyncLastSyncedTime))
    return;
  pref_service->RegisterInt64Pref(prefs::kSyncLastSyncedTime, 0);
  pref_service->RegisterBooleanPref(prefs::kSyncHasSetupCompleted, false);

  // If you've never synced before, all datatypes are on by default.
  pref_service->RegisterBooleanPref(prefs::kSyncBookmarks, true);
  pref_service->RegisterBooleanPref(prefs::kSyncPreferences, true);
  pref_service->RegisterBooleanPref(prefs::kSyncAutofill, true);

  // TODO(albertb): Consider getting rid of this preference once we have a UI
  // for per-data type disabling.
  if (bootstrap_sync_authentication_ &&
      !pref_service->FindPreference(prefs::kSyncBootstrappedAuth))
    pref_service->RegisterBooleanPref(prefs::kSyncBootstrappedAuth, false);
}

void ProfileSyncService::ClearPreferences() {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->ClearPref(prefs::kSyncLastSyncedTime);
  pref_service->ClearPref(prefs::kSyncHasSetupCompleted);
  pref_service->ScheduleSavePersistentPrefs();
}

// The domain and name of the LSID cookie which we use to bootstrap the sync
// authentication in Chromium OS.
const char kLsidCookieDomain[] = "www.google.com";
const char kLsidCookieName[]   = "LSID";

std::string ProfileSyncService::GetLsidForAuthBootstraping() {
  if (bootstrap_sync_authentication_ && profile()->GetRequestContext()) {
    // If we're running inside Chromium OS, bootstrap the sync authentication by
    // using the LSID cookie provided by the Chromium OS login manager.
    net::CookieMonster::CookieList cookies = profile()->GetRequestContext()->
        GetCookieStore()->GetCookieMonster()->GetAllCookies();
    for (net::CookieMonster::CookieList::const_iterator it = cookies.begin();
         it != cookies.end(); ++it) {
      if (kLsidCookieDomain == it->first) {
        const net::CookieMonster::CanonicalCookie& cookie = it->second;
        if (kLsidCookieName == cookie.Name()) {
          return cookie.Value();
        }
      }
    }
  }
  return std::string();
}

void ProfileSyncService::InitializeBackend(bool delete_sync_data_folder) {
  bool invalidate_sync_login = false;
  bool invalidate_sync_xmpp_login = false;
#if !defined(NDEBUG)
  invalidate_sync_login = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kInvalidateSyncLogin);
  invalidate_sync_xmpp_login = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kInvalidateSyncXmppLogin);
#endif
  backend_->Initialize(sync_service_url_, profile_->GetRequestContext(),
                       GetLsidForAuthBootstraping(), delete_sync_data_folder,
                       invalidate_sync_login,
                       invalidate_sync_xmpp_login,
                       notification_method_);
}

void ProfileSyncService::StartUp() {
  // Don't start up multiple times.
  if (backend_.get())
    return;

  last_synced_time_ = base::Time::FromInternalValue(
      profile_->GetPrefs()->GetInt64(prefs::kSyncLastSyncedTime));

  backend_.reset(
      new SyncBackendHost(this, profile_, profile_->GetPath(),
                          data_type_controllers_));

  // Initialize the backend.  Every time we start up a new SyncBackendHost,
  // we'll want to start from a fresh SyncDB, so delete any old one that might
  // be there.
  InitializeBackend(!HasSyncSetupCompleted());
}

void ProfileSyncService::Shutdown(bool sync_disabled) {
  if (backend_.get())
    backend_->Shutdown(sync_disabled);

  // Stop all data type controllers, if needed.
  if (data_type_manager_.get() &&
      data_type_manager_->state() != DataTypeManager::STOPPED) {
    data_type_manager_->Stop();
  }

  backend_.reset();
  data_type_manager_.reset();

  // Clear various flags.
  is_auth_in_progress_ = false;
  backend_initialized_ = false;
  expecting_first_run_auth_needed_event_ = false;
  last_attempted_user_email_.clear();
}

void ProfileSyncService::EnableForUser() {
  if (WizardIsVisible()) {
    // TODO(timsteele): Focus wizard.
    return;
  }
  expecting_first_run_auth_needed_event_ = true;

  StartUp();
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::DisableForUser() {
  if (WizardIsVisible()) {
    // TODO(timsteele): Focus wizard.
    return;
  }
  Shutdown(true);
  ClearPreferences();

  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

bool ProfileSyncService::HasSyncSetupCompleted() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted);
}

void ProfileSyncService::SetSyncSetupCompleted() {
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kSyncHasSetupCompleted, true);
  prefs->ScheduleSavePersistentPrefs();
}

void ProfileSyncService::UpdateLastSyncedTime() {
  last_synced_time_ = base::Time::Now();
  profile_->GetPrefs()->SetInt64(prefs::kSyncLastSyncedTime,
      last_synced_time_.ToInternalValue());
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();
}

// An invariant has been violated.  Transition to an error state where we try
// to do as little work as possible, to avoid further corruption or crashes.
void ProfileSyncService::OnUnrecoverableError() {
  unrecoverable_error_detected_ = true;

  // Shut all data types down.
  if (data_type_manager_.get())
    data_type_manager_->Stop();

  // Tell the wizard so it can inform the user only if it is already open.
  wizard_.Step(SyncSetupWizard::FATAL_ERROR);
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
  LOG(ERROR) << "Unrecoverable error detected -- ProfileSyncService unusable.";
}

void ProfileSyncService::OnBackendInitialized() {
  backend_initialized_ = true;

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  last_synced_time_ will only be null
  // in this case, because the pref wasn't restored on StartUp.
  if (last_synced_time_.is_null())
    UpdateLastSyncedTime();
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());

  StartProcessingChangesIfReady();
}

void ProfileSyncService::OnSyncCycleCompleted() {
  UpdateLastSyncedTime();
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::OnAuthError() {
  last_auth_error_ = backend_->GetAuthError();
  // Protect against the in-your-face dialogs that pop out of nowhere.
  // Require the user to click somewhere to run the setup wizard in the case
  // of a steady-state auth failure.
  if (WizardIsVisible() || expecting_first_run_auth_needed_event_) {
    wizard_.Step(AuthError::NONE == last_auth_error_.state() ?
        SyncSetupWizard::GAIA_SUCCESS : SyncSetupWizard::GAIA_LOGIN);
  }

  if (expecting_first_run_auth_needed_event_) {
    last_auth_error_ = AuthError::None();
    expecting_first_run_auth_needed_event_ = false;
  }

  if (!WizardIsVisible()) {
    auth_error_time_ == base::TimeTicks::Now();
  }

  if (!auth_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("Sync.AuthorizationTimeInNetwork",
                    base::TimeTicks::Now() - auth_start_time_);
    auth_start_time_ = base::TimeTicks();
  }

  is_auth_in_progress_ = false;
  // Fan the notification out to interested UI-thread components.
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::ShowLoginDialog() {
  if (WizardIsVisible())
    return;

  if (!auth_error_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Sync.ReauthorizationTime",
                             base::TimeTicks::Now() - auth_error_time_);
    auth_error_time_ = base::TimeTicks();  // Reset auth_error_time_ to null.
  }

  if (last_auth_error_.state() != AuthError::NONE) {
    wizard_.Step(SyncSetupWizard::GAIA_LOGIN);
  }
}

SyncBackendHost::StatusSummary ProfileSyncService::QuerySyncStatusSummary() {
  return backend_->GetStatusSummary();
}

SyncBackendHost::Status ProfileSyncService::QueryDetailedSyncStatus() {
  return backend_->GetDetailedStatus();
}

std::wstring ProfileSyncService::BuildSyncStatusSummaryText(
  const sync_api::SyncManager::Status::Summary& summary) {
  switch (summary) {
    case sync_api::SyncManager::Status::OFFLINE:
      return L"OFFLINE";
    case sync_api::SyncManager::Status::OFFLINE_UNSYNCED:
      return L"OFFLINE_UNSYNCED";
    case sync_api::SyncManager::Status::SYNCING:
      return L"SYNCING";
    case sync_api::SyncManager::Status::READY:
      return L"READY";
    case sync_api::SyncManager::Status::CONFLICT:
      return L"CONFLICT";
    case sync_api::SyncManager::Status::OFFLINE_UNUSABLE:
      return L"OFFLINE_UNUSABLE";
    case sync_api::SyncManager::Status::INVALID:  // fall through
    default:
      return L"UNKNOWN";
  }
}

std::wstring ProfileSyncService::GetLastSyncedTimeString() const {
  if (last_synced_time_.is_null())
    return l10n_util::GetString(IDS_SYNC_TIME_NEVER);

  base::TimeDelta last_synced = base::Time::Now() - last_synced_time_;

  if (last_synced < base::TimeDelta::FromMinutes(1))
    return l10n_util::GetString(IDS_SYNC_TIME_JUST_NOW);

  return TimeFormat::TimeElapsed(last_synced);
}

string16 ProfileSyncService::GetAuthenticatedUsername() const {
  return backend_->GetAuthenticatedUsername();
}

void ProfileSyncService::OnUserClickedCustomize() {
  // This is coming from the gaia_login, so set configure_on_accept=false
  // (because when the user accepts, he/she will not have signed in yet).
#if defined(OS_WIN)
  CustomizeSyncWindowView::Show(NULL, profile_, false);
#endif
}

void ProfileSyncService::OnUserSubmittedAuth(
    const std::string& username, const std::string& password,
    const std::string& captcha) {
  last_attempted_user_email_ = username;
  is_auth_in_progress_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());

  auth_start_time_ = base::TimeTicks::Now();
  backend_->Authenticate(username, password, captcha);
}

void ProfileSyncService::OnUserCancelledDialog() {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted)) {
    // A sync dialog was aborted before authentication.
    // Rollback.
    DisableForUser();
  }

  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::ChangeDataTypes(
  const browser_sync::DataTypeManager::TypeSet& desired_types) {
  data_type_manager_->Configure(desired_types);
}

void ProfileSyncService::StartProcessingChangesIfReady() {
  DCHECK(backend_initialized_);

  if (bootstrap_sync_authentication_) {
    SetSyncSetupCompleted();
  }

  std::set<syncable::ModelType> types;
  for (DataTypeController::TypeMap::const_iterator it =
           data_type_controllers_.begin();
       it != data_type_controllers_.end(); ++it) {
    types.insert((*it).first);
  }

  data_type_manager_.reset(
      factory_->CreateDataTypeManager(backend_.get(), data_type_controllers_));
  data_type_manager_->Configure(types);
}

void ProfileSyncService::ActivateDataType(
    DataTypeController* data_type_controller,
    ChangeProcessor* change_processor) {
  change_processor->Start(profile(), backend_->GetUserShareHandle());
  backend_->ActivateDataType(data_type_controller, change_processor);
}

void ProfileSyncService::DeactivateDataType(
    DataTypeController* data_type_controller,
    ChangeProcessor* change_processor) {
  change_processor->Stop();
  backend_->DeactivateDataType(data_type_controller, change_processor);
}

void ProfileSyncService::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::SYNC_CONFIGURE_START: {
      // TODO(sync): Maybe toast?
      break;
    }
    case NotificationType::SYNC_CONFIGURE_DONE: {
      DataTypeManager::ConfigureResult result =
          *(Details<DataTypeManager::ConfigureResult>(details).ptr());
      if (result != DataTypeManager::OK) {
        OnUnrecoverableError();
        return;
      }

      // TODO(sync): Less wizard, more toast.
      wizard_.Step(SyncSetupWizard::DONE);
      FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
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

void ProfileSyncService::SyncEvent(SyncEventCodes code) {
  UMA_HISTOGRAM_ENUMERATION("Sync.EventCodes", code, MAX_SYNC_EVENT_CODE);
}

bool ProfileSyncService::IsSyncEnabled() {
  // We have switches::kEnableSync just in case we need to change back to
  // sync-disabled-by-default on a platform.
  return !CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableSync);
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
