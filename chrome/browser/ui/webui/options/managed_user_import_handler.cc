// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/managed_user_import_handler.h"

#include <set>

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"
#include "chrome/browser/managed_mode/managed_user_sync_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

scoped_ptr<base::ListValue> GetAvatarIcons() {
  scoped_ptr<base::ListValue> avatar_icons(new base::ListValue);
  for (size_t i = 0; i < ProfileInfoCache::GetDefaultAvatarIconCount(); ++i) {
    std::string avatar_url = ProfileInfoCache::GetDefaultAvatarIconUrl(i);
    avatar_icons->Append(new base::StringValue(avatar_url));
  }

  return avatar_icons.Pass();
}

}  // namespace

namespace options {

ManagedUserImportHandler::ManagedUserImportHandler()
    : weak_ptr_factory_(this) {
}

ManagedUserImportHandler::~ManagedUserImportHandler() {}

void ManagedUserImportHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
      { "managedUserImportTitle", IDS_IMPORT_EXISTING_MANAGED_USER_TITLE },
      { "managedUserImportText", IDS_IMPORT_EXISTING_MANAGED_USER_TEXT },
      { "createNewUserLink", IDS_CREATE_NEW_USER_LINK },
      { "managedUserImportOk", IDS_IMPORT_EXISTING_MANAGED_USER_OK },
      { "managedUserAlreadyOnThisDevice",
          IDS_MANAGED_USER_ALREADY_ON_THIS_DEVICE },
      { "noExistingManagedUsers", IDS_MANAGED_USER_NO_EXISTING_ERROR },
      { "managedUserSelectAvatarTitle", IDS_MANAGED_USER_SELECT_AVATAR_TITLE },
      { "managedUserSelectAvatarText", IDS_MANAGED_USER_SELECT_AVATAR_TEXT },
      { "managedUserSelectAvatarOk", IDS_MANAGED_USER_SELECT_AVATAR_OK },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  localized_strings->Set("avatarIcons", GetAvatarIcons().release());
}

void ManagedUserImportHandler::InitializeHandler() {
  registrar_.Add(this, chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED,
                 content::Source<Profile>(Profile::FromWebUI(web_ui())));
}

void ManagedUserImportHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestManagedUserImportUpdate",
      base::Bind(&ManagedUserImportHandler::RequestManagedUserImportUpdate,
                 base::Unretained(this)));
}

void ManagedUserImportHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED, type);
  RequestManagedUserImportUpdate(NULL);
}

void ManagedUserImportHandler::RequestManagedUserImportUpdate(
    const base::ListValue* args) {
  if (Profile::FromWebUI(web_ui())->IsManaged())
    return;

  if (!IsAccountConnected() || HasAuthError()) {
    ClearManagedUsersAndShowError();
  } else {
    // Account connected and no sign-in errors, then hide
    // any error messages and send the managed users to update
    // the managed user list.
    web_ui()->CallJavascriptFunction(
        "ManagedUserImportOverlay.hideErrorBubble");

    ManagedUserSyncService* managed_user_sync_service =
        ManagedUserSyncServiceFactory::GetForProfile(
            Profile::FromWebUI(web_ui()));
    managed_user_sync_service->GetManagedUsersAsync(
        base::Bind(&ManagedUserImportHandler::SendExistingManagedUsers,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ManagedUserImportHandler::SendExistingManagedUsers(
    const DictionaryValue* dict) {
  DCHECK(dict);
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  std::set<std::string> managed_user_ids;
  for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i)
    managed_user_ids.insert(cache.GetManagedUserIdOfProfileAtIndex(i));

  ListValue managed_users;
  for (DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    const DictionaryValue* value = NULL;
    bool success = it.value().GetAsDictionary(&value);
    DCHECK(success);
    std::string name;
    value->GetString(ManagedUserSyncService::kName, &name);
    std::string avatar_str;
    value->GetString(ManagedUserSyncService::kChromeAvatar, &avatar_str);

    DictionaryValue* managed_user = new DictionaryValue;
    managed_user->SetString("id", it.key());
    managed_user->SetString("name", name);

    int avatar_index = ManagedUserSyncService::kNoAvatar;
    success = ManagedUserSyncService::GetAvatarIndex(avatar_str, &avatar_index);
    DCHECK(success);
    managed_user->SetBoolean("needAvatar",
                             avatar_index == ManagedUserSyncService::kNoAvatar);

    std::string supervised_user_icon =
        std::string(chrome::kChromeUIThemeURL) +
        "IDR_SUPERVISED_USER_PLACEHOLDER";
    std::string avatar_url =
        avatar_index == ManagedUserSyncService::kNoAvatar ?
            supervised_user_icon :
            ProfileInfoCache::GetDefaultAvatarIconUrl(avatar_index);
    managed_user->SetString("iconURL", avatar_url);
    bool on_current_device =
        managed_user_ids.find(it.key()) != managed_user_ids.end();
    managed_user->SetBoolean("onCurrentDevice", on_current_device);

    managed_users.Append(managed_user);
  }

  web_ui()->CallJavascriptFunction(
      "ManagedUserImportOverlay.receiveExistingManagedUsers",
      managed_users);
}

void ManagedUserImportHandler::ClearManagedUsersAndShowError() {
  web_ui()->CallJavascriptFunction(
      "ManagedUserImportOverlay.receiveExistingManagedUsers");
  string16 error_message =
      l10n_util::GetStringUTF16(IDS_MANAGED_USER_IMPORT_SIGN_IN_ERROR);
  web_ui()->CallJavascriptFunction("ManagedUserImportOverlay.onError",
                                   base::StringValue(error_message));
}

bool ManagedUserImportHandler::IsAccountConnected() const {
  Profile* profile = Profile::FromWebUI(web_ui());
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  return !signin_manager->GetAuthenticatedUsername().empty();
}

bool ManagedUserImportHandler::HasAuthError() const {
  Profile* profile = Profile::FromWebUI(web_ui());
  SigninGlobalError* signin_global_error =
      SigninGlobalError::GetForProfile(profile);
  GoogleServiceAuthError::State state =
      signin_global_error->GetLastAuthError().state();

  return state == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
      state == GoogleServiceAuthError::USER_NOT_SIGNED_UP ||
      state == GoogleServiceAuthError::ACCOUNT_DELETED ||
      state == GoogleServiceAuthError::ACCOUNT_DISABLED;
}

}  // namespace options
