// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_supervised_user_import_handler.h"

#include <stddef.h>

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/referrer.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"


SigninSupervisedUserImportHandler::SigninSupervisedUserImportHandler()
    : weak_ptr_factory_(this) {
}

SigninSupervisedUserImportHandler::~SigninSupervisedUserImportHandler() {
}

void SigninSupervisedUserImportHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("supervisedUserImportTitle",
      l10n_util::GetStringUTF16(
          IDS_IMPORT_EXISTING_LEGACY_SUPERVISED_USER_TITLE));
  localized_strings->SetString("supervisedUserImportText",
      l10n_util::GetStringUTF16(
          IDS_IMPORT_EXISTING_LEGACY_SUPERVISED_USER_TEXT));
  localized_strings->SetString("noSupervisedUserImportText",
      l10n_util::GetStringUTF16(IDS_IMPORT_NO_EXISTING_SUPERVISED_USER_TEXT));
  localized_strings->SetString("supervisedUserImportOk",
      l10n_util::GetStringUTF16(IDS_IMPORT_EXISTING_LEGACY_SUPERVISED_USER_OK));
  localized_strings->SetString("supervisedUserAlreadyOnThisDevice",
      l10n_util::GetStringUTF16(
          IDS_LEGACY_SUPERVISED_USER_ALREADY_ON_THIS_DEVICE));
}

void SigninSupervisedUserImportHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("getExistingSupervisedUsers",
      base::Bind(&SigninSupervisedUserImportHandler::GetExistingSupervisedUsers,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openUrlInLastActiveProfileBrowser",
      base::Bind(
          &SigninSupervisedUserImportHandler::OpenUrlInLastActiveProfileBrowser,
          base::Unretained(this)));
}

void SigninSupervisedUserImportHandler::AssignWebUICallbackId(
    const base::ListValue* args) {
  CHECK_LE(1U, args->GetSize());
  CHECK(webui_callback_id_.empty());
  CHECK(args->GetString(0, &webui_callback_id_));
}

void SigninSupervisedUserImportHandler::OpenUrlInLastActiveProfileBrowser(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string url;
  bool success = args->GetString(0, &url);
  DCHECK(success);
  content::OpenURLParams params(GURL(url),
                                content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                false);
  // Get the browser owned by the last used profile.
  Browser* browser =
      chrome::FindLastActiveWithProfile(ProfileManager::GetLastUsedProfile());
  DCHECK(browser);
  if (browser)
    browser->OpenURL(params);
}

void SigninSupervisedUserImportHandler::GetExistingSupervisedUsers(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  AssignWebUICallbackId(args);

  base::FilePath custodian_profile_path;
  const base::Value* profile_path_value;
  bool success = args->Get(1, &profile_path_value);
  DCHECK(success);
  success = base::GetValueAsFilePath(*profile_path_value,
                                     &custodian_profile_path);
  DCHECK(success);

  // Load custodian profile.
  g_browser_process->profile_manager()->CreateProfileAsync(
      custodian_profile_path,
      base::Bind(
          &SigninSupervisedUserImportHandler::LoadCustodianProfileCallback,
          weak_ptr_factory_.GetWeakPtr()),
      base::string16(), std::string(), std::string());
}

void SigninSupervisedUserImportHandler::LoadCustodianProfileCallback(
    Profile* profile, Profile::CreateStatus status) {

  // This method gets called once before with Profile::CREATE_STATUS_CREATED.
  switch (status) {
    case Profile::CREATE_STATUS_LOCAL_FAIL: {
      // TODO(mahmadi): see if a better error message is required here.
      RejectCallback(GetLocalErorrMessage());
      break;
    }
    case Profile::CREATE_STATUS_CREATED: {
      // Ignore the intermediate status.
      break;
    }
    case Profile::CREATE_STATUS_INITIALIZED: {
      // We are only interested in Profile::CREATE_STATUS_INITIALIZED when
      // everything is ready.
      if (profile->IsSupervised()) {
        webui_callback_id_.clear();
        return;
      }

      if (!IsAccountConnected(profile) || HasAuthError(profile)) {
        RejectCallback(GetAuthErorrMessage());
        return;
      }

      SupervisedUserSyncService* supervised_user_sync_service =
          SupervisedUserSyncServiceFactory::GetForProfile(profile);
      if (supervised_user_sync_service) {
        supervised_user_sync_service->GetSupervisedUsersAsync(
            base::Bind(
                &SigninSupervisedUserImportHandler::SendExistingSupervisedUsers,
                weak_ptr_factory_.GetWeakPtr(), profile));
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

void SigninSupervisedUserImportHandler::RejectCallback(
    const base::string16& error) {
  web_ui()->CallJavascriptFunction("cr.webUIResponse",
      base::StringValue(webui_callback_id_),
      base::FundamentalValue(false),
      base::StringValue(error));
  webui_callback_id_.clear();
}

base::string16 SigninSupervisedUserImportHandler::GetLocalErorrMessage() const {
  return l10n_util::GetStringUTF16(
      IDS_LEGACY_SUPERVISED_USER_IMPORT_LOCAL_ERROR);
}

base::string16 SigninSupervisedUserImportHandler::GetAuthErorrMessage() const {
  return l10n_util::GetStringUTF16(
      IDS_PROFILES_CREATE_CUSTODIAN_ACCOUNT_DETAILS_OUT_OF_DATE_ERROR);
}

void SigninSupervisedUserImportHandler::SendExistingSupervisedUsers(
    Profile* profile,
    const base::DictionaryValue* dict) {
  DCHECK(dict);
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetAllProfilesAttributes();

  // Collect the ids of local supervised user profiles.
  std::set<std::string> supervised_user_ids;
  for (auto& entry : entries) {
    // Filter out omitted profiles. These are currently being imported, and
    // shouldn't show up as "already on this device" just yet.
    if (entry->IsLegacySupervised() && !entry->IsOmitted()) {
      supervised_user_ids.insert(entry->GetSupervisedUserId());
    }
  }

  base::ListValue supervised_users;
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

    std::string avatar_url =
        avatar_index == SupervisedUserSyncService::kNoAvatar ?
            profiles::GetDefaultAvatarIconUrl(
                profiles::GetPlaceholderAvatarIndex()) :
            profiles::GetDefaultAvatarIconUrl(avatar_index);
    supervised_user->SetString("iconURL", avatar_url);
    bool on_current_device =
        supervised_user_ids.find(it.key()) != supervised_user_ids.end();
    supervised_user->SetBoolean("onCurrentDevice", on_current_device);

    supervised_users.Append(supervised_user);
  }

  // Resolve callback with response.
  web_ui()->CallJavascriptFunction("cr.webUIResponse",
      base::StringValue(webui_callback_id_),
      base::FundamentalValue(true),
      supervised_users);
  webui_callback_id_.clear();
}

bool SigninSupervisedUserImportHandler::IsAccountConnected(
      Profile* profile) const {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  return signin_manager && signin_manager->IsAuthenticated();
}

bool SigninSupervisedUserImportHandler::HasAuthError(Profile* profile) const {
  SigninErrorController* error_controller =
      SigninErrorControllerFactory::GetForProfile(profile);
  if (!error_controller)
    return true;

  GoogleServiceAuthError::State state = error_controller->auth_error().state();
  return state != GoogleServiceAuthError::NONE;
}
