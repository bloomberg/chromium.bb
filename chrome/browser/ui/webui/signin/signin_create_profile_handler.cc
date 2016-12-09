// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_create_profile_handler.h"

#include <stddef.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/legacy/supervised_user_registration_utility.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

SigninCreateProfileHandler::SigninCreateProfileHandler()
    : profile_creation_type_(NO_CREATION_IN_PROGRESS),
      weak_ptr_factory_(this) {
  g_browser_process->profile_manager()->
      GetProfileAttributesStorage().AddObserver(this);
}

SigninCreateProfileHandler::~SigninCreateProfileHandler() {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Cancellation is only supported for supervised users.
  CancelProfileRegistration(false);
#endif
  g_browser_process->profile_manager()->
    GetProfileAttributesStorage().RemoveObserver(this);
}

void SigninCreateProfileHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString(
      "createDesktopShortcutLabel",
      l10n_util::GetStringUTF16(
          IDS_PROFILES_CREATE_DESKTOP_SHORTCUT_LABEL));
  localized_strings->SetString(
      "manageProfilesSupervisedSignedInLabel",
      l10n_util::GetStringUTF16(
          IDS_PROFILES_CREATE_SUPERVISED_MULTI_SIGNED_IN_LABEL));
  localized_strings->SetString(
      "noSignedInUserMessage",
      l10n_util::GetStringUTF16(
          IDS_PROFILES_CREATE_SUPERVISED_NO_SIGNED_IN_USER_TEXT));
  localized_strings->SetString("createProfileConfirm",
                               l10n_util::GetStringUTF16(IDS_SAVE));
  localized_strings->SetString("learnMore",
                               l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  localized_strings->SetString(
      "selectAnAccount",
      l10n_util::GetStringUTF16(
          IDS_PROFILES_CREATE_SUPERVISED_SENTINEL_MENU_ITEM_TEXT));
  localized_strings->SetString(
      "createProfileTitle",
      l10n_util::GetStringUTF16(IDS_PROFILES_CREATE_TITLE));
  localized_strings->SetString(
      "supervisedUserLearnMoreTitle",
      l10n_util::GetStringUTF16(IDS_LEGACY_SUPERVISED_USER_LEARN_MORE_TITLE));
  localized_strings->SetString(
      "supervisedUserLearnMoreDone",
      l10n_util::GetStringUTF16(
          IDS_LEGACY_SUPERVISED_USER_LEARN_MORE_DONE_BUTTON));
  localized_strings->SetString(
      "supervisedUserLearnMoreText",
      l10n_util::GetStringFUTF16(
          IDS_SUPERVISED_USER_LEARN_MORE_TEXT,
          base::ASCIIToUTF16(
              chrome::kLegacySupervisedUserManagementURL),
          base::ASCIIToUTF16(
              chrome::kLegacySupervisedUserManagementDisplayURL)));
  localized_strings->SetString(
      "importExistingSupervisedUserLink",
      l10n_util::GetStringUTF16(
          IDS_IMPORT_EXISTING_LEGACY_SUPERVISED_USER_TITLE));
  localized_strings->SetString(
      "manageProfilesExistingSupervisedUser",
      l10n_util::GetStringUTF16(
          IDS_PROFILES_CREATE_LEGACY_SUPERVISED_USER_ERROR_EXISTS_REMOTELY));
  localized_strings->SetString(
      "managedProfilesExistingLocalSupervisedUser",
      l10n_util::GetStringUTF16(
          IDS_PROFILES_CREATE_LEGACY_SUPERVISED_USER_ERROR_EXISTS_LOCALLY));
  localized_strings->SetString(
      "custodianAccountNotSelectedError",
      l10n_util::GetStringUTF16(
          IDS_PROFILES_CREATE_NO_CUSTODIAN_ACCOUNT_ERROR));
  localized_strings->SetString(
      "supervisedUserCreatedTitle",
       l10n_util::GetStringUTF16(IDS_LEGACY_SUPERVISED_USER_CREATED_TITLE));
  // The first two substitution parameters remain to be filled by the page JS.
  localized_strings->SetString(
      "supervisedUserCreatedText",
      l10n_util::GetStringFUTF16(
            IDS_SUPERVISED_USER_CREATED_TEXT,
            base::ASCIIToUTF16("$1"),
            base::ASCIIToUTF16("$2"),
            base::ASCIIToUTF16(chrome::kLegacySupervisedUserManagementURL),
            base::ASCIIToUTF16(
                chrome::kLegacySupervisedUserManagementDisplayURL)));
  localized_strings->SetString(
      "exitAndChildlockLabel",
      l10n_util::GetStringUTF16(
          IDS_PROFILES_PROFILE_SIGNOUT_BUTTON));
  localized_strings->SetString(
      "supervisedUserCreatedDone",
      l10n_util::GetStringUTF16(
          IDS_LEGACY_SUPERVISED_USER_CREATED_DONE_BUTTON));
  localized_strings->SetString(
      "supervisedUserCreatedSwitch",
      l10n_util::GetStringUTF16(
          IDS_LEGACY_SUPERVISED_USER_CREATED_SWITCH_BUTTON));
}

void SigninCreateProfileHandler::RegisterMessages() {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Cancellation is only supported for supervised users.
  web_ui()->RegisterMessageCallback(
      "cancelCreateProfile",
      base::Bind(&SigninCreateProfileHandler::HandleCancelProfileCreation,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "switchToProfile",
      base::Bind(&SigninCreateProfileHandler::SwitchToProfile,
                 base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback(
      "createProfile", base::Bind(&SigninCreateProfileHandler::CreateProfile,
                                  base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "requestDefaultProfileIcons",
      base::Bind(&SigninCreateProfileHandler::RequestDefaultProfileIcons,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "requestSignedInProfiles",
      base::Bind(&SigninCreateProfileHandler::RequestSignedInProfiles,
                 base::Unretained(this)));
}

void SigninCreateProfileHandler::RequestDefaultProfileIcons(
    const base::ListValue* args) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "cr.webUIListenerCallback", base::StringValue("profile-icons-received"),
      *profiles::GetDefaultProfileAvatarIconsAndLabels());

  SendNewProfileDefaults();
}

void SigninCreateProfileHandler::SendNewProfileDefaults() {
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  base::DictionaryValue profile_info;
  profile_info.SetString("name", storage.ChooseNameForNewProfile(0));

  web_ui()->CallJavascriptFunctionUnsafe(
      "cr.webUIListenerCallback",
      base::StringValue("profile-defaults-received"), profile_info);
}

void SigninCreateProfileHandler::RequestSignedInProfiles(
    const base::ListValue* args) {
  base::ListValue user_info_list;
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()->
          GetProfileAttributesStorage().GetAllProfilesAttributesSortedByName();
  for (ProfileAttributesEntry* entry : entries) {
    base::string16 username = entry->GetUserName();
    if (username.empty())
      continue;
    base::string16 profile_path = entry->GetPath().AsUTF16Unsafe();
    std::unique_ptr<base::DictionaryValue> user_info(
        new base::DictionaryValue());
    user_info->SetString("username", username);
    user_info->SetString("profilePath", profile_path);

    user_info_list.Append(std::move(user_info));
  }
  web_ui()->CallJavascriptFunctionUnsafe(
      "cr.webUIListenerCallback", base::StringValue("signedin-users-received"),
      user_info_list);
}

void SigninCreateProfileHandler::OnProfileAuthInfoChanged(
    const base::FilePath& profile_path) {
  RequestSignedInProfiles(nullptr);
}

void SigninCreateProfileHandler::CreateProfile(const base::ListValue* args) {
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
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  std::string supervised_user_id;
  base::FilePath custodian_profile_path;
  if (GetSupervisedCreateProfileArgs(args, &supervised_user_id,
                                     &custodian_profile_path)) {
    // Load custodian profile.
    g_browser_process->profile_manager()->CreateProfileAsync(
        custodian_profile_path,
        base::Bind(&SigninCreateProfileHandler::LoadCustodianProfileCallback,
                   weak_ptr_factory_.GetWeakPtr(), name, icon_url,
                   create_shortcut, supervised_user_id),
        base::string16(), std::string(), std::string());
  } else {
    DoCreateProfile(name, icon_url, create_shortcut, std::string(), nullptr);
  }
#else
  DoCreateProfile(name, icon_url, create_shortcut, std::string(), nullptr);
#endif
}

void SigninCreateProfileHandler::DoCreateProfile(
    const base::string16& name,
    const std::string& icon_url,
    bool create_shortcut,
    const std::string& supervised_user_id,
    Profile* custodian_profile) {
  ProfileMetrics::LogProfileAddNewUser(ProfileMetrics::ADD_NEW_USER_DIALOG);

  profile_path_being_created_ = ProfileManager::CreateMultiProfileAsync(
      name, icon_url,
      base::Bind(&SigninCreateProfileHandler::OnProfileCreated,
                 weak_ptr_factory_.GetWeakPtr(), create_shortcut,
                 supervised_user_id, custodian_profile),
      supervised_user_id);
}

void SigninCreateProfileHandler::OnProfileCreated(
    bool create_shortcut,
    const std::string& supervised_user_id,
    Profile* custodian_profile,
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
                                   custodian_profile, profile);
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

void SigninCreateProfileHandler::HandleProfileCreationSuccess(
    bool create_shortcut,
    const std::string& supervised_user_id,
    Profile* custodian_profile,
    Profile* profile) {
  switch (profile_creation_type_) {
    case NON_SUPERVISED_PROFILE_CREATION: {
      DCHECK(supervised_user_id.empty());
      CreateShortcutAndShowSuccess(create_shortcut, nullptr, profile);
      break;
    }
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    case SUPERVISED_PROFILE_CREATION:
    case SUPERVISED_PROFILE_IMPORT:
      RegisterSupervisedUser(create_shortcut, supervised_user_id,
                             custodian_profile, profile);
      break;
#endif
    case NO_CREATION_IN_PROGRESS:
      NOTREACHED();
      break;
  }
}

void SigninCreateProfileHandler::CreateShortcutAndShowSuccess(
    bool create_shortcut,
    Profile* custodian_profile,
    Profile* profile) {
  if (create_shortcut) {
    DCHECK(ProfileShortcutManager::IsFeatureEnabled());
    ProfileShortcutManager* shortcut_manager =
        g_browser_process->profile_manager()->profile_shortcut_manager();
    DCHECK(shortcut_manager);
    if (shortcut_manager)
      shortcut_manager->CreateProfileShortcut(profile->GetPath());
  }

  DCHECK_EQ(profile_path_being_created_.value(), profile->GetPath().value());
  profile_path_being_created_.clear();
  DCHECK_NE(NO_CREATION_IN_PROGRESS, profile_creation_type_);
  base::DictionaryValue dict;
  dict.SetString("name", profile->GetPrefs()->GetString(prefs::kProfileName));
  dict.Set("filePath", base::CreateFilePathValue(profile->GetPath()));

  bool is_force_signin_enabled = signin::IsForceSigninEnabled();
  bool open_new_window = !is_force_signin_enabled;

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // If the new profile is a supervised user, instead of opening a new window
  // right away, a confirmation page will be shown by JS from the creation
  // dialog. If we are importing an existing supervised profile or creating a
  // new non-supervised user profile we don't show any confirmation, so open
  // the new window now.

  open_new_window =
      open_new_window && profile_creation_type_ != SUPERVISED_PROFILE_CREATION;

  dict.SetBoolean("showConfirmation",
                  profile_creation_type_ == SUPERVISED_PROFILE_CREATION);

  bool is_supervised = profile_creation_type_ == SUPERVISED_PROFILE_CREATION ||
                       profile_creation_type_ == SUPERVISED_PROFILE_IMPORT;
  dict.SetBoolean("isSupervised", is_supervised);

  if (is_supervised) {
    DCHECK(custodian_profile);
    if (custodian_profile) {
      std::string custodian_username = custodian_profile->GetProfileUserName();
      dict.SetString("custodianUsername", custodian_username);
    }
  }
#endif

  web_ui()->CallJavascriptFunctionUnsafe(
      "cr.webUIListenerCallback",
      GetWebUIListenerName(PROFILE_CREATION_SUCCESS), dict);

  if (open_new_window) {
    // Opening the new window must be the last action, after all callbacks
    // have been run, to give them a chance to initialize the profile.
    OpenNewWindowForProfile(profile, Profile::CREATE_STATUS_INITIALIZED);
  } else if (is_force_signin_enabled) {
    OpenSigninDialogForProfile(profile);
  }
  profile_creation_type_ = NO_CREATION_IN_PROGRESS;
}

void SigninCreateProfileHandler::OpenNewWindowForProfile(
    Profile* profile,
    Profile::CreateStatus status) {
  profiles::OpenBrowserWindowForProfile(
      base::Bind(&SigninCreateProfileHandler::OnBrowserReadyCallback,
                 weak_ptr_factory_.GetWeakPtr()),
      false,  // Don't create a window if one already exists.
      true,  // Create a first run window.
      profile,
      status);
}

void SigninCreateProfileHandler::OpenSigninDialogForProfile(Profile* profile) {
  UserManagerProfileDialog::ShowSigninDialog(
      web_ui()->GetWebContents()->GetBrowserContext(), profile->GetPath());
}

void SigninCreateProfileHandler::ShowProfileCreationError(
    Profile* profile,
    const base::string16& error) {
  DCHECK_NE(NO_CREATION_IN_PROGRESS, profile_creation_type_);
  web_ui()->CallJavascriptFunctionUnsafe(
      "cr.webUIListenerCallback", GetWebUIListenerName(PROFILE_CREATION_ERROR),
      base::StringValue(error));
  // The ProfileManager calls us back with a NULL profile in some cases.
  if (profile) {
    webui::DeleteProfileAtPath(profile->GetPath(),
                               web_ui(),
                               ProfileMetrics::DELETE_PROFILE_SETTINGS);
  }
  profile_creation_type_ = NO_CREATION_IN_PROGRESS;
  profile_path_being_created_.clear();
}

void SigninCreateProfileHandler::RecordProfileCreationMetrics(
    Profile::CreateStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Profile.CreateResult", status,
                            Profile::MAX_CREATE_STATUS);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "Profile.CreateTimeNoTimeout",
      base::TimeTicks::Now() - profile_creation_start_time_);
}

base::string16 SigninCreateProfileHandler::GetProfileCreationErrorMessageLocal()
    const {
  int message_id = IDS_PROFILES_CREATE_LOCAL_ERROR;
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Local errors can occur during supervised profile import.
  if (profile_creation_type_ == SUPERVISED_PROFILE_IMPORT)
    message_id = IDS_LEGACY_SUPERVISED_USER_IMPORT_LOCAL_ERROR;
#endif
  return l10n_util::GetStringUTF16(message_id);
}

void SigninCreateProfileHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_WINDOW_READY, type);

  // Only respond to one Browser Window Ready event.
  registrar_.Remove(this, chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                    content::NotificationService::AllSources());
  UserManager::Hide();
}

void SigninCreateProfileHandler::OnBrowserReadyCallback(
    Profile* profile,
    Profile::CreateStatus profile_create_status) {
  Browser* browser = chrome::FindAnyBrowser(profile, false);
  // Closing the User Manager before the window is created can flakily cause
  // Chrome to close.
  if (browser && browser->window()) {
    UserManager::Hide();
  } else {
    registrar_.Add(this,
                   chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                   content::NotificationService::AllSources());
  }
}

base::StringValue SigninCreateProfileHandler::GetWebUIListenerName(
    ProfileCreationStatus status) const {
  switch (status) {
    case PROFILE_CREATION_SUCCESS:
      return base::StringValue("create-profile-success");
    case PROFILE_CREATION_ERROR:
      return base::StringValue("create-profile-error");
  }
  NOTREACHED();
  return base::StringValue(std::string());
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
base::string16 SigninCreateProfileHandler::GetProfileCreateErrorMessageRemote()
    const {
  return l10n_util::GetStringUTF16(
      profile_creation_type_ == SUPERVISED_PROFILE_IMPORT
          ? IDS_LEGACY_SUPERVISED_USER_IMPORT_REMOTE_ERROR
          : IDS_PROFILES_CREATE_REMOTE_ERROR);
}

base::string16 SigninCreateProfileHandler::GetProfileCreateErrorMessageSignin()
    const {
  return l10n_util::GetStringUTF16(
      profile_creation_type_ == SUPERVISED_PROFILE_IMPORT
          ? IDS_LEGACY_SUPERVISED_USER_IMPORT_SIGN_IN_ERROR
          : IDS_PROFILES_CREATE_SIGN_IN_ERROR);
}

bool SigninCreateProfileHandler::GetSupervisedCreateProfileArgs(
    const base::ListValue* args,
    std::string* supervised_user_id,
    base::FilePath* custodian_profile_path) {
  bool supervised_user = false;
  bool success = args->GetBoolean(3, &supervised_user);
  DCHECK(success);

  if (!supervised_user)
    return false;

  success = args->GetString(4, supervised_user_id);
  DCHECK(success);
  const base::Value* path_value;
  success = args->Get(5, &path_value);
  DCHECK(success);
  success = base::GetValueAsFilePath(*path_value, custodian_profile_path);
  DCHECK(success);

  return !custodian_profile_path->empty();
}

void SigninCreateProfileHandler::LoadCustodianProfileCallback(
    const base::string16& name,
    const std::string& icon_url,
    bool create_shortcut,
    const std::string& supervised_user_id,
    Profile* custodian_profile,
    Profile::CreateStatus status) {
  // This method gets called once before with Profile::CREATE_STATUS_CREATED.
  switch (status) {
    case Profile::CREATE_STATUS_LOCAL_FAIL: {
      ShowProfileCreationError(nullptr, GetProfileCreationErrorMessageLocal());
      break;
    }
    case Profile::CREATE_STATUS_CREATED: {
      // Ignore the intermediate status.
      break;
    }
    case Profile::CREATE_STATUS_INITIALIZED: {
      // We are only interested in Profile::CREATE_STATUS_INITIALIZED when
      // everything is ready.
      if (!IsAccountConnected(custodian_profile) ||
          HasAuthError(custodian_profile)) {
        ShowProfileCreationError(nullptr, l10n_util::GetStringFUTF16(
          IDS_PROFILES_CREATE_CUSTODIAN_ACCOUNT_DETAILS_OUT_OF_DATE_ERROR,
          base::ASCIIToUTF16(custodian_profile->GetProfileUserName())));
        return;
      }

      PrefService* prefs = custodian_profile->GetPrefs();
      if (!prefs->GetBoolean(prefs::kSupervisedUserCreationAllowed)) {
        ShowProfileCreationError(
            nullptr,
            l10n_util::GetStringUTF16(
                IDS_PROFILES_CREATE_SUPERVISED_NOT_ALLOWED_BY_POLICY));
        return;
      }

      if (!supervised_user_id.empty()) {
        profile_creation_type_ = SUPERVISED_PROFILE_IMPORT;

        // Load all supervised users managed by this user in order to
        // check if this supervised user already exists on this device.
        SupervisedUserSyncService* supervised_user_sync_service =
            SupervisedUserSyncServiceFactory::GetForProfile(custodian_profile);
        if (supervised_user_sync_service) {
          supervised_user_sync_service->GetSupervisedUsersAsync(base::Bind(
              &SigninCreateProfileHandler::DoCreateProfileIfPossible,
              weak_ptr_factory_.GetWeakPtr(), name, icon_url, create_shortcut,
              supervised_user_id, custodian_profile));
        } else {
          ShowProfileCreationError(nullptr,
                                   GetProfileCreateErrorMessageRemote());
        }
      } else {
        profile_creation_type_ = SUPERVISED_PROFILE_CREATION;
        std::string new_supervised_user_id =
            SupervisedUserRegistrationUtility::GenerateNewSupervisedUserId();

        // If sync is not yet fully initialized, the creation may take extra
        // time, so show a message. Import doesn't wait for an acknowledgment,
        // so it won't have the same potential delay.
        browser_sync::ProfileSyncService* sync_service =
            ProfileSyncServiceFactory::GetInstance()->GetForProfile(
                custodian_profile);
        browser_sync::ProfileSyncService::SyncStatusSummary status =
            sync_service->QuerySyncStatusSummary();
        if (status ==
            browser_sync::ProfileSyncService::DATATYPES_NOT_INITIALIZED) {
          ShowProfileCreationWarning(l10n_util::GetStringUTF16(
              IDS_PROFILES_CREATE_SUPERVISED_JUST_SIGNED_IN));
        }

        DoCreateProfile(name, icon_url, create_shortcut, new_supervised_user_id,
                        custodian_profile);
      }
      break;
    }
    case Profile::CREATE_STATUS_CANCELED:
    case Profile::CREATE_STATUS_REMOTE_FAIL:
    case Profile::MAX_CREATE_STATUS: {
      NOTREACHED();
      break;
    }
  }
}

bool SigninCreateProfileHandler::IsAccountConnected(Profile* profile)
    const {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  return signin_manager && signin_manager->IsAuthenticated();
}

bool SigninCreateProfileHandler::HasAuthError(Profile* profile)
    const {
  SigninErrorController* error_controller =
      SigninErrorControllerFactory::GetForProfile(profile);
  if (!error_controller)
    return true;

  GoogleServiceAuthError::State state = error_controller->auth_error().state();
  return state != GoogleServiceAuthError::NONE;
}

void SigninCreateProfileHandler::DoCreateProfileIfPossible(
    const base::string16& name,
    const std::string& icon_url,
    bool create_shortcut,
    const std::string& supervised_user_id,
    Profile* custodian_profile,
    const base::DictionaryValue* dict) {
  DCHECK(dict);
  if (!dict->HasKey(supervised_user_id))
    return;

  // Check if this supervised user already exists on this machine.
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()->
          GetProfileAttributesStorage().GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    if (supervised_user_id == entry->GetSupervisedUserId()) {
      ShowProfileCreationError(nullptr, GetProfileCreationErrorMessageLocal());
      return;
    }
  }

  DoCreateProfile(name, icon_url, create_shortcut, supervised_user_id,
                  custodian_profile);
}

void SigninCreateProfileHandler::HandleCancelProfileCreation(
    const base::ListValue* args) {
  CancelProfileRegistration(true);
}

// Non-supervised user creation cannot be canceled. (Creating a non-supervised
// profile shouldn't take significant time, and it can easily be deleted
// afterward.)
void SigninCreateProfileHandler::CancelProfileRegistration(
    bool user_initiated) {
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

  // Canceling registration means the callback passed into
  // RegisterAndInitSync() won't be called, so the cleanup must be done here.
  profile_path_being_created_.clear();
  webui::DeleteProfileAtPath(new_profile->GetPath(),
                             web_ui(),
                             ProfileMetrics::DELETE_PROFILE_SETTINGS);
}

void SigninCreateProfileHandler::RegisterSupervisedUser(
    bool create_shortcut,
    const std::string& supervised_user_id,
    Profile* custodian_profile,
    Profile* new_profile) {
  DCHECK_EQ(profile_path_being_created_.value(),
            new_profile->GetPath().value());

  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(new_profile);

  // Register the supervised user using the profile of the custodian.
  supervised_user_registration_utility_ =
      SupervisedUserRegistrationUtility::Create(custodian_profile);
  if (supervised_user_service) {
    supervised_user_service->RegisterAndInitSync(
        supervised_user_registration_utility_.get(), custodian_profile,
        supervised_user_id,
        base::Bind(&SigninCreateProfileHandler::OnSupervisedUserRegistered,
                   weak_ptr_factory_.GetWeakPtr(), create_shortcut,
                   custodian_profile, new_profile));
  }
}

void SigninCreateProfileHandler::OnSupervisedUserRegistered(
    bool create_shortcut,
    Profile* custodian_profile,
    Profile* profile,
    const GoogleServiceAuthError& error) {
  GoogleServiceAuthError::State state = error.state();
  RecordSupervisedProfileCreationMetrics(state);
  if (state == GoogleServiceAuthError::NONE) {
    CreateShortcutAndShowSuccess(create_shortcut, custodian_profile, profile);
    return;
  }

  base::string16 error_msg;
  if (state == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
      state == GoogleServiceAuthError::USER_NOT_SIGNED_UP ||
      state == GoogleServiceAuthError::ACCOUNT_DELETED ||
      state == GoogleServiceAuthError::ACCOUNT_DISABLED) {
    error_msg = GetProfileCreateErrorMessageSignin();
  } else {
    error_msg = GetProfileCreateErrorMessageRemote();
  }
  ShowProfileCreationError(profile, error_msg);
}

void SigninCreateProfileHandler::ShowProfileCreationWarning(
    const base::string16& warning) {
  DCHECK_EQ(SUPERVISED_PROFILE_CREATION, profile_creation_type_);
  web_ui()->CallJavascriptFunctionUnsafe(
      "cr.webUIListenerCallback", base::StringValue("create-profile-warning"),
      base::StringValue(warning));
}

void SigninCreateProfileHandler::RecordSupervisedProfileCreationMetrics(
    GoogleServiceAuthError::State error_state) {
  if (profile_creation_type_ == SUPERVISED_PROFILE_CREATION) {
    UMA_HISTOGRAM_ENUMERATION("Profile.SupervisedProfileCreateError",
                              error_state, GoogleServiceAuthError::NUM_STATES);
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Profile.SupervisedProfileTotalCreateTime",
        base::TimeTicks::Now() - profile_creation_start_time_);
  } else {
    DCHECK_EQ(SUPERVISED_PROFILE_IMPORT, profile_creation_type_);
    UMA_HISTOGRAM_ENUMERATION("Profile.SupervisedProfileImportError",
                              error_state, GoogleServiceAuthError::NUM_STATES);
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Profile.SupervisedProfileTotalImportTime",
        base::TimeTicks::Now() - profile_creation_start_time_);
  }
}

void SigninCreateProfileHandler::SwitchToProfile(
      const base::ListValue* args) {
  DCHECK(args);
  const base::Value* file_path_value;
  if (!args->Get(0, &file_path_value))
    return;

  base::FilePath profile_file_path;
  if (!base::GetValueAsFilePath(*file_path_value, &profile_file_path))
    return;

  Profile* profile = g_browser_process->profile_manager()->
      GetProfileByPath(profile_file_path);
  DCHECK(profile);

  profiles::OpenBrowserWindowForProfile(
      base::Bind(&SigninCreateProfileHandler::OnBrowserReadyCallback,
                 weak_ptr_factory_.GetWeakPtr()),
      false,  // Don't create a window if one already exists.
      true,  // Create a first run window.
      profile,
      Profile::CREATE_STATUS_INITIALIZED);
}
#endif
