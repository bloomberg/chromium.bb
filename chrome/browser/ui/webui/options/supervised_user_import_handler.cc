// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/supervised_user_import_handler.h"

#include <set>

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/web_ui.h"
#include "grit/theme_resources.h"

namespace {

scoped_ptr<base::ListValue> GetAvatarIcons() {
  scoped_ptr<base::ListValue> avatar_icons(new base::ListValue);
  for (size_t i = 0; i < profiles::GetDefaultAvatarIconCount(); ++i) {
    std::string avatar_url = profiles::GetDefaultAvatarIconUrl(i);
    avatar_icons->Append(new base::StringValue(avatar_url));
  }

  return avatar_icons.Pass();
}

}  // namespace

namespace options {

SupervisedUserImportHandler::SupervisedUserImportHandler()
    : observer_(this),
      weak_ptr_factory_(this) {}

SupervisedUserImportHandler::~SupervisedUserImportHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile->IsSupervised()) {
    SupervisedUserSyncService* service =
        SupervisedUserSyncServiceFactory::GetForProfile(profile);
    if (service)
      service->RemoveObserver(this);
    subscription_.reset();
  }
}

void SupervisedUserImportHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
      { "supervisedUserImportTitle",
          IDS_IMPORT_EXISTING_SUPERVISED_USER_TITLE },
      { "supervisedUserImportText", IDS_IMPORT_EXISTING_SUPERVISED_USER_TEXT },
      { "createNewUserLink", IDS_CREATE_NEW_USER_LINK },
      { "supervisedUserImportOk", IDS_IMPORT_EXISTING_SUPERVISED_USER_OK },
      { "supervisedUserImportSigninError",
          IDS_SUPERVISED_USER_IMPORT_SIGN_IN_ERROR },
      { "supervisedUserAlreadyOnThisDevice",
          IDS_SUPERVISED_USER_ALREADY_ON_THIS_DEVICE },
      { "noExistingSupervisedUsers", IDS_SUPERVISED_USER_NO_EXISTING_ERROR },
      { "supervisedUserSelectAvatarTitle",
          IDS_SUPERVISED_USER_SELECT_AVATAR_TITLE },
      { "supervisedUserSelectAvatarText",
          IDS_SUPERVISED_USER_SELECT_AVATAR_TEXT },
      { "supervisedUserSelectAvatarOk", IDS_SUPERVISED_USER_SELECT_AVATAR_OK },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  localized_strings->Set("avatarIcons", GetAvatarIcons().release());
}

void SupervisedUserImportHandler::InitializeHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile->IsSupervised()) {
    SupervisedUserSyncService* sync_service =
        SupervisedUserSyncServiceFactory::GetForProfile(profile);
    if (sync_service) {
      sync_service->AddObserver(this);
      observer_.Add(ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->
                        signin_error_controller());
      SupervisedUserSharedSettingsService* settings_service =
          SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
              profile);
      subscription_ = settings_service->Subscribe(
          base::Bind(&SupervisedUserImportHandler::OnSharedSettingChanged,
                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      DCHECK(!SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
                 profile));
      DCHECK(!ProfileOAuth2TokenServiceFactory::GetForProfile(profile));
    }
  }
}

void SupervisedUserImportHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestSupervisedUserImportUpdate",
      base::Bind(&SupervisedUserImportHandler::
                      RequestSupervisedUserImportUpdate,
                 base::Unretained(this)));
}

void SupervisedUserImportHandler::OnSupervisedUsersChanged() {
  FetchSupervisedUsers();
}

void SupervisedUserImportHandler::FetchSupervisedUsers() {
  web_ui()->CallJavascriptFunction(
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
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();

  // Collect the ids of local supervised user profiles.
  std::set<std::string> supervised_user_ids;
  for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i) {
    if (cache.ProfileIsSupervisedAtIndex(i))
      supervised_user_ids.insert(cache.GetSupervisedUserIdOfProfileAtIndex(i));
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

    base::DictionaryValue* supervised_user = new base::DictionaryValue;
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

    supervised_users.Append(supervised_user);
  }

  web_ui()->CallJavascriptFunction(
      "options.SupervisedUserListData.receiveExistingSupervisedUsers",
      supervised_users);
}

void SupervisedUserImportHandler::ClearSupervisedUsersAndShowError() {
  web_ui()->CallJavascriptFunction(
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
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  if (!token_service)
    return true;

  SigninErrorController* error_controller =
      token_service->signin_error_controller();

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
