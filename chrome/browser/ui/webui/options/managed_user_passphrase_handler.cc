// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/managed_user_passphrase_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"
#include "chrome/browser/managed_mode/managed_user_passphrase.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"

namespace options {

ManagedUserPassphraseHandler::ManagedUserPassphraseHandler()
    : weak_ptr_factory_(this) {
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
  ManagedModeNavigationObserver::FromWebContents(
      web_ui()->GetWebContents())->set_elevated(success);
  web_ui()->CallJavascriptFunction("ManagedUserSettings.setAuthenticated",
                                   base::FundamentalValue(success));
}

void ManagedUserPassphraseHandler::SetElevated(
    const base::ListValue* args) {
  bool elevated;
  bool success = args->GetBoolean(0, &elevated);
  DCHECK(success);

  Profile* profile = Profile::FromWebUI(web_ui());
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile);
  if (!elevated) {
    ManagedModeNavigationObserver::FromWebContents(
        web_ui()->GetWebContents())->set_elevated(false);
    return;
  }
  managed_user_service->RequestAuthorization(
      web_ui()->GetWebContents(),
      base::Bind(&ManagedUserPassphraseHandler::PassphraseDialogCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ManagedUserPassphraseHandler::SetLocalPassphrase(
    const base::ListValue* args) {
  // Only change the passphrase if the custodian is authenticated.
  if (!ManagedModeNavigationObserver::FromWebContents(
      web_ui()->GetWebContents())->is_elevated()) {
    return;
  }

  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  std::string passphrase;
  args->GetString(0, &passphrase);
  if (passphrase.empty()) {
    pref_service->SetString(prefs::kManagedModeLocalPassphrase, std::string());
    pref_service->SetString(prefs::kManagedModeLocalSalt, std::string());
    return;
  }

  ManagedUserPassphrase passphrase_key_generator((std::string()));
  std::string encoded_passphrase_hash;
  bool success = passphrase_key_generator.GenerateHashFromPassphrase(
      passphrase,
      &encoded_passphrase_hash);
  if (success) {
    pref_service->SetString(prefs::kManagedModeLocalPassphrase,
                            encoded_passphrase_hash);
    pref_service->SetString(prefs::kManagedModeLocalSalt,
                            passphrase_key_generator.GetSalt());
  }
  // TODO(akuegel): Give failure back to the UI.
}

}  // namespace options
