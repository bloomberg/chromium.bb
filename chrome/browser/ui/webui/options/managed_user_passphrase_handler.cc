// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/managed_user_passphrase_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/managed_mode/managed_user_passphrase.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/managed_user_passphrase_dialog.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"

namespace options {

ManagedUserPassphraseHandler::ManagedUserPassphraseHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

ManagedUserPassphraseHandler::~ManagedUserPassphraseHandler() {
}

void ManagedUserPassphraseHandler::InitializeHandler() {
}

void ManagedUserPassphraseHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "setPassphrase",
      base::Bind(&ManagedUserPassphraseHandler::SetLocalPassphrase,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setElevated",
      base::Bind(&ManagedUserPassphraseHandler::SetElevated,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "isPassphraseSet",
      base::Bind(&ManagedUserPassphraseHandler::IsPassphraseSet,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "resetPassphrase",
      base::Bind(&ManagedUserPassphraseHandler::ResetPassphrase,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ManagedUserPassphraseHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "confirmPassphrase", IDS_CONFIRM_PASSPHRASE_LABEL },
    { "enterPassphrase", IDS_ENTER_PASSPHRASE_LABEL },
    { "savePassphrase", IDS_SAVE_PASSPHRASE_BUTTON },
    { "setPassphraseInstructions", IDS_SET_PASSPHRASE_INSTRUCTIONS },
    { "passphraseMismatch", IDS_PASSPHRASE_MISMATCH },
  };
  RegisterStrings(localized_strings, resources, arraysize(resources));

  RegisterTitle(localized_strings,
                "setPassphraseTitle",
                IDS_SET_PASSPHRASE_TITLE);
}

void ManagedUserPassphraseHandler::PassphraseDialogCallback(bool success) {
  base::FundamentalValue unlock_success(success);
  web_ui()->CallJavascriptFunction("ManagedUserSettings.isAuthenticated",
                                   unlock_success);
}

void ManagedUserPassphraseHandler::SetElevated(
    const base::ListValue* args) {
  bool elevated;
  args->GetBoolean(0, &elevated);

  Profile* profile = Profile::FromWebUI(web_ui());
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile);
  if (!elevated) {
    managed_user_service->SetElevated(false);
    return;
  }
  if (managed_user_service->IsElevated()) {
    // If the custodian is already authenticated, skip the passphrase dialog.
    PassphraseDialogCallback(true);
    return;
  }
  // Is deleted automatically when the dialog is closed.
  new ManagedUserPassphraseDialog(
      web_ui()->GetWebContents(),
      base::Bind(&ManagedUserPassphraseHandler::PassphraseDialogCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ManagedUserPassphraseHandler::IsPassphraseSet(
    const base::ListValue* args) {
  // Get the name of the callback function.
  std::string callback_function_name;
  args->GetString(0, &callback_function_name);
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  base::FundamentalValue is_passphrase_set(!pref_service->GetString(
      prefs::kManagedModeLocalPassphrase).empty());
  web_ui()->CallJavascriptFunction(callback_function_name,
                                   is_passphrase_set);
}

void ManagedUserPassphraseHandler::ResetPassphrase(
    const base::ListValue* args) {
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  pref_service->SetString(prefs::kManagedModeLocalPassphrase, "");
  pref_service->SetString(prefs::kManagedModeLocalSalt, "");
}

void ManagedUserPassphraseHandler::SetLocalPassphrase(
    const base::ListValue* args) {
  // Only change the passphrase if the custodian is authenticated.
  Profile* profile = Profile::FromWebUI(web_ui());
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile);
  if (!managed_user_service->IsElevated())
    return;

  std::string passphrase;
  args->GetString(0, &passphrase);
  ManagedUserPassphrase passphrase_key_generator((std::string()));
  std::string encoded_passphrase_hash;
  bool success = passphrase_key_generator.GenerateHashFromPassphrase(
      passphrase,
      &encoded_passphrase_hash);
  if (success) {
    PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
    pref_service->SetString(prefs::kManagedModeLocalPassphrase,
                            encoded_passphrase_hash);
    pref_service->SetString(prefs::kManagedModeLocalSalt,
                            passphrase_key_generator.GetSalt());
  }
  // TODO(akuegel): Give failure back to the UI.
}

}  // namespace options
