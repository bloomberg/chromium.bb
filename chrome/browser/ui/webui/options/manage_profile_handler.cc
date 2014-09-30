// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/manage_profile_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/options/options_handlers_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(ENABLE_SETTINGS_APP)
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "content/public/browser/web_contents.h"
#endif

namespace options {

namespace {

const char kCreateProfileIdentifier[] = "create";
const char kManageProfileIdentifier[] = "manage";

// Given |args| from the WebUI, parses value 0 as a FilePath |profile_file_path|
// and returns true on success.
bool GetProfilePathFromArgs(const base::ListValue* args,
                            base::FilePath* profile_file_path) {
  const base::Value* file_path_value;
  if (!args->Get(0, &file_path_value))
    return false;
  return base::GetValueAsFilePath(*file_path_value, profile_file_path);
}

}  // namespace

ManageProfileHandler::ManageProfileHandler()
    : weak_factory_(this) {
}

ManageProfileHandler::~ManageProfileHandler() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  // Sync may be disabled in tests.
  if (service)
    service->RemoveObserver(this);
}

void ManageProfileHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "manageProfilesNameLabel", IDS_PROFILES_MANAGE_NAME_LABEL },
    { "manageProfilesDuplicateNameError",
        IDS_PROFILES_MANAGE_DUPLICATE_NAME_ERROR },
    { "manageProfilesIconLabel", IDS_PROFILES_MANAGE_ICON_LABEL },
    { "manageProfilesExistingSupervisedUser",
        IDS_PROFILES_CREATE_EXISTING_SUPERVISED_USER_ERROR },
    { "manageProfilesSupervisedSignedInLabel",
        IDS_PROFILES_CREATE_SUPERVISED_SIGNED_IN_LABEL },
    { "manageProfilesSupervisedNotSignedIn",
        IDS_PROFILES_CREATE_SUPERVISED_NOT_SIGNED_IN_HTML },
    { "manageProfilesSupervisedAccountDetailsOutOfDate",
        IDS_PROFILES_CREATE_SUPERVISED_ACCOUNT_DETAILS_OUT_OF_DATE_LABEL },
    { "manageProfilesSupervisedSignInAgainLink",
        IDS_PROFILES_CREATE_SUPERVISED_SIGN_IN_AGAIN_LINK },
    { "manageProfilesConfirm", IDS_SAVE },
    { "deleteProfileTitle", IDS_PROFILES_DELETE_TITLE },
    { "deleteProfileOK", IDS_PROFILES_DELETE_OK_BUTTON_LABEL },
    { "deleteProfileMessage", IDS_PROFILES_DELETE_MESSAGE },
    { "deleteSupervisedProfileAddendum",
        IDS_PROFILES_DELETE_SUPERVISED_ADDENDUM },
    { "disconnectManagedProfileTitle",
        IDS_PROFILES_DISCONNECT_MANAGED_PROFILE_TITLE },
    { "disconnectManagedProfileOK",
        IDS_PROFILES_DISCONNECT_MANAGED_PROFILE_OK_BUTTON_LABEL },
    { "createProfileTitle", IDS_PROFILES_CREATE_TITLE },
    { "createProfileInstructions", IDS_PROFILES_CREATE_INSTRUCTIONS },
    { "createProfileConfirm", IDS_ADD },
    { "createProfileShortcutCheckbox", IDS_PROFILES_CREATE_SHORTCUT_CHECKBOX },
    { "createProfileShortcutButton", IDS_PROFILES_CREATE_SHORTCUT_BUTTON },
    { "removeProfileShortcutButton", IDS_PROFILES_REMOVE_SHORTCUT_BUTTON },
    { "importExistingSupervisedUserLink",
        IDS_PROFILES_IMPORT_EXISTING_SUPERVISED_USER_LINK },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "manageProfile", IDS_PROFILES_MANAGE_TITLE);
  RegisterTitle(localized_strings, "createProfile", IDS_PROFILES_CREATE_TITLE);

  localized_strings->SetBoolean("profileShortcutsEnabled",
                                ProfileShortcutManager::IsFeatureEnabled());

  GenerateSignedinUserSpecificStrings(localized_strings);
}

void ManageProfileHandler::InitializeHandler() {
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
                 content::NotificationService::AllSources());

  Profile* profile = Profile::FromWebUI(web_ui());
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSupervisedUserCreationAllowed,
      base::Bind(&ManageProfileHandler::OnCreateSupervisedUserPrefChange,
                 base::Unretained(this)));
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  // Sync may be disabled for tests.
  if (service)
    service->AddObserver(this);
}

void ManageProfileHandler::InitializePage() {
  SendExistingProfileNames();
  OnCreateSupervisedUserPrefChange();
}

void ManageProfileHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("setProfileIconAndName",
      base::Bind(&ManageProfileHandler::SetProfileIconAndName,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestDefaultProfileIcons",
      base::Bind(&ManageProfileHandler::RequestDefaultProfileIcons,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestNewProfileDefaults",
      base::Bind(&ManageProfileHandler::RequestNewProfileDefaults,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestHasProfileShortcuts",
      base::Bind(&ManageProfileHandler::RequestHasProfileShortcuts,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestCreateProfileUpdate",
      base::Bind(&ManageProfileHandler::RequestCreateProfileUpdate,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("profileIconSelectionChanged",
      base::Bind(&ManageProfileHandler::ProfileIconSelectionChanged,
                 base::Unretained(this)));
#if defined(ENABLE_SETTINGS_APP)
  web_ui()->RegisterMessageCallback("switchAppListProfile",
      base::Bind(&ManageProfileHandler::SwitchAppListProfile,
                 base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback("addProfileShortcut",
      base::Bind(&ManageProfileHandler::AddProfileShortcut,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeProfileShortcut",
      base::Bind(&ManageProfileHandler::RemoveProfileShortcut,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("refreshGaiaPicture",
      base::Bind(&ManageProfileHandler::RefreshGaiaPicture,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showDisconnectManagedProfileDialog",
      base::Bind(&ManageProfileHandler::ShowDisconnectManagedProfileDialog,
                 base::Unretained(this)));
}

void ManageProfileHandler::Uninitialize() {
  registrar_.RemoveAll();
}

void ManageProfileHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED) {
    SendExistingProfileNames();
    base::StringValue value(kManageProfileIdentifier);
    SendProfileIconsAndNames(value);
  }
}

void ManageProfileHandler::OnStateChanged() {
  RequestCreateProfileUpdate(NULL);
}

void ManageProfileHandler::GenerateSignedinUserSpecificStrings(
    base::DictionaryValue* dictionary) {
  std::string username;
  std::string domain_name;

#if !defined(OS_CHROMEOS)
  Profile* profile = Profile::FromWebUI(web_ui());
  DCHECK(profile);
  SigninManagerBase* manager = SigninManagerFactory::GetForProfile(profile);
  if (manager) {
    username = manager->GetAuthenticatedUsername();
    // If there is no one logged in or if the profile name is empty then the
    // domain name is empty. This happens in browser tests.
    if (!username.empty()) {
      domain_name = "<span id=disconnect-managed-profile-domain-name>" +
                    gaia::ExtractDomainName(username) + "</span>";
    }
  }
#endif

  dictionary->SetString(
      "disconnectManagedProfileDomainInformation",
      l10n_util::GetStringFUTF16(
          IDS_PROFILES_DISCONNECT_MANAGED_PROFILE_DOMAIN_INFORMATION,
          base::ASCIIToUTF16(domain_name)));

  dictionary->SetString(
      "disconnectManagedProfileText",
      l10n_util::GetStringFUTF16(
          IDS_PROFILES_DISCONNECT_MANAGED_PROFILE_TEXT,
          base::UTF8ToUTF16(username),
          base::UTF8ToUTF16(chrome::kSyncGoogleDashboardURL)));
}

void ManageProfileHandler::RequestDefaultProfileIcons(
    const base::ListValue* args) {
  std::string mode;
  bool ok = args->GetString(0, &mode);
  DCHECK(ok);
  DCHECK(mode == kCreateProfileIdentifier || mode == kManageProfileIdentifier);
  if (ok) {
    base::StringValue value(mode);
    SendProfileIconsAndNames(value);
  }
}

void ManageProfileHandler::RequestNewProfileDefaults(
    const base::ListValue* args) {
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  const size_t icon_index = cache.ChooseAvatarIconIndexForNewProfile();

  base::DictionaryValue profile_info;
  profile_info.SetString("name", cache.ChooseNameForNewProfile(icon_index));
  profile_info.SetString("iconURL",
      profiles::GetDefaultAvatarIconUrl(icon_index));

  web_ui()->CallJavascriptFunction(
      "ManageProfileOverlay.receiveNewProfileDefaults", profile_info);
}

void ManageProfileHandler::SendProfileIconsAndNames(
    const base::StringValue& mode) {
  base::ListValue image_url_list;
  base::ListValue default_name_list;

  // First add the GAIA picture if it's available.
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  Profile* profile = Profile::FromWebUI(web_ui());
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  if (profile_index != std::string::npos) {
    const gfx::Image* icon =
        cache.GetGAIAPictureOfProfileAtIndex(profile_index);
    if (icon) {
      gfx::Image icon2 = profiles::GetAvatarIconForWebUI(*icon, true);
      gaia_picture_url_ = webui::GetBitmapDataUrl(icon2.AsBitmap());
      image_url_list.AppendString(gaia_picture_url_);
      default_name_list.AppendString(std::string());
    }
  }

  // Next add the default avatar icons and names.
  for (size_t i = 0; i < profiles::GetDefaultAvatarIconCount(); i++) {
    std::string url = profiles::GetDefaultAvatarIconUrl(i);
    image_url_list.AppendString(url);
    default_name_list.AppendString(cache.ChooseNameForNewProfile(i));
  }

  web_ui()->CallJavascriptFunction(
      "ManageProfileOverlay.receiveDefaultProfileIconsAndNames", mode,
      image_url_list, default_name_list);
}

void ManageProfileHandler::SendExistingProfileNames() {
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  base::DictionaryValue profile_name_dict;
  for (size_t i = 0, e = cache.GetNumberOfProfiles(); i < e; ++i) {
    profile_name_dict.SetBoolean(
        base::UTF16ToUTF8(cache.GetNameOfProfileAtIndex(i)), true);
  }

  web_ui()->CallJavascriptFunction(
      "ManageProfileOverlay.receiveExistingProfileNames", profile_name_dict);
}

void ManageProfileHandler::ShowDisconnectManagedProfileDialog(
    const base::ListValue* args) {
  base::DictionaryValue replacements;
  GenerateSignedinUserSpecificStrings(&replacements);
  web_ui()->CallJavascriptFunction(
      "ManageProfileOverlay.showDisconnectManagedProfileDialog", replacements);
}

void ManageProfileHandler::SetProfileIconAndName(const base::ListValue* args) {
  DCHECK(args);

  base::FilePath profile_file_path;
  if (!GetProfilePathFromArgs(args, &profile_file_path))
    return;

  Profile* profile =
      g_browser_process->profile_manager()->GetProfile(profile_file_path);
  if (!profile)
    return;

  std::string icon_url;
  if (!args->GetString(1, &icon_url))
    return;

  PrefService* pref_service = profile->GetPrefs();
  // Updating the profile preferences will cause the cache to be updated.

  // Metrics logging variable.
  bool previously_using_gaia_icon =
      pref_service->GetBoolean(prefs::kProfileUsingGAIAAvatar);

  size_t new_icon_index;
  if (icon_url == gaia_picture_url_) {
    pref_service->SetBoolean(prefs::kProfileUsingDefaultAvatar, false);
    pref_service->SetBoolean(prefs::kProfileUsingGAIAAvatar, true);
    if (!previously_using_gaia_icon) {
      // Only log if they changed to the GAIA photo.
      // Selection of GAIA photo as avatar is logged as part of the function
      // below.
      ProfileMetrics::LogProfileSwitchGaia(ProfileMetrics::GAIA_OPT_IN);
    }
  } else if (profiles::IsDefaultAvatarIconUrl(icon_url, &new_icon_index)) {
    ProfileMetrics::LogProfileAvatarSelection(new_icon_index);
    pref_service->SetInteger(prefs::kProfileAvatarIndex, new_icon_index);
    pref_service->SetBoolean(prefs::kProfileUsingDefaultAvatar, false);
    pref_service->SetBoolean(prefs::kProfileUsingGAIAAvatar, false);
  }
  ProfileMetrics::LogProfileUpdate(profile_file_path);

  if (profile->IsSupervised())
    return;

  base::string16 new_profile_name;
  if (!args->GetString(2, &new_profile_name))
    return;

  base::TrimWhitespace(new_profile_name, base::TRIM_ALL, &new_profile_name);
  CHECK(!new_profile_name.empty());
  profiles::UpdateProfileName(profile, new_profile_name);
}

#if defined(ENABLE_SETTINGS_APP)
void ManageProfileHandler::SwitchAppListProfile(const base::ListValue* args) {
  DCHECK(args);
  DCHECK(profiles::IsMultipleProfilesEnabled());

  const base::Value* file_path_value;
  base::FilePath profile_file_path;
  if (!args->Get(0, &file_path_value) ||
      !base::GetValueAsFilePath(*file_path_value, &profile_file_path))
    return;

  AppListService* app_list_service = AppListService::Get(
      options::helper::GetDesktopType(web_ui()));
  app_list_service->SetProfilePath(profile_file_path);
  app_list_service->Show();

  // Close the settings app, since it will now be for the wrong profile.
  web_ui()->GetWebContents()->Close();
}
#endif  // defined(ENABLE_SETTINGS_APP)

void ManageProfileHandler::ProfileIconSelectionChanged(
    const base::ListValue* args) {
  DCHECK(args);

  base::FilePath profile_file_path;
  if (!GetProfilePathFromArgs(args, &profile_file_path))
    return;

  // Currently this only supports editing the current profile's info.
  if (profile_file_path != Profile::FromWebUI(web_ui())->GetPath())
    return;

  std::string icon_url;
  if (!args->GetString(1, &icon_url))
    return;

  if (icon_url != gaia_picture_url_)
    return;

  // If the selection is the GAIA picture then also show the profile name in the
  // text field. This will display either the GAIA given name, if available,
  // or the first name.
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_file_path);
  if (profile_index == std::string::npos)
    return;
  base::string16 gaia_name = cache.GetNameOfProfileAtIndex(profile_index);
  if (gaia_name.empty())
    return;

  base::StringValue gaia_name_value(gaia_name);
  base::StringValue mode_value(kManageProfileIdentifier);
  web_ui()->CallJavascriptFunction("ManageProfileOverlay.setProfileName",
                                   gaia_name_value, mode_value);
}

void ManageProfileHandler::RequestHasProfileShortcuts(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ProfileShortcutManager::IsFeatureEnabled());

  base::FilePath profile_file_path;
  if (!GetProfilePathFromArgs(args, &profile_file_path))
    return;

  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_file_path);
  if (profile_index == std::string::npos)
    return;

  // Don't show the add/remove desktop shortcut button in the single user case.
  if (cache.GetNumberOfProfiles() <= 1)
    return;

  const base::FilePath profile_path =
      cache.GetPathOfProfileAtIndex(profile_index);
  ProfileShortcutManager* shortcut_manager =
      g_browser_process->profile_manager()->profile_shortcut_manager();
  shortcut_manager->HasProfileShortcuts(
      profile_path, base::Bind(&ManageProfileHandler::OnHasProfileShortcuts,
                               weak_factory_.GetWeakPtr()));
}

void ManageProfileHandler::RequestCreateProfileUpdate(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  SigninManagerBase* manager =
      SigninManagerFactory::GetForProfile(profile);
  base::string16 username =
      base::UTF8ToUTF16(manager->GetAuthenticatedUsername());
  ProfileSyncService* service =
     ProfileSyncServiceFactory::GetForProfile(profile);
  GoogleServiceAuthError::State state = service->GetAuthError().state();
  bool has_error = (state == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
                    state == GoogleServiceAuthError::USER_NOT_SIGNED_UP ||
                    state == GoogleServiceAuthError::ACCOUNT_DELETED ||
                    state == GoogleServiceAuthError::ACCOUNT_DISABLED);
  web_ui()->CallJavascriptFunction("CreateProfileOverlay.updateSignedInStatus",
                                   base::StringValue(username),
                                   base::FundamentalValue(has_error));

  OnCreateSupervisedUserPrefChange();
}

void ManageProfileHandler::OnCreateSupervisedUserPrefChange() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  base::FundamentalValue allowed(
      prefs->GetBoolean(prefs::kSupervisedUserCreationAllowed));
  web_ui()->CallJavascriptFunction(
      "CreateProfileOverlay.updateSupervisedUsersAllowed", allowed);
}

void ManageProfileHandler::OnHasProfileShortcuts(bool has_shortcuts) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::FundamentalValue has_shortcuts_value(has_shortcuts);
  web_ui()->CallJavascriptFunction(
      "ManageProfileOverlay.receiveHasProfileShortcuts", has_shortcuts_value);
}

void ManageProfileHandler::AddProfileShortcut(const base::ListValue* args) {
  base::FilePath profile_file_path;
  if (!GetProfilePathFromArgs(args, &profile_file_path))
    return;

  DCHECK(ProfileShortcutManager::IsFeatureEnabled());
  ProfileShortcutManager* shortcut_manager =
      g_browser_process->profile_manager()->profile_shortcut_manager();
  DCHECK(shortcut_manager);

  shortcut_manager->CreateProfileShortcut(profile_file_path);

  // Update the UI buttons.
  OnHasProfileShortcuts(true);
}

void ManageProfileHandler::RemoveProfileShortcut(const base::ListValue* args) {
  base::FilePath profile_file_path;
  if (!GetProfilePathFromArgs(args, &profile_file_path))
    return;

  DCHECK(ProfileShortcutManager::IsFeatureEnabled());
  ProfileShortcutManager* shortcut_manager =
    g_browser_process->profile_manager()->profile_shortcut_manager();
  DCHECK(shortcut_manager);

  shortcut_manager->RemoveProfileShortcuts(profile_file_path);

  // Update the UI buttons.
  OnHasProfileShortcuts(false);
}

void ManageProfileHandler::RefreshGaiaPicture(const base::ListValue* args) {
  profiles::UpdateGaiaProfilePhotoIfNeeded(Profile::FromWebUI(web_ui()));
}

}  // namespace options
