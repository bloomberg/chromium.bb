// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"
#include "net/base/cookie_monster.h"
#include "views/window/window.h"

using browser_sync::ChangeProcessor;
using browser_sync::DataTypeController;
using browser_sync::SyncBackendHost;

typedef GoogleServiceAuthError AuthError;

// Default sync server URL.
static const char kSyncServerUrl[] = "https://clients4.google.com/chrome-sync";

ProfileSyncService::ProfileSyncService(Profile* profile)
    : last_auth_error_(AuthError::None()),
      profile_(profile),
      sync_service_url_(kSyncServerUrl),
      backend_initialized_(false),
      expecting_first_run_auth_needed_event_(false),
      is_auth_in_progress_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(wizard_(this)),
      unrecoverable_error_detected_(false) {
}

ProfileSyncService::~ProfileSyncService() {
  Shutdown(false);
  STLDeleteValues(&data_type_controllers_);
}

void ProfileSyncService::Initialize() {
  InitSettings();
  RegisterPreferences();

  if (!profile()->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted)) {
    DisableForUser();  // Clean up in case of previous crash / setup abort.
    // If the LSID is empty, we're in a UI test that is not testing sync
    // behavior, so we don't want the sync service to start.
    // profile()->GetRequestContext() also checks if this is in a test.
    if (browser_defaults::kBootstrapSyncAuthentication &&
        profile()->GetRequestContext() &&
        !GetLsidForAuthBootstraping().empty())
      StartUp();  // We always start sync for Chrome OS.
  } else {
    StartUp();
  }
}

void ProfileSyncService::RegisterDataTypeController(
    DataTypeController* data_type_controller) {
  DCHECK(data_type_controllers_.count(data_type_controller->type()) == 0);
  data_type_controllers_[data_type_controller->type()] =
      data_type_controller;
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
  pref_service->RegisterInt64Pref(prefs::kSyncLastSyncedTime, 0);
  pref_service->RegisterBooleanPref(prefs::kSyncHasSetupCompleted, false);
}

void ProfileSyncService::ClearPreferences() {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->ClearPref(prefs::kSyncLastSyncedTime);
  pref_service->ClearPref(prefs::kSyncHasSetupCompleted);

  pref_service->ScheduleSavePersistentPrefs();
}

// The domain and name of the LSID cookie which we use to bootstrap the sync
// authentication in Chrome OS.
const char kLsidCookieDomain[] = "www.google.com";
const char kLsidCookieName[]   = "LSID";

std::string ProfileSyncService::GetLsidForAuthBootstraping() {
  if (browser_defaults::kBootstrapSyncAuthentication) {
    // If we're running inside Chrome OS, bootstrap the sync authentication by
    // using the LSID cookie provided by the Chrome OS login manager.
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
#if !defined(NDEBUG)
  invalidate_sync_login = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kInvalidateSyncLogin);
#endif
  backend_->Initialize(sync_service_url_, profile_->GetRequestContext(),
                       GetLsidForAuthBootstraping(), delete_sync_data_folder,
                       invalidate_sync_login);
}

void ProfileSyncService::StartUp() {
  // Don't start up multiple times.
  if (backend_.get())
    return;

  last_synced_time_ = base::Time::FromInternalValue(
      profile_->GetPrefs()->GetInt64(prefs::kSyncLastSyncedTime));

  backend_.reset(new SyncBackendHost(this, profile_->GetPath()));

  // Initialize the backend.  Every time we start up a new SyncBackendHost,
  // we'll want to start from a fresh SyncDB, so delete any old one that might
  // be there.
  InitializeBackend(!HasSyncSetupCompleted());
}

void ProfileSyncService::Shutdown(bool sync_disabled) {
  if (backend_.get())
    backend_->Shutdown(sync_disabled);

  // Stop all data type controllers.
  // TODO(skrul): Change this to support multiple data type controllers.
  if (data_type_controllers_.count(syncable::BOOKMARKS))
    data_type_controllers_[syncable::BOOKMARKS]->Stop();

  backend_.reset();

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

  // TODO(skrul): Change this to support multiple data type controllers.
  data_type_controllers_[syncable::BOOKMARKS]->Stop();

  // Tell the wizard so it can inform the user only if it is already open.
  wizard_.Step(SyncSetupWizard::FATAL_ERROR);
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
  LOG(ERROR) << "Unrecoverable error detected -- ProfileSyncService unusable.";
}

void ProfileSyncService::OnBackendInitialized() {
  backend_initialized_ = true;
  StartProcessingChangesIfReady();

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  last_synced_time_ will only be null
  // in this case, because the pref wasn't restored on StartUp.
  if (last_synced_time_.is_null())
    UpdateLastSyncedTime();
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
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

void ProfileSyncService::OnUserSubmittedAuth(
    const std::string& username, const std::string& password,
    const std::string& captcha) {
  last_attempted_user_email_ = username;
  is_auth_in_progress_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());

  auth_start_time_ = base::TimeTicks::Now();
  backend_->Authenticate(username, password, captcha);
}

void ProfileSyncService::OnUserAcceptedMergeAndSync() {
  // Start each data type with "merge_allowed" to correspond to the
  // user's acceptance of merge.
  // TODO(skrul): Change this to support multiple data type controllers.
  data_type_controllers_[syncable::BOOKMARKS]->Start(
      true,
      NewCallback(this, &ProfileSyncService::BookmarkStartCallback));
}

void ProfileSyncService::OnUserCancelledDialog() {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted)) {
    // A sync dialog was aborted before authentication or merge acceptance.
    // Rollback.
    DisableForUser();
  }

  FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
}

void ProfileSyncService::StartProcessingChangesIfReady() {
  DCHECK(backend_initialized_);

  // If the user has completed sync setup, we are always allowed to
  // merge data.
  bool merge_allowed =
      profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted);

  // If we're running inside Chrome OS, always allow merges and
  // consider the sync setup complete.
  if (browser_defaults::kBootstrapSyncAuthentication) {
    merge_allowed = true;
    SetSyncSetupCompleted();
  }

  // Start data types.
  // TODO(skrul): Change this to support multiple data type controllers.
  data_type_controllers_[syncable::BOOKMARKS]->Start(
      merge_allowed,
      NewCallback(this, &ProfileSyncService::BookmarkStartCallback));
}

void ProfileSyncService::BookmarkStartCallback(
    DataTypeController::StartResult result) {
  switch (result) {
    case DataTypeController::OK:
    case DataTypeController::OK_FIRST_RUN: {
      wizard_.Step(result == DataTypeController::OK ? SyncSetupWizard::DONE :
                   SyncSetupWizard::DONE_FIRST_TIME);
      FOR_EACH_OBSERVER(Observer, observers_, OnStateChanged());
      break;
    }

    case DataTypeController::NEEDS_MERGE: {
      ProfileSyncService::SyncEvent(MERGE_AND_SYNC_NEEDED);
      wizard_.Step(SyncSetupWizard::MERGE_AND_SYNC);
      break;
    }

    default: {
      LOG(ERROR) << "Bookmark start failed";
      OnUnrecoverableError();
    }
  }
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
  // True only after all bootstrapping has succeeded: the bookmark model is
  // loaded, the sync backend is initialized, the two domains are
  // consistent with one another, and no unrecoverable error has transpired.
  if (data_type_controllers_.count(syncable::BOOKMARKS)) {
    return data_type_controllers_[syncable::BOOKMARKS]->state() ==
        DataTypeController::RUNNING;
  }
  return false;
}
