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

void ManagedUserPassphraseHandler::SetLocalPassphrase(
    const base::ListValue* args) {
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
