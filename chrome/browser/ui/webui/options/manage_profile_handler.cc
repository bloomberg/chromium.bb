// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/manage_profile_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"

ManageProfileHandler::ManageProfileHandler() {
}

ManageProfileHandler::~ManageProfileHandler() {
}

void ManageProfileHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "manageProfilesTitle", IDS_PROFILES_MANAGE_TITLE },
    { "manageProfilesNameLabel", IDS_PROFILES_MANAGE_NAME_LABEL },
    { "manageProfilesDuplicateNameError",
        IDS_PROFILES_MANAGE_DUPLICATE_NAME_ERROR },
    { "manageProfilesIconLabel", IDS_PROFILES_MANAGE_ICON_LABEL },
    { "deleteProfileTitle", IDS_PROFILES_DELETE_TITLE },
    { "deleteProfileOK", IDS_PROFILES_DELETE_OK_BUTTON_LABEL },
    { "deleteProfileMessage", IDS_PROFILES_DELETE_MESSAGE },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
}

void ManageProfileHandler::Initialize() {
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
                 content::NotificationService::AllSources());
  SendProfileNames();
}

void ManageProfileHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("setProfileNameAndIcon",
      base::Bind(&ManageProfileHandler::SetProfileNameAndIcon,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("deleteProfile",
      base::Bind(&ManageProfileHandler::DeleteProfile,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("requestDefaultProfileIcons",
      base::Bind(&ManageProfileHandler::RequestDefaultProfileIcons,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("requestProfileInfo",
      base::Bind(&ManageProfileHandler::RequestProfileInfo,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("profileIconSelectionChanged",
      base::Bind(&ManageProfileHandler::ProfileIconSelectionChanged,
                 base::Unretained(this)));
}

void ManageProfileHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED) {
    SendProfileNames();
    SendProfileIcons();
  } else {
    OptionsPageUIHandler::Observe(type, source, details);
  }
}

void ManageProfileHandler::RequestDefaultProfileIcons(const ListValue* args) {
  SendProfileIcons();
}

void ManageProfileHandler::SendProfileIcons() {
  ListValue image_url_list;

  // First add the GAIA picture if it's available.
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  Profile* profile = Profile::FromWebUI(web_ui_);
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  if (profile_index != std::string::npos) {
    const gfx::Image* icon =
        cache.GetGAIAPictureOfProfileAtIndex(profile_index);
    if (icon) {
      gfx::Image icon2 = profiles::GetAvatarIconForWebUI(*icon, true);
      gaia_picture_url_ = web_ui_util::GetImageDataUrl(icon2);
      image_url_list.Append(Value::CreateStringValue(gaia_picture_url_));
    }
  }

  // Next add the default avatar icons.
  for (size_t i = 0; i < ProfileInfoCache::GetDefaultAvatarIconCount(); i++) {
    std::string url = ProfileInfoCache::GetDefaultAvatarIconUrl(i);
    image_url_list.Append(Value::CreateStringValue(url));
  }

  web_ui_->CallJavascriptFunction(
      "ManageProfileOverlay.receiveDefaultProfileIcons",
      image_url_list);
}

void ManageProfileHandler::SendProfileNames() {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  DictionaryValue profile_name_dict;
  for (size_t i = 0, e = cache.GetNumberOfProfiles(); i < e; ++i)
    profile_name_dict.SetBoolean(UTF16ToUTF8(cache.GetNameOfProfileAtIndex(i)),
                                 true);

  web_ui_->CallJavascriptFunction("ManageProfileOverlay.receiveProfileNames",
                                  profile_name_dict);
}

void ManageProfileHandler::SetProfileNameAndIcon(const ListValue* args) {
  DCHECK(args);

  Value* file_path_value;
  FilePath profile_file_path;
  if (!args->Get(0, &file_path_value) ||
      !base::GetValueAsFilePath(*file_path_value, &profile_file_path))
    return;

  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_file_path);
  if (profile_index == std::string::npos)
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
    cache.SetNameOfProfileAtIndex(profile_index, new_profile_name);
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
    cache.SetAvatarIconOfProfileAtIndex(profile_index, new_icon_index);
    cache.SetIsUsingGAIAPictureOfProfileAtIndex(profile_index, false);
  }

  ProfileMetrics::LogProfileUpdate(profile_file_path);
}

void ManageProfileHandler::DeleteProfile(const ListValue* args) {
  DCHECK(args);

  ProfileMetrics::LogProfileDeleteUser(ProfileMetrics::PROFILE_DELETED);

  Value* file_path_value;
  FilePath profile_file_path;
  if (!args->Get(0, &file_path_value) ||
      !base::GetValueAsFilePath(*file_path_value, &profile_file_path))
    return;

  g_browser_process->profile_manager()->ScheduleProfileForDeletion(
      profile_file_path);
}

void ManageProfileHandler::RequestProfileInfo(const ListValue* args) {
  DCHECK(args);

  Value* index_value;
  double index_double;
  if (!args->Get(0, &index_value) || !index_value->GetAsDouble(&index_double))
    return;

  int index = static_cast<int>(index_double);
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  int profile_count = cache.GetNumberOfProfiles();
  if (index < 0 && index >= profile_count)
    return;

  FilePath profile_path = cache.GetPathOfProfileAtIndex(index);
  FilePath current_profile_path = Profile::FromWebUI(web_ui_)->GetPath();
  bool is_current_profile =
      profile_path == Profile::FromWebUI(web_ui_)->GetPath();

  DictionaryValue profile_value;
  profile_value.SetString("name", cache.GetNameOfProfileAtIndex(index));
  profile_value.Set("filePath", base::CreateFilePathValue(profile_path));
  profile_value.SetBoolean("isCurrentProfile", is_current_profile);

  bool is_gaia_picture =
      cache.IsUsingGAIAPictureOfProfileAtIndex(index) &&
      cache.GetGAIAPictureOfProfileAtIndex(index);
  if (is_gaia_picture) {
    gfx::Image icon = profiles::GetAvatarIconForWebUI(
        cache.GetAvatarIconOfProfileAtIndex(index), true);
    profile_value.SetString("iconURL", web_ui_util::GetImageDataUrl(icon));
  } else {
    size_t icon_index = cache.GetAvatarIconIndexOfProfileAtIndex(index);
    profile_value.SetString("iconURL",
                             cache.GetDefaultAvatarIconUrl(icon_index));
  }

  web_ui_->CallJavascriptFunction("ManageProfileOverlay.setProfileInfo",
                                  profile_value);

  // Ensure that we have the most up to date GAIA picture.
  if (is_current_profile) {
    GAIAInfoUpdateService* service =
        Profile::FromWebUI(web_ui_)->GetGAIAInfoUpdateService();
    if (service)
      service->Update();
  }
}

void ManageProfileHandler::ProfileIconSelectionChanged(
    const base::ListValue* args) {
  DCHECK(args);

  Value* file_path_value;
  FilePath file_path;
  if (!args->Get(0, &file_path_value) ||
      !base::GetValueAsFilePath(*file_path_value, &file_path)) {
    return;
  }

  // Currently this only supports editing the current profile's info.
  if (file_path != Profile::FromWebUI(web_ui_)->GetPath())
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
  size_t i = cache.GetIndexOfProfileWithPath(file_path);
  if (i == std::string::npos)
    return;
  string16 gaia_name = cache.GetGAIANameOfProfileAtIndex(i);
  if (gaia_name.empty())
    return;

  StringValue gaia_name_value(gaia_name);
  web_ui_->CallJavascriptFunction("ManageProfileOverlay.setProfileName",
                                  gaia_name_value);
}
