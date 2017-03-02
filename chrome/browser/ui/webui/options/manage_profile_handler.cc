// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/manage_profile_handler.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

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

void HandleLogDeleteUserDialogShown(const base::ListValue* args) {
  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_SETTINGS_SHOW_WARNING);
}

}  // namespace

ManageProfileHandler::ManageProfileHandler()
    : weak_factory_(this) {
}

ManageProfileHandler::~ManageProfileHandler() {
  browser_sync::ProfileSyncService* service =
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
    { "manageProfilesIconLabel", IDS_PROFILES_MANAGE_ICON_LABEL },
    { "manageProfilesExistingSupervisedUser",
        IDS_PROFILES_CREATE_LEGACY_SUPERVISED_USER_ERROR_EXISTS_REMOTELY },
    { "managedProfilesExistingLocalSupervisedUser",
        IDS_PROFILES_CREATE_LEGACY_SUPERVISED_USER_ERROR_EXISTS_LOCALLY },
    { "manageProfilesSupervisedSignedInLabel",
        IDS_PROFILES_CREATE_SUPERVISED_SIGNED_IN_LABEL },
    { "manageProfilesSupervisedNotSignedIn",
        IDS_PROFILES_CREATE_SUPERVISED_NOT_SIGNED_IN_HTML },
    { "manageProfilesSupervisedAccountDetailsOutOfDate",
        IDS_PROFILES_CREATE_SUPERVISED_ACCOUNT_DETAILS_OUT_OF_DATE_LABEL },
    { "manageProfilesSupervisedSignInAgainLink",
        IDS_PROFILES_GAIA_REAUTH_TITLE },
    { "manageProfilesConfirm", IDS_SAVE },
    { "deleteProfileTitle", IDS_PROFILES_DELETE_TITLE },
    { "deleteProfileOK", IDS_PROFILES_DELETE_OK_BUTTON_LABEL },
    { "deleteProfileMessage", IDS_PROFILES_DELETE_MESSAGE },
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
        IDS_IMPORT_EXISTING_LEGACY_SUPERVISED_USER_TITLE },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "manageProfile", IDS_PROFILES_MANAGE_TITLE);
  RegisterTitle(localized_strings, "createProfile", IDS_PROFILES_CREATE_TITLE);
  RegisterTitle(localized_strings, "disconnectAccount",
      IDS_DISCONNECT_ACCOUNT_TITLE);

  base::string16 supervised_user_dashboard_url =
      base::ASCIIToUTF16(chrome::kLegacySupervisedUserManagementURL);
  base::string16 supervised_user_dashboard_display =
      base::ASCIIToUTF16(chrome::kLegacySupervisedUserManagementDisplayURL);
  localized_strings->SetString("deleteSupervisedProfileAddendum",
    l10n_util::GetStringFUTF16(IDS_PROFILES_DELETE_LEGACY_SUPERVISED_ADDENDUM,
                               supervised_user_dashboard_url,
                               supervised_user_dashboard_display));

  localized_strings->SetBoolean("profileShortcutsEnabled",
                                ProfileShortcutManager::IsFeatureEnabled());

  GenerateSignedinUserSpecificStrings(localized_strings);
}

void ManageProfileHandler::InitializeHandler() {
  g_browser_process->profile_manager()->
      GetProfileAttributesStorage().AddObserver(this);

  Profile* profile = Profile::FromWebUI(web_ui());
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSupervisedUserCreationAllowed,
      base::Bind(&ManageProfileHandler::OnCreateSupervisedUserPrefChange,
                 base::Unretained(this)));
  browser_sync::ProfileSyncService* service =
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
  web_ui()->RegisterMessageCallback("logDeleteUserDialogShown",
      base::Bind(&HandleLogDeleteUserDialogShown));
}

void ManageProfileHandler::Uninitialize() {
  g_browser_process->profile_manager()->
      GetProfileAttributesStorage().RemoveObserver(this);
}

void ManageProfileHandler::OnProfileAdded(const base::FilePath& profile_path) {
  SendExistingProfileNames();
}

void ManageProfileHandler::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  SendExistingProfileNames();
}

void ManageProfileHandler::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  base::StringValue value(kManageProfileIdentifier);
  SendProfileIconsAndNames(value);
}

void ManageProfileHandler::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  base::StringValue value(kManageProfileIdentifier);
  SendProfileIconsAndNames(value);
}

void ManageProfileHandler::OnStateChanged(syncer::SyncService* sync) {
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
    username = manager->GetAuthenticatedAccountInfo().email;
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
  const ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  const size_t icon_index = storage.ChooseAvatarIconIndexForNewProfile();

  base::DictionaryValue profile_info;
  profile_info.SetString("name", storage.ChooseNameForNewProfile(icon_index));
  profile_info.SetString("iconURL",
      profiles::GetDefaultAvatarIconUrl(icon_index));

  web_ui()->CallJavascriptFunctionUnsafe(
      "ManageProfileOverlay.receiveNewProfileDefaults", profile_info);
}

void ManageProfileHandler::SendProfileIconsAndNames(
    const base::StringValue& mode) {
  base::ListValue image_url_list;
  base::ListValue default_name_list;

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();

  // In manage mode, first add the GAIA picture if it is available. No GAIA
  // picture in create mode.
  if (mode.GetString() == kManageProfileIdentifier) {
    Profile* profile = Profile::FromWebUI(web_ui());
    ProfileAttributesEntry* entry = nullptr;
    bool success = storage.GetProfileAttributesWithPath(profile->GetPath(),
                                                        &entry);
    const gfx::Image* icon = success ? entry->GetGAIAPicture() : nullptr;
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
    default_name_list.AppendString(storage.ChooseNameForNewProfile(i));
  }

  web_ui()->CallJavascriptFunctionUnsafe(
      "ManageProfileOverlay.receiveDefaultProfileIconsAndNames", mode,
      image_url_list, default_name_list);
}

void ManageProfileHandler::SendExistingProfileNames() {
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()->
      GetProfileAttributesStorage().GetAllProfilesAttributes();
  base::DictionaryValue profile_name_dict;
  for (const ProfileAttributesEntry* entry : entries)
    profile_name_dict.SetBoolean(base::UTF16ToUTF8(entry->GetName()), true);

  web_ui()->CallJavascriptFunctionUnsafe(
      "ManageProfileOverlay.receiveExistingProfileNames", profile_name_dict);
}

void ManageProfileHandler::ShowDisconnectManagedProfileDialog(
    const base::ListValue* args) {
  base::DictionaryValue replacements;
  GenerateSignedinUserSpecificStrings(&replacements);
  web_ui()->CallJavascriptFunctionUnsafe(
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
  } else {
    // Only default avatars and Gaia account photos are supported.
    CHECK(false);
  }
  ProfileMetrics::LogProfileUpdate(profile_file_path);

  if (profile->IsLegacySupervised())
    return;

  base::string16 new_profile_name;
  if (!args->GetString(2, &new_profile_name))
    return;

  base::TrimWhitespace(new_profile_name, base::TRIM_ALL, &new_profile_name);
  CHECK(!new_profile_name.empty());
  profiles::UpdateProfileName(profile, new_profile_name);
}

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
  ProfileAttributesEntry* entry = nullptr;
  if (!g_browser_process->profile_manager()->GetProfileAttributesStorage().
      GetProfileAttributesWithPath(profile_file_path, &entry))
    return;
  base::string16 gaia_name = entry->GetName();
  if (gaia_name.empty())
    return;

  base::StringValue gaia_name_value(gaia_name);
  base::StringValue mode_value(kManageProfileIdentifier);
  web_ui()->CallJavascriptFunctionUnsafe("ManageProfileOverlay.setProfileName",
                                         gaia_name_value, mode_value);
}

void ManageProfileHandler::RequestHasProfileShortcuts(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ProfileShortcutManager::IsFeatureEnabled());

  base::FilePath profile_file_path;
  if (!GetProfilePathFromArgs(args, &profile_file_path))
    return;

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry;
  if (!storage.GetProfileAttributesWithPath(profile_file_path, &entry))
    return;

  // Don't show the add/remove desktop shortcut button in the single user case.
  if (storage.GetNumberOfProfiles() <= 1u)
    return;

  ProfileShortcutManager* shortcut_manager =
      g_browser_process->profile_manager()->profile_shortcut_manager();
  shortcut_manager->HasProfileShortcuts(
      entry->GetPath(), base::Bind(&ManageProfileHandler::OnHasProfileShortcuts,
                                   weak_factory_.GetWeakPtr()));
}

void ManageProfileHandler::RequestCreateProfileUpdate(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  SigninManagerBase* manager =
      SigninManagerFactory::GetForProfile(profile);
  base::string16 username =
      base::UTF8ToUTF16(manager->GetAuthenticatedAccountInfo().email);
  browser_sync::ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  GoogleServiceAuthError::State state = GoogleServiceAuthError::NONE;

  // |service| might be null if Sync is disabled from the command line.
  if (service)
    state = service->GetAuthError().state();

  bool has_error = (!service ||
                    state == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
                    state == GoogleServiceAuthError::USER_NOT_SIGNED_UP ||
                    state == GoogleServiceAuthError::ACCOUNT_DELETED ||
                    state == GoogleServiceAuthError::ACCOUNT_DISABLED);
  web_ui()->CallJavascriptFunctionUnsafe(
      "CreateProfileOverlay.updateSignedInStatus", base::StringValue(username),
      base::Value(has_error));

  OnCreateSupervisedUserPrefChange();
}

void ManageProfileHandler::OnCreateSupervisedUserPrefChange() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  base::Value allowed(prefs->GetBoolean(prefs::kSupervisedUserCreationAllowed));
  web_ui()->CallJavascriptFunctionUnsafe(
      "CreateProfileOverlay.updateSupervisedUsersAllowed", allowed);
}

void ManageProfileHandler::OnHasProfileShortcuts(bool has_shortcuts) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value has_shortcuts_value(has_shortcuts);
  web_ui()->CallJavascriptFunctionUnsafe(
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
  profiles::UpdateGaiaProfileInfoIfNeeded(Profile::FromWebUI(web_ui()));
}

}  // namespace options
