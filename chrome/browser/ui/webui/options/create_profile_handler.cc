// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/create_profile_handler.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/supervised_user/supervised_user_registration_utility.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/options/options_handlers_helper.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace options {

CreateProfileHandler::CreateProfileHandler()
    : profile_creation_type_(NO_CREATION_IN_PROGRESS),
      weak_ptr_factory_(this) {
}

CreateProfileHandler::~CreateProfileHandler() {
  CancelProfileRegistration(false);
}

void CreateProfileHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
}

void CreateProfileHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "cancelCreateProfile",
      base::Bind(&CreateProfileHandler::HandleCancelProfileCreation,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "createProfile",
      base::Bind(&CreateProfileHandler::CreateProfile,
                 base::Unretained(this)));
}

void CreateProfileHandler::CreateProfile(const base::ListValue* args) {
  // This handler could have been called for a supervised user, for example
  // because the user fiddled with the web inspector. Silently return in this
  // case.
  Profile* current_profile = Profile::FromWebUI(web_ui());
  if (current_profile->IsSupervised())
    return;

  if (!profiles::IsMultipleProfilesEnabled())
    return;

  // We can have only one in progress profile creation
  // at any given moment, if new ones are initiated just
  // ignore them until we are done with the old one.
  if (profile_creation_type_ != NO_CREATION_IN_PROGRESS)
    return;

  profile_creation_type_ = NON_SUPERVISED_PROFILE_CREATION;

  DCHECK(profile_path_being_created_.empty());
  profile_creation_start_time_ = base::TimeTicks::Now();

  base::string16 name;
  base::string16 icon;
  std::string supervised_user_id;
  bool create_shortcut = false;
  bool supervised_user = false;
  if (args->GetString(0, &name) && args->GetString(1, &icon)) {
    if (args->GetBoolean(2, &create_shortcut)) {
      bool success = args->GetBoolean(3, &supervised_user);
      DCHECK(success);
      success = args->GetString(4, &supervised_user_id);
      DCHECK(success);
    }
  }

  if (supervised_user) {
    if (!IsValidExistingSupervisedUserId(supervised_user_id))
      return;

    profile_creation_type_ = SUPERVISED_PROFILE_IMPORT;
    if (supervised_user_id.empty()) {
      profile_creation_type_ = SUPERVISED_PROFILE_CREATION;
      supervised_user_id =
          SupervisedUserRegistrationUtility::GenerateNewSupervisedUserId();

      // If sync is not yet fully initialized, the creation may take extra time,
      // so show a message. Import doesn't wait for an acknowledgement, so it
      // won't have the same potential delay.
      ProfileSyncService* sync_service =
          ProfileSyncServiceFactory::GetInstance()->GetForProfile(
              current_profile);
      ProfileSyncService::SyncStatusSummary status =
          sync_service->QuerySyncStatusSummary();
      if (status == ProfileSyncService::DATATYPES_NOT_INITIALIZED) {
        ShowProfileCreationWarning(l10n_util::GetStringUTF16(
            IDS_PROFILES_CREATE_SUPERVISED_JUST_SIGNED_IN));
      }
    }
  }

  ProfileMetrics::LogProfileAddNewUser(ProfileMetrics::ADD_NEW_USER_DIALOG);

  profile_path_being_created_ = ProfileManager::CreateMultiProfileAsync(
      name, icon,
      base::Bind(&CreateProfileHandler::OnProfileCreated,
                 weak_ptr_factory_.GetWeakPtr(),
                 create_shortcut,
                 helper::GetDesktopType(web_ui()),
                 supervised_user_id),
      supervised_user_id);
}

void CreateProfileHandler::OnProfileCreated(
    bool create_shortcut,
    chrome::HostDesktopType desktop_type,
    const std::string& supervised_user_id,
    Profile* profile,
    Profile::CreateStatus status) {
  if (status != Profile::CREATE_STATUS_CREATED)
    RecordProfileCreationMetrics(status);

  switch (status) {
    case Profile::CREATE_STATUS_LOCAL_FAIL: {
      ShowProfileCreationError(profile,
                               GetProfileCreationErrorMessage(LOCAL_ERROR));
      break;
    }
    case Profile::CREATE_STATUS_CREATED: {
      // Do nothing for an intermediate status.
      break;
    }
    case Profile::CREATE_STATUS_INITIALIZED: {
      HandleProfileCreationSuccess(create_shortcut, desktop_type,
                                   supervised_user_id, profile);
      break;
    }
    // User-initiated cancellation is handled in CancelProfileRegistration and
    // does not call this callback.
    case Profile::CREATE_STATUS_CANCELED:
    // Supervised user registration errors are handled in
    // OnSupervisedUserRegistered().
    case Profile::CREATE_STATUS_REMOTE_FAIL:
    case Profile::MAX_CREATE_STATUS: {
      NOTREACHED();
      break;
    }
  }
}

void CreateProfileHandler::HandleProfileCreationSuccess(
    bool create_shortcut,
    chrome::HostDesktopType desktop_type,
    const std::string& supervised_user_id,
    Profile* profile) {
  switch (profile_creation_type_) {
    case NON_SUPERVISED_PROFILE_CREATION: {
      DCHECK(supervised_user_id.empty());
      CreateShortcutAndShowSuccess(create_shortcut, desktop_type, profile);
      break;
    }
    case SUPERVISED_PROFILE_CREATION:
    case SUPERVISED_PROFILE_IMPORT:
      RegisterSupervisedUser(create_shortcut, desktop_type,
                             supervised_user_id, profile);
      break;
    case NO_CREATION_IN_PROGRESS:
      NOTREACHED();
      break;
  }
}

void CreateProfileHandler::RegisterSupervisedUser(
    bool create_shortcut,
    chrome::HostDesktopType desktop_type,
    const std::string& supervised_user_id,
    Profile* new_profile) {
  DCHECK_EQ(profile_path_being_created_.value(),
            new_profile->GetPath().value());

  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(new_profile);

  // Register the supervised user using the profile of the custodian.
  supervised_user_registration_utility_ =
      SupervisedUserRegistrationUtility::Create(Profile::FromWebUI(web_ui()));
  supervised_user_service->RegisterAndInitSync(
      supervised_user_registration_utility_.get(),
      Profile::FromWebUI(web_ui()),
      supervised_user_id,
      base::Bind(&CreateProfileHandler::OnSupervisedUserRegistered,
                 weak_ptr_factory_.GetWeakPtr(),
                 create_shortcut,
                 desktop_type,
                 new_profile));
}

void CreateProfileHandler::OnSupervisedUserRegistered(
    bool create_shortcut,
    chrome::HostDesktopType desktop_type,
    Profile* profile,
    const GoogleServiceAuthError& error) {
  GoogleServiceAuthError::State state = error.state();
  RecordSupervisedProfileCreationMetrics(state);
  if (state == GoogleServiceAuthError::NONE) {
    CreateShortcutAndShowSuccess(create_shortcut, desktop_type, profile);
    return;
  }

  base::string16 error_msg;
  if (state == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
      state == GoogleServiceAuthError::USER_NOT_SIGNED_UP ||
      state == GoogleServiceAuthError::ACCOUNT_DELETED ||
      state == GoogleServiceAuthError::ACCOUNT_DISABLED) {
    error_msg = GetProfileCreationErrorMessage(SIGNIN_ERROR);
  } else {
    error_msg = GetProfileCreationErrorMessage(REMOTE_ERROR);
  }
  ShowProfileCreationError(profile, error_msg);
}

void CreateProfileHandler::CreateShortcutAndShowSuccess(
    bool create_shortcut,
    chrome::HostDesktopType desktop_type,
    Profile* profile) {
  if (create_shortcut) {
    ProfileShortcutManager* shortcut_manager =
        g_browser_process->profile_manager()->profile_shortcut_manager();

    if (shortcut_manager)
      shortcut_manager->CreateProfileShortcut(profile->GetPath());
  }

  DCHECK_EQ(profile_path_being_created_.value(), profile->GetPath().value());
  profile_path_being_created_.clear();
  DCHECK_NE(NO_CREATION_IN_PROGRESS, profile_creation_type_);
  base::DictionaryValue dict;
  dict.SetString("name",
                 profile->GetPrefs()->GetString(prefs::kProfileName));
  dict.Set("filePath", base::CreateFilePathValue(profile->GetPath()));
  bool is_supervised =
      profile_creation_type_ == SUPERVISED_PROFILE_CREATION ||
      profile_creation_type_ == SUPERVISED_PROFILE_IMPORT;
  dict.SetBoolean("isManaged", is_supervised);
  web_ui()->CallJavascriptFunction(
      GetJavascriptMethodName(PROFILE_CREATION_SUCCESS), dict);

  // If the new profile is a supervised user, instead of opening a new window
  // right away, a confirmation overlay will be shown by JS from the creation
  // dialog. If we are importing an existing supervised profile or creating a
  // new non-supervised user profile we don't show any confirmation, so open
  // the new window now.
  if (profile_creation_type_ != SUPERVISED_PROFILE_CREATION) {
    // Opening the new window must be the last action, after all callbacks
    // have been run, to give them a chance to initialize the profile.
    helper::OpenNewWindowForProfile(desktop_type,
                                    profile,
                                    Profile::CREATE_STATUS_INITIALIZED);
  }
  profile_creation_type_ = NO_CREATION_IN_PROGRESS;
}

void CreateProfileHandler::ShowProfileCreationError(
    Profile* profile,
    const base::string16& error) {
  DCHECK_NE(NO_CREATION_IN_PROGRESS, profile_creation_type_);
  profile_creation_type_ = NO_CREATION_IN_PROGRESS;
  profile_path_being_created_.clear();
  web_ui()->CallJavascriptFunction(
      GetJavascriptMethodName(PROFILE_CREATION_ERROR),
      base::StringValue(error));
  // The ProfileManager calls us back with a NULL profile in some cases.
  if (profile)
    helper::DeleteProfileAtPath(profile->GetPath(), web_ui());
}

void CreateProfileHandler::ShowProfileCreationWarning(
    const base::string16& warning) {
  DCHECK_EQ(SUPERVISED_PROFILE_CREATION, profile_creation_type_);
  web_ui()->CallJavascriptFunction("BrowserOptions.showCreateProfileWarning",
                                   base::StringValue(warning));
}

void CreateProfileHandler::HandleCancelProfileCreation(
    const base::ListValue* args) {
  CancelProfileRegistration(true);
}

void CreateProfileHandler::CancelProfileRegistration(bool user_initiated) {
  if (profile_path_being_created_.empty())
    return;

  ProfileManager* manager = g_browser_process->profile_manager();
  Profile* new_profile = manager->GetProfileByPath(profile_path_being_created_);
  if (!new_profile)
    return;

  // Non-supervised user creation cannot be canceled. (Creating a non-supervised
  // profile shouldn't take significant time, and it can easily be deleted
  // afterward.)
  if (!new_profile->IsSupervised())
    return;

  if (user_initiated) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Profile.CreateTimeCanceledNoTimeout",
        base::TimeTicks::Now() - profile_creation_start_time_);
    RecordProfileCreationMetrics(Profile::CREATE_STATUS_CANCELED);
  }

  DCHECK(supervised_user_registration_utility_.get());
  supervised_user_registration_utility_.reset();

  DCHECK_NE(NO_CREATION_IN_PROGRESS, profile_creation_type_);
  profile_creation_type_ = NO_CREATION_IN_PROGRESS;

  // Cancelling registration means the callback passed into
  // RegisterAndInitSync() won't be called, so the cleanup must be done here.
  profile_path_being_created_.clear();
  helper::DeleteProfileAtPath(new_profile->GetPath(), web_ui());
}

void CreateProfileHandler::RecordProfileCreationMetrics(
    Profile::CreateStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Profile.CreateResult",
                            status,
                            Profile::MAX_CREATE_STATUS);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "Profile.CreateTimeNoTimeout",
      base::TimeTicks::Now() - profile_creation_start_time_);
}

void CreateProfileHandler::RecordSupervisedProfileCreationMetrics(
    GoogleServiceAuthError::State error_state) {
  if (profile_creation_type_ == SUPERVISED_PROFILE_CREATION) {
    UMA_HISTOGRAM_ENUMERATION("Profile.SupervisedProfileCreateError",
                              error_state,
                              GoogleServiceAuthError::NUM_STATES);
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Profile.SupervisedProfileTotalCreateTime",
        base::TimeTicks::Now() - profile_creation_start_time_);
  } else {
    DCHECK_EQ(SUPERVISED_PROFILE_IMPORT, profile_creation_type_);
    UMA_HISTOGRAM_ENUMERATION("Profile.SupervisedProfileImportError",
                              error_state,
                              GoogleServiceAuthError::NUM_STATES);
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Profile.SupervisedProfileTotalImportTime",
        base::TimeTicks::Now() - profile_creation_start_time_);
  }
}

base::string16 CreateProfileHandler::GetProfileCreationErrorMessage(
    ProfileCreationErrorType error) const {
  int message_id = -1;
  switch (error) {
    case SIGNIN_ERROR:
      message_id =
          profile_creation_type_ == SUPERVISED_PROFILE_IMPORT ?
              IDS_SUPERVISED_USER_IMPORT_SIGN_IN_ERROR :
              IDS_PROFILES_CREATE_SIGN_IN_ERROR;
      break;
    case REMOTE_ERROR:
      message_id =
          profile_creation_type_ == SUPERVISED_PROFILE_IMPORT ?
              IDS_SUPERVISED_USER_IMPORT_REMOTE_ERROR :
              IDS_PROFILES_CREATE_REMOTE_ERROR;
      break;
    case LOCAL_ERROR:
      message_id =
          profile_creation_type_ == SUPERVISED_PROFILE_IMPORT ?
              IDS_SUPERVISED_USER_IMPORT_LOCAL_ERROR :
              IDS_PROFILES_CREATE_LOCAL_ERROR;
      break;
  }

  return l10n_util::GetStringUTF16(message_id);
}

std::string CreateProfileHandler::GetJavascriptMethodName(
    ProfileCreationStatus status) const {
  switch (status) {
    case PROFILE_CREATION_SUCCESS:
      return profile_creation_type_ == SUPERVISED_PROFILE_IMPORT ?
          "BrowserOptions.showManagedUserImportSuccess" :
          "BrowserOptions.showCreateProfileSuccess";
    case PROFILE_CREATION_ERROR:
      return profile_creation_type_ == SUPERVISED_PROFILE_IMPORT ?
          "BrowserOptions.showManagedUserImportError" :
          "BrowserOptions.showCreateProfileError";
  }

  NOTREACHED();
  return std::string();
}

bool CreateProfileHandler::IsValidExistingSupervisedUserId(
    const std::string& existing_supervised_user_id) const {
  if (existing_supervised_user_id.empty())
    return true;

  Profile* profile = Profile::FromWebUI(web_ui());
  const base::DictionaryValue* dict =
      SupervisedUserSyncServiceFactory::GetForProfile(profile)->
          GetSupervisedUsers();
  if (!dict->HasKey(existing_supervised_user_id))
    return false;

  // Check if this supervised user already exists on this machine.
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i) {
    if (existing_supervised_user_id ==
            cache.GetSupervisedUserIdOfProfileAtIndex(i))
      return false;
  }
  return true;
}

}  // namespace options
