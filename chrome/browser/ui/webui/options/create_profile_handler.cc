// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/create_profile_handler.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/legacy/supervised_user_registration_utility.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

namespace options {

CreateProfileHandler::CreateProfileHandler()
    : profile_creation_type_(NO_CREATION_IN_PROGRESS),
      weak_ptr_factory_(this) {
}

CreateProfileHandler::~CreateProfileHandler() {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Cancellation is only supported for supervised users.
  CancelProfileRegistration(false);
#endif
}

void CreateProfileHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
}

void CreateProfileHandler::RegisterMessages() {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Cancellation is only supported for supervised users.
  web_ui()->RegisterMessageCallback(
      "cancelCreateProfile",
      base::Bind(&CreateProfileHandler::HandleCancelProfileCreation,
                 base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback(
      "createProfile",
      base::Bind(&CreateProfileHandler::CreateProfile,
                 base::Unretained(this)));
}

void CreateProfileHandler::CreateProfile(const base::ListValue* args) {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // This handler could have been called for a supervised user, for example
  // because the user fiddled with the web inspector. Silently return.
  if (Profile::FromWebUI(web_ui())->IsSupervised())
    return;
#endif

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
  std::string icon_url;
  bool create_shortcut = false;
  if (args->GetString(0, &name) && args->GetString(1, &icon_url)) {
    DCHECK(base::IsStringASCII(icon_url));
    base::TrimWhitespace(name, base::TRIM_ALL, &name);
    CHECK(!name.empty());
#ifndef NDEBUG
    size_t icon_index;
    DCHECK(profiles::IsDefaultAvatarIconUrl(icon_url, &icon_index));
#endif
    args->GetBoolean(2, &create_shortcut);
  }
  std::string supervised_user_id;
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (!ProcessSupervisedCreateProfileArgs(args, &supervised_user_id))
    return;
#endif

  ProfileMetrics::LogProfileAddNewUser(ProfileMetrics::ADD_NEW_USER_DIALOG);

  profile_path_being_created_ = ProfileManager::CreateMultiProfileAsync(
      name, icon_url, base::Bind(&CreateProfileHandler::OnProfileCreated,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 create_shortcut, supervised_user_id),
      supervised_user_id);
}

void CreateProfileHandler::OnProfileCreated(
    bool create_shortcut,
    const std::string& supervised_user_id,
    Profile* profile,
    Profile::CreateStatus status) {
  if (status != Profile::CREATE_STATUS_CREATED)
    RecordProfileCreationMetrics(status);

  switch (status) {
    case Profile::CREATE_STATUS_LOCAL_FAIL: {
      ShowProfileCreationError(profile, GetProfileCreationErrorMessageLocal());
      break;
    }
    case Profile::CREATE_STATUS_CREATED: {
      // Do nothing for an intermediate status.
      break;
    }
    case Profile::CREATE_STATUS_INITIALIZED: {
      HandleProfileCreationSuccess(create_shortcut, supervised_user_id,
                                   profile);
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
    const std::string& supervised_user_id,
    Profile* profile) {
  switch (profile_creation_type_) {
    case NON_SUPERVISED_PROFILE_CREATION: {
      DCHECK(supervised_user_id.empty());
      CreateShortcutAndShowSuccess(create_shortcut, profile);
      break;
    }
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    case SUPERVISED_PROFILE_CREATION:
    case SUPERVISED_PROFILE_IMPORT:
      RegisterSupervisedUser(create_shortcut, supervised_user_id, profile);
      break;
#endif
    case NO_CREATION_IN_PROGRESS:
      NOTREACHED();
      break;
  }
}

void CreateProfileHandler::CreateShortcutAndShowSuccess(bool create_shortcut,
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
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  bool is_supervised =
      profile_creation_type_ == SUPERVISED_PROFILE_CREATION ||
      profile_creation_type_ == SUPERVISED_PROFILE_IMPORT;
  dict.SetBoolean("isSupervised", is_supervised);
#endif
  web_ui()->CallJavascriptFunctionUnsafe(
      GetJavascriptMethodName(PROFILE_CREATION_SUCCESS), dict);

  // If the new profile is a supervised user, instead of opening a new window
  // right away, a confirmation overlay will be shown by JS from the creation
  // dialog. If we are importing an existing supervised profile or creating a
  // new non-supervised user profile we don't show any confirmation, so open
  // the new window now.
  bool should_open_new_window = true;
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (profile_creation_type_ == SUPERVISED_PROFILE_CREATION)
    should_open_new_window = false;
#endif

  if (should_open_new_window) {
    // Opening the new window must be the last action, after all callbacks
    // have been run, to give them a chance to initialize the profile.
    webui::OpenNewWindowForProfile(profile);
  }
  profile_creation_type_ = NO_CREATION_IN_PROGRESS;
}

void CreateProfileHandler::ShowProfileCreationError(
    Profile* profile,
    const base::string16& error) {
  DCHECK_NE(NO_CREATION_IN_PROGRESS, profile_creation_type_);
  profile_creation_type_ = NO_CREATION_IN_PROGRESS;
  profile_path_being_created_.clear();
  web_ui()->CallJavascriptFunctionUnsafe(
      GetJavascriptMethodName(PROFILE_CREATION_ERROR),
      base::StringValue(error));
  // The ProfileManager calls us back with a NULL profile in some cases.
  if (profile) {
    webui::DeleteProfileAtPath(profile->GetPath(),
                               web_ui(),
                               ProfileMetrics::DELETE_PROFILE_SETTINGS);
  }
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

base::string16 CreateProfileHandler::GetProfileCreationErrorMessageLocal()
    const {
  int message_id = IDS_PROFILES_CREATE_LOCAL_ERROR;
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Local errors can occur during supervised profile import.
  if (profile_creation_type_ == SUPERVISED_PROFILE_IMPORT)
    message_id = IDS_LEGACY_SUPERVISED_USER_IMPORT_LOCAL_ERROR;
#endif
  return l10n_util::GetStringUTF16(message_id);
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
base::string16 CreateProfileHandler::GetProfileCreationErrorMessageRemote()
    const {
  return l10n_util::GetStringUTF16(
      profile_creation_type_ == SUPERVISED_PROFILE_IMPORT ?
          IDS_LEGACY_SUPERVISED_USER_IMPORT_REMOTE_ERROR :
          IDS_PROFILES_CREATE_REMOTE_ERROR);
}

base::string16 CreateProfileHandler::GetProfileCreationErrorMessageSignin()
    const {
  return l10n_util::GetStringUTF16(
      profile_creation_type_ == SUPERVISED_PROFILE_IMPORT ?
          IDS_LEGACY_SUPERVISED_USER_IMPORT_SIGN_IN_ERROR :
          IDS_PROFILES_CREATE_SIGN_IN_ERROR);
}
#endif

std::string CreateProfileHandler::GetJavascriptMethodName(
    ProfileCreationStatus status) const {
  switch (profile_creation_type_) {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    case SUPERVISED_PROFILE_IMPORT:
      switch (status) {
        case PROFILE_CREATION_SUCCESS:
          return "BrowserOptions.showSupervisedUserImportSuccess";
        case PROFILE_CREATION_ERROR:
          return "BrowserOptions.showSupervisedUserImportError";
      }
      break;
#endif
    default:
      switch (status) {
        case PROFILE_CREATION_SUCCESS:
          return "BrowserOptions.showCreateProfileSuccess";
        case PROFILE_CREATION_ERROR:
          return "BrowserOptions.showCreateProfileError";
      }
      break;
  }

  NOTREACHED();
  return std::string();
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
bool CreateProfileHandler::ProcessSupervisedCreateProfileArgs(
    const base::ListValue* args, std::string* supervised_user_id) {
  bool supervised_user = false;
  if (args->GetSize() >= 5) {
      bool success = args->GetBoolean(3, &supervised_user);
      DCHECK(success);

      success = args->GetString(4, supervised_user_id);
      DCHECK(success);
  }

  if (supervised_user) {
    if (!IsValidExistingSupervisedUserId(*supervised_user_id))
      return false;

    profile_creation_type_ = SUPERVISED_PROFILE_IMPORT;
    if (supervised_user_id->empty()) {
      profile_creation_type_ = SUPERVISED_PROFILE_CREATION;
      *supervised_user_id =
          SupervisedUserRegistrationUtility::GenerateNewSupervisedUserId();

      // If sync is not yet fully initialized, the creation may take extra time,
      // so show a message. Import doesn't wait for an acknowledgment, so it
      // won't have the same potential delay.
      browser_sync::ProfileSyncService* sync_service =
          ProfileSyncServiceFactory::GetInstance()->GetForProfile(
              Profile::FromWebUI(web_ui()));
      browser_sync::ProfileSyncService::SyncStatusSummary status =
          sync_service->QuerySyncStatusSummary();
      if (status ==
          browser_sync::ProfileSyncService::DATATYPES_NOT_INITIALIZED) {
        ShowProfileCreationWarning(l10n_util::GetStringUTF16(
            IDS_PROFILES_CREATE_SUPERVISED_JUST_SIGNED_IN));
      }
    }
  }
  return true;
}

void CreateProfileHandler::HandleCancelProfileCreation(
    const base::ListValue* args) {
  CancelProfileRegistration(true);
}

// Non-supervised user creation cannot be canceled. (Creating a non-supervised
// profile shouldn't take significant time, and it can easily be deleted
// afterward.)
void CreateProfileHandler::CancelProfileRegistration(bool user_initiated) {
  if (profile_path_being_created_.empty())
    return;

  ProfileManager* manager = g_browser_process->profile_manager();
  Profile* new_profile = manager->GetProfileByPath(profile_path_being_created_);
  if (!new_profile || !new_profile->IsSupervised())
    return;

  DCHECK(supervised_user_registration_utility_.get());
  supervised_user_registration_utility_.reset();

  if (user_initiated) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Profile.CreateTimeCanceledNoTimeout",
        base::TimeTicks::Now() - profile_creation_start_time_);
    RecordProfileCreationMetrics(Profile::CREATE_STATUS_CANCELED);
  }

  DCHECK_NE(NO_CREATION_IN_PROGRESS, profile_creation_type_);
  profile_creation_type_ = NO_CREATION_IN_PROGRESS;

  // Cancelling registration means the callback passed into
  // RegisterAndInitSync() won't be called, so the cleanup must be done here.
  profile_path_being_created_.clear();
  webui::DeleteProfileAtPath(new_profile->GetPath(),
                             web_ui(),
                             ProfileMetrics::DELETE_PROFILE_SETTINGS);
}

void CreateProfileHandler::RegisterSupervisedUser(
    bool create_shortcut,
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
                 new_profile));
}

void CreateProfileHandler::OnSupervisedUserRegistered(
    bool create_shortcut,
    Profile* profile,
    const GoogleServiceAuthError& error) {
  GoogleServiceAuthError::State state = error.state();
  RecordSupervisedProfileCreationMetrics(state);
  if (state == GoogleServiceAuthError::NONE) {
    CreateShortcutAndShowSuccess(create_shortcut, profile);
    return;
  }

  base::string16 error_msg;
  if (state == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
      state == GoogleServiceAuthError::USER_NOT_SIGNED_UP ||
      state == GoogleServiceAuthError::ACCOUNT_DELETED ||
      state == GoogleServiceAuthError::ACCOUNT_DISABLED) {
    error_msg = GetProfileCreationErrorMessageSignin();
  } else {
    error_msg = GetProfileCreationErrorMessageRemote();
  }
  ShowProfileCreationError(profile, error_msg);
}

void CreateProfileHandler::ShowProfileCreationWarning(
    const base::string16& warning) {
  DCHECK_EQ(SUPERVISED_PROFILE_CREATION, profile_creation_type_);
  web_ui()->CallJavascriptFunctionUnsafe(
      "BrowserOptions.showCreateProfileWarning", base::StringValue(warning));
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
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()->
      GetProfileAttributesStorage().GetAllProfilesAttributes();
  for (const ProfileAttributesEntry* entry : entries) {
    if (existing_supervised_user_id == entry->GetSupervisedUserId())
      return false;
  }
  return true;
}
#endif

}  // namespace options
