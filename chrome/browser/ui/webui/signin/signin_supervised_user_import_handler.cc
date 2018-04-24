// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_supervised_user_import_handler.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/referrer.h"
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
  web_ui()->RegisterMessageCallback(
      "getExistingSupervisedUsers",
      base::BindRepeating(
          &SigninSupervisedUserImportHandler::GetExistingSupervisedUsers,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openUrlInLastActiveProfileBrowser",
      base::BindRepeating(
          &SigninSupervisedUserImportHandler::OpenUrlInLastActiveProfileBrowser,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "authenticateCustodian",
      base::BindRepeating(
          &SigninSupervisedUserImportHandler::AuthenticateCustodian,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancelLoadingSupervisedUsers",
      base::BindRepeating(
          &SigninSupervisedUserImportHandler::HandleCancelLoadSupervisedUsers,
          base::Unretained(this)));
}

void SigninSupervisedUserImportHandler::AssignWebUICallbackId(
    const base::ListValue* args) {
  CHECK_LE(1U, args->GetSize());
  CHECK(webui_callback_id_.empty());
  CHECK(args->GetString(0, &webui_callback_id_));
  AllowJavascript();
}

void SigninSupervisedUserImportHandler::OpenUrlInLastActiveProfileBrowser(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string url;
  bool success = args->GetString(0, &url);
  DCHECK(success);
  content::OpenURLParams params(GURL(url), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  // ProfileManager::GetLastUsedProfile() will attempt to load the default
  // profile if there is no last used profile. If the default profile is not
  // fully loaded and initialized, it will attempt to do so synchronously.
  // Therefore we cannot use that method here. If the last used profile is not
  // loaded, we do nothing. This is an edge case and should not happen often.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath last_used_profile_dir =
      profile_manager->GetLastUsedProfileDir(profile_manager->user_data_dir());
  Profile* last_used_profile =
      profile_manager->GetProfileByPath(last_used_profile_dir);

  if (last_used_profile) {
    // Last used profile may be the Guest Profile.
    if (ProfileManager::IncognitoModeForced(last_used_profile))
      last_used_profile = last_used_profile->GetOffTheRecordProfile();

    // Get the browser owned by the last used profile or create a new one if
    // it doesn't exist.
    Browser* browser = chrome::FindLastActiveWithProfile(last_used_profile);
    if (!browser)
      browser = new Browser(
          Browser::CreateParams(Browser::TYPE_TABBED, last_used_profile, true));
    browser->OpenURL(params);
  }
}

void SigninSupervisedUserImportHandler::AuthenticateCustodian(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());

  std::string email;
  bool success = args->GetString(0, &email);
  DCHECK(success);

  UserManagerProfileDialog::ShowReauthDialog(
      web_ui()->GetWebContents()->GetBrowserContext(), email,
      signin_metrics::Reason::REASON_REAUTHENTICATION);
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

void SigninSupervisedUserImportHandler::HandleCancelLoadSupervisedUsers(
    const base::ListValue* args) {
  webui_callback_id_.clear();
}

void SigninSupervisedUserImportHandler::LoadCustodianProfileCallback(
    Profile* profile, Profile::CreateStatus status) {

  // This method gets called once before with Profile::CREATE_STATUS_CREATED.
  switch (status) {
    case Profile::CREATE_STATUS_LOCAL_FAIL: {
      RejectCallback(GetLocalErrorMessage());
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
        RejectCallback(GetAuthErrorMessage(profile));
        return;
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
  RejectJavascriptCallback(base::Value(webui_callback_id_), base::Value(error));
  webui_callback_id_.clear();
}

base::string16 SigninSupervisedUserImportHandler::GetLocalErrorMessage() const {
  return l10n_util::GetStringUTF16(
      IDS_LEGACY_SUPERVISED_USER_IMPORT_LOCAL_ERROR);
}

base::string16 SigninSupervisedUserImportHandler::GetAuthErrorMessage(
    Profile* profile) const {
  return l10n_util::GetStringFUTF16(
      IDS_PROFILES_CREATE_CUSTODIAN_ACCOUNT_DETAILS_OUT_OF_DATE_ERROR,
      base::ASCIIToUTF16(profile->GetProfileUserName()));
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
