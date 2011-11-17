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
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
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
}

void ManageProfileHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED)
    SendProfileNames();
  else
    OptionsPageUIHandler::Observe(type, source, details);
}

void ManageProfileHandler::RequestDefaultProfileIcons(const ListValue* args) {
  ListValue image_url_list;
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

  cache.SetNameOfProfileAtIndex(profile_index, new_profile_name);
  // The index in the cache may have changed if a new name triggered an
  // alphabetical resort.
  profile_index = cache.GetIndexOfProfileWithPath(profile_file_path);
  if (profile_index == std::string::npos)
    return;

  string16 icon_url;
  size_t new_icon_index;
  if (!args->GetString(2, &icon_url) ||
      !cache.IsDefaultAvatarIconUrl(UTF16ToUTF8(icon_url), &new_icon_index))
    return;

  ProfileMetrics::LogProfileAvatarSelection(new_icon_index);
  cache.SetAvatarIconOfProfileAtIndex(profile_index, new_icon_index);

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

  DictionaryValue profile_value;

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

  FilePath current_profile_path =
      web_ui_->tab_contents()->browser_context()->GetPath();
  size_t icon_index = cache.GetAvatarIconIndexOfProfileAtIndex(index);
  FilePath profile_path = cache.GetPathOfProfileAtIndex(index);
  profile_value.SetString("name", cache.GetNameOfProfileAtIndex(index));
  profile_value.SetString("iconURL",
                           cache.GetDefaultAvatarIconUrl(icon_index));
  profile_value.Set("filePath", base::CreateFilePathValue(profile_path));
  profile_value.SetBoolean("isCurrentProfile",
                            profile_path == current_profile_path);

  web_ui_->CallJavascriptFunction("ManageProfileOverlay.setProfileInfo",
                                  profile_value);
}
