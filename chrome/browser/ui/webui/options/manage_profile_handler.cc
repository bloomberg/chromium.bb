// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/manage_profile_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/webui/web_ui_util.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#endif

#if defined(ENABLE_SETTINGS_APP)
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "content/public/browser/web_contents.h"
#endif

namespace options {

namespace {

const char kCreateProfileIconGridName[] = "create-profile-icon-grid";
const char kManageProfileIconGridName[] = "manage-profile-icon-grid";

// Given |args| from the WebUI, parses value 0 as a FilePath |profile_file_path|
// and returns true on success.
bool GetProfilePathFromArgs(const ListValue* args,
                            base::FilePath* profile_file_path) {
  const Value* file_path_value;
  if (!args->Get(0, &file_path_value))
    return false;
  return base::GetValueAsFilePath(*file_path_value, profile_file_path);
}

}  // namespace

ManageProfileHandler::ManageProfileHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

ManageProfileHandler::~ManageProfileHandler() {
}

void ManageProfileHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "manageProfilesNameLabel", IDS_PROFILES_MANAGE_NAME_LABEL },
    { "manageProfilesDuplicateNameError",
        IDS_PROFILES_MANAGE_DUPLICATE_NAME_ERROR },
    { "manageProfilesIconLabel", IDS_PROFILES_MANAGE_ICON_LABEL },
    { "manageProfilesManagedUserSettings",
        IDS_PROFILES_MANAGE_MANAGED_USER_SETTINGS_BUTTON },
    { "manageProfilesManagedLabel", IDS_PROFILES_CREATE_MANAGED_CHECKBOX },
    { "deleteProfileTitle", IDS_PROFILES_DELETE_TITLE },
    { "deleteProfileOK", IDS_PROFILES_DELETE_OK_BUTTON_LABEL },
    { "deleteProfileMessage", IDS_PROFILES_DELETE_MESSAGE },
    { "createProfileTitle", IDS_PROFILES_CREATE_TITLE },
    { "createProfileInstructions", IDS_PROFILES_CREATE_INSTRUCTIONS },
    { "createProfileConfirm", IDS_PROFILES_CREATE_CONFIRM },
    { "createProfileShortcut", IDS_PROFILES_CREATE_SHORTCUT },
    { "removeProfileShortcut", IDS_PROFILES_REMOVE_SHORTCUT },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "manageProfile",
                IDS_PROFILES_MANAGE_TITLE);
  RegisterTitle(localized_strings, "createProfile",
                IDS_PROFILES_CREATE_TITLE);

  localized_strings->SetBoolean("profileShortcutsEnabled",
                                ProfileShortcutManager::IsFeatureEnabled());
}

void ManageProfileHandler::InitializeHandler() {
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
                 content::NotificationService::AllSources());
}

void ManageProfileHandler::InitializePage() {
  SendProfileNames();
}

void ManageProfileHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("setProfileNameAndIcon",
      base::Bind(&ManageProfileHandler::SetProfileNameAndIcon,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("deleteProfile",
      base::Bind(&ManageProfileHandler::DeleteProfile,
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
}

void ManageProfileHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED) {
    SendProfileNames();
    base::StringValue value(kManageProfileIconGridName);
    SendProfileIcons(value);
  } else {
    OptionsPageUIHandler::Observe(type, source, details);
  }
}

void ManageProfileHandler::RequestDefaultProfileIcons(const ListValue* args) {
  base::StringValue create_value(kCreateProfileIconGridName);
  base::StringValue manage_value(kManageProfileIconGridName);
  SendProfileIcons(manage_value);
  SendProfileIcons(create_value);
}

void ManageProfileHandler::RequestNewProfileDefaults(const ListValue* args) {
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  const size_t icon_index = cache.ChooseAvatarIconIndexForNewProfile();

  DictionaryValue profile_info;
  profile_info.SetString("name", cache.ChooseNameForNewProfile(icon_index));
  profile_info.SetString("iconURL", cache.GetDefaultAvatarIconUrl(icon_index));

  web_ui()->CallJavascriptFunction(
      "ManageProfileOverlay.receiveNewProfileDefaults", profile_info);
}

void ManageProfileHandler::SendProfileIcons(
    const base::StringValue& icon_grid) {
  ListValue image_url_list;

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
      image_url_list.Append(new base::StringValue(gaia_picture_url_));
    }
  }

  // Next add the default avatar icons.
  for (size_t i = 0; i < ProfileInfoCache::GetDefaultAvatarIconCount(); i++) {
    std::string url = ProfileInfoCache::GetDefaultAvatarIconUrl(i);
    image_url_list.Append(new base::StringValue(url));
  }

  web_ui()->CallJavascriptFunction(
      "ManageProfileOverlay.receiveDefaultProfileIcons", icon_grid,
      image_url_list);
}

void ManageProfileHandler::SendProfileNames() {
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  DictionaryValue profile_name_dict;
  for (size_t i = 0, e = cache.GetNumberOfProfiles(); i < e; ++i)
    profile_name_dict.SetBoolean(UTF16ToUTF8(cache.GetNameOfProfileAtIndex(i)),
                                 true);

  web_ui()->CallJavascriptFunction("ManageProfileOverlay.receiveProfileNames",
                                   profile_name_dict);
}

void ManageProfileHandler::SetProfileNameAndIcon(const ListValue* args) {
  DCHECK(args);

  base::FilePath profile_file_path;
  if (!GetProfilePathFromArgs(args, &profile_file_path))
    return;

  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_file_path);
  if (profile_index == std::string::npos)
    return;

  Profile* profile =
      g_browser_process->profile_manager()->GetProfile(profile_file_path);
  if (!profile)
    return;

  string16 new_profile_name;
  if (!args->GetString(1, &new_profile_name))
    return;
  if (new_profile_name == cache.GetGAIANameOfProfileAtIndex(profile_index)) {
    // Set the profile to use the GAIA name as the profile name. Note, this
    // is a little weird if the user typed their GAIA name manually but
    // it's not a big deal.
    cache.SetIsUsingGAIANameOfProfileAtIndex(profile_index, true);
    // Using the GAIA name as the profile name can invalidate the profile index.
    profile_index = cache.GetIndexOfProfileWithPath(profile_file_path);
    if (profile_index == std::string::npos)
      return;
  } else {
    PrefService* pref_service = profile->GetPrefs();
    // Updating the profile preference will cause the cache to be updated for
    // this preference.
    pref_service->SetString(prefs::kProfileName, UTF16ToUTF8(new_profile_name));

    // Changing the profile name can invalidate the profile index.
    profile_index = cache.GetIndexOfProfileWithPath(profile_file_path);
    if (profile_index == std::string::npos)
      return;

    cache.SetIsUsingGAIANameOfProfileAtIndex(profile_index, false);
    // Unsetting the GAIA name as the profile name can invalidate the profile
    // index.
    profile_index = cache.GetIndexOfProfileWithPath(profile_file_path);
    if (profile_index == std::string::npos)
      return;
  }

  std::string icon_url;
  if (!args->GetString(2, &icon_url))
    return;

  // Metrics logging variable.
  bool previously_using_gaia_icon =
      cache.IsUsingGAIANameOfProfileAtIndex(profile_index);

  size_t new_icon_index;
  if (icon_url == gaia_picture_url_) {
    cache.SetIsUsingGAIAPictureOfProfileAtIndex(profile_index, true);
    if (!previously_using_gaia_icon) {
      // Only log if they changed to the GAIA photo.
      // Selection of GAIA photo as avatar is logged as part of the function
      // below.
      ProfileMetrics::LogProfileSwitchGaia(ProfileMetrics::GAIA_OPT_IN);
    }
  } else if (cache.IsDefaultAvatarIconUrl(icon_url, &new_icon_index)) {
    ProfileMetrics::LogProfileAvatarSelection(new_icon_index);
    PrefService* pref_service = profile->GetPrefs();
    // Updating the profile preference will cause the cache to be updated for
    // this preference.
    pref_service->SetInteger(prefs::kProfileAvatarIndex, new_icon_index);
    cache.SetIsUsingGAIAPictureOfProfileAtIndex(profile_index, false);
  }
  ProfileMetrics::LogProfileUpdate(profile_file_path);
}

void ManageProfileHandler::DeleteProfile(const ListValue* args) {
  DCHECK(args);
#if defined(ENABLE_MANAGED_USERS)
  // This handler could have been called in managed mode, for example because
  // the user fiddled with the web inspector. Silently return in this case.
  ManagedUserService* service =
      ManagedUserServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  if (service->ProfileIsManaged())
    return;
#endif

  if (!ProfileManager::IsMultipleProfilesEnabled())
    return;

  ProfileMetrics::LogProfileDeleteUser(ProfileMetrics::PROFILE_DELETED);

  base::FilePath profile_file_path;
  if (!GetProfilePathFromArgs(args, &profile_file_path))
    return;

  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  chrome::HostDesktopType desktop_type = chrome::HOST_DESKTOP_TYPE_NATIVE;
  if (browser)
    desktop_type = browser->host_desktop_type();

  g_browser_process->profile_manager()->ScheduleProfileForDeletion(
      profile_file_path, desktop_type);
}

#if defined(ENABLE_SETTINGS_APP)
void ManageProfileHandler::SwitchAppListProfile(const ListValue* args) {
  DCHECK(args);
  DCHECK(ProfileManager::IsMultipleProfilesEnabled());

  const Value* file_path_value;
  base::FilePath profile_file_path;
  if (!args->Get(0, &file_path_value) ||
      !base::GetValueAsFilePath(*file_path_value, &profile_file_path))
    return;

  AppListService::Get()->SetAppListProfile(profile_file_path);
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

  // If the selection is the GAIA picture then also show the GAIA name in the
  // text field.
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_file_path);
  if (profile_index == std::string::npos)
    return;
  string16 gaia_name = cache.GetGAIANameOfProfileAtIndex(profile_index);
  if (gaia_name.empty())
    return;

  StringValue gaia_name_value(gaia_name);
  web_ui()->CallJavascriptFunction("ManageProfileOverlay.setProfileName",
                                   gaia_name_value);
}

void ManageProfileHandler::RequestHasProfileShortcuts(const ListValue* args) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(ProfileShortcutManager::IsFeatureEnabled());

  base::FilePath profile_file_path;
  if (!GetProfilePathFromArgs(args, &profile_file_path))
    return;

  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_file_path);
  if (profile_index == std::string::npos)
    return;

  const base::FilePath profile_path =
      cache.GetPathOfProfileAtIndex(profile_index);
  ProfileShortcutManager* shortcut_manager =
      g_browser_process->profile_manager()->profile_shortcut_manager();
  shortcut_manager->HasProfileShortcuts(
      profile_path, base::Bind(&ManageProfileHandler::OnHasProfileShortcuts,
                               weak_factory_.GetWeakPtr()));
}

void ManageProfileHandler::OnHasProfileShortcuts(bool has_shortcuts) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

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

}  // namespace options
