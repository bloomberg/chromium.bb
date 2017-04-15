// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/supervised_user_import_handler.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/web_ui.h"

namespace {

std::unique_ptr<base::ListValue> GetAvatarIcons() {
  std::unique_ptr<base::ListValue> avatar_icons(new base::ListValue);
  for (size_t i = 0; i < profiles::GetDefaultAvatarIconCount(); ++i) {
    std::string avatar_url = profiles::GetDefaultAvatarIconUrl(i);
    avatar_icons->AppendString(avatar_url);
  }

  return avatar_icons;
}

bool ProfileIsLegacySupervised(const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry;

  return g_browser_process->profile_manager()->GetProfileAttributesStorage().
      GetProfileAttributesWithPath(profile_path, &entry) &&
      entry->IsLegacySupervised();
}

}  // namespace

namespace options {

SupervisedUserImportHandler::SupervisedUserImportHandler()
    : profile_observer_(this),
      signin_error_observer_(this),
      supervised_user_sync_service_observer_(this),
      removed_profile_is_supervised_(false),
      weak_ptr_factory_(this) {
}

SupervisedUserImportHandler::~SupervisedUserImportHandler() {
}

void SupervisedUserImportHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
      { "supervisedUserImportTitle",
          IDS_IMPORT_EXISTING_LEGACY_SUPERVISED_USER_TITLE },
      { "supervisedUserImportText",
          IDS_IMPORT_EXISTING_LEGACY_SUPERVISED_USER_TEXT },
      { "createNewUserLink", IDS_CREATE_NEW_LEGACY_SUPERVISED_USER_LINK },
      { "supervisedUserImportOk",
          IDS_IMPORT_EXISTING_LEGACY_SUPERVISED_USER_OK },
      { "supervisedUserImportSigninError",
          IDS_LEGACY_SUPERVISED_USER_IMPORT_SIGN_IN_ERROR },
      { "supervisedUserAlreadyOnThisDevice",
          IDS_LEGACY_SUPERVISED_USER_ALREADY_ON_THIS_DEVICE },
      { "noExistingSupervisedUsers",
          IDS_LEGACY_SUPERVISED_USER_NO_EXISTING_ERROR },
      { "supervisedUserSelectAvatarTitle",
          IDS_LEGACY_SUPERVISED_USER_SELECT_AVATAR_TITLE },
      { "supervisedUserSelectAvatarText",
          IDS_LEGACY_SUPERVISED_USER_SELECT_AVATAR_TEXT },
      { "supervisedUserSelectAvatarOk",
          IDS_LEGACY_SUPERVISED_USER_SELECT_AVATAR_OK },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  localized_strings->Set("avatarIcons", GetAvatarIcons().release());
}

void SupervisedUserImportHandler::InitializeHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile->IsSupervised()) {
    profile_observer_.Add(
        &g_browser_process->profile_manager()->GetProfileAttributesStorage());
    SupervisedUserSyncService* sync_service =
        SupervisedUserSyncServiceFactory::GetForProfile(profile);
    if (sync_service) {
      supervised_user_sync_service_observer_.Add(sync_service);
      signin_error_observer_.Add(
          SigninErrorControllerFactory::GetForProfile(profile));
      SupervisedUserSharedSettingsService* settings_service =
          SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
              profile);
      subscription_ = settings_service->Subscribe(
          base::Bind(&SupervisedUserImportHandler::OnSharedSettingChanged,
                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      DCHECK(!SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
                 profile));
      DCHECK(!SigninErrorControllerFactory::GetForProfile(profile));
    }
  }
}

void SupervisedUserImportHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestSupervisedUserImportUpdate",
      base::Bind(&SupervisedUserImportHandler::
                      RequestSupervisedUserImportUpdate,
                 base::Unretained(this)));
}

void SupervisedUserImportHandler::OnProfileAdded(
    const base::FilePath& profile_path) {
  // When a supervised profile is added, re-send the list to update the
  // the "already on this device" status.
  if (ProfileIsLegacySupervised(profile_path))
    FetchSupervisedUsers();
}

void SupervisedUserImportHandler::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  DCHECK(!removed_profile_is_supervised_);
  // When a supervised profile is removed, re-send the list to update the
  // "already on this device" status. We can't do that right now because the
  // profile still exists, so defer to OnProfileWasRemoved.
  if (ProfileIsLegacySupervised(profile_path))
    removed_profile_is_supervised_ = true;
}

void SupervisedUserImportHandler::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  if (removed_profile_is_supervised_) {
    removed_profile_is_supervised_ = false;
    FetchSupervisedUsers();
  }
}

void SupervisedUserImportHandler::OnProfileIsOmittedChanged(
    const base::FilePath& profile_path) {
  if (ProfileIsLegacySupervised(profile_path))
    FetchSupervisedUsers();
}

void SupervisedUserImportHandler::OnSupervisedUsersChanged() {
  FetchSupervisedUsers();
}

void SupervisedUserImportHandler::FetchSupervisedUsers() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "options.SupervisedUserListData.resetPromise");
  RequestSupervisedUserImportUpdate(NULL);
}

void SupervisedUserImportHandler::RequestSupervisedUserImportUpdate(
    const base::ListValue* /* args */) {
  if (Profile::FromWebUI(web_ui())->IsSupervised())
    return;

  if (!IsAccountConnected() || HasAuthError()) {
    ClearSupervisedUsersAndShowError();
  } else {
    SupervisedUserSyncService* supervised_user_sync_service =
        SupervisedUserSyncServiceFactory::GetForProfile(
            Profile::FromWebUI(web_ui()));
    if (supervised_user_sync_service) {
      supervised_user_sync_service->GetSupervisedUsersAsync(
          base::Bind(&SupervisedUserImportHandler::SendExistingSupervisedUsers,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void SupervisedUserImportHandler::SendExistingSupervisedUsers(
    const base::DictionaryValue* dict) {
  DCHECK(dict);
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()->
      GetProfileAttributesStorage().GetAllProfilesAttributes();

  // Collect the ids of local supervised user profiles.
  std::set<std::string> supervised_user_ids;
  for (const ProfileAttributesEntry* entry : entries) {
    // Filter out omitted profiles. These are currently being imported, and
    // shouldn't show up as "already on this device" just yet.
    if (entry->IsLegacySupervised() && !entry->IsOmitted()) {
      supervised_user_ids.insert(entry->GetSupervisedUserId());
    }
  }

  base::ListValue supervised_users;
  Profile* profile = Profile::FromWebUI(web_ui());
  SupervisedUserSharedSettingsService* service =
      SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(profile);
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* value = NULL;
    bool success = it.value().GetAsDictionary(&value);
    DCHECK(success);
    std::string name;
    value->GetString(SupervisedUserSyncService::kName, &name);

    std::unique_ptr<base::DictionaryValue> supervised_user(
        new base::DictionaryValue);
    supervised_user->SetString("id", it.key());
    supervised_user->SetString("name", name);

    int avatar_index = SupervisedUserSyncService::kNoAvatar;
    const base::Value* avatar_index_value =
        service->GetValue(it.key(), supervised_users::kChromeAvatarIndex);
    if (avatar_index_value) {
      success = avatar_index_value->GetAsInteger(&avatar_index);
    } else {
      // Check if there is a legacy avatar index stored.
      std::string avatar_str;
      value->GetString(SupervisedUserSyncService::kChromeAvatar, &avatar_str);
      success =
          SupervisedUserSyncService::GetAvatarIndex(avatar_str, &avatar_index);
    }
    DCHECK(success);
    supervised_user->SetBoolean(
        "needAvatar",
        avatar_index == SupervisedUserSyncService::kNoAvatar);

    std::string supervised_user_icon =
        std::string(chrome::kChromeUIThemeURL) +
        "IDR_SUPERVISED_USER_PLACEHOLDER";
    std::string avatar_url =
        avatar_index == SupervisedUserSyncService::kNoAvatar ?
            supervised_user_icon :
            profiles::GetDefaultAvatarIconUrl(avatar_index);
    supervised_user->SetString("iconURL", avatar_url);
    bool on_current_device =
        supervised_user_ids.find(it.key()) != supervised_user_ids.end();
    supervised_user->SetBoolean("onCurrentDevice", on_current_device);

    supervised_users.Append(std::move(supervised_user));
  }

  web_ui()->CallJavascriptFunctionUnsafe(
      "options.SupervisedUserListData.receiveExistingSupervisedUsers",
      supervised_users);
}

void SupervisedUserImportHandler::ClearSupervisedUsersAndShowError() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "options.SupervisedUserListData.onSigninError");
}

bool SupervisedUserImportHandler::IsAccountConnected() const {
  Profile* profile = Profile::FromWebUI(web_ui());
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  return signin_manager && signin_manager->IsAuthenticated();
}

bool SupervisedUserImportHandler::HasAuthError() const {
  Profile* profile = Profile::FromWebUI(web_ui());
  SigninErrorController* error_controller =
      SigninErrorControllerFactory::GetForProfile(profile);
  if (!error_controller)
    return true;

  GoogleServiceAuthError::State state = error_controller->auth_error().state();

  return state == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
      state == GoogleServiceAuthError::USER_NOT_SIGNED_UP ||
      state == GoogleServiceAuthError::ACCOUNT_DELETED ||
      state == GoogleServiceAuthError::ACCOUNT_DISABLED;
}

void SupervisedUserImportHandler::OnSharedSettingChanged(
    const std::string& supervised_user_id,
    const std::string& key) {
  if (key == supervised_users::kChromeAvatarIndex)
    FetchSupervisedUsers();
}

void SupervisedUserImportHandler::OnErrorChanged() {
  FetchSupervisedUsers();
}

}  // namespace options
