// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/managed_user_passphrase_dialog.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/managed_mode/managed_user_passphrase.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/gfx/size.h"

namespace {

// Handles the message when the user entered a passphrase and clicks the Unlock
// button.
class ManagedUserPassphraseDialogMessageHandler
    : public content::WebUIMessageHandler {

 public:
  ManagedUserPassphraseDialogMessageHandler();
  virtual ~ManagedUserPassphraseDialogMessageHandler() {}

  // content::WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Gets called from the UI with the entered passphrase as a parameter. The
  // correctness of the passphrase is checked and the result is returned to the
  // UI.
  void CheckPassphrase(const base::ListValue* args) const;

  base::WeakPtrFactory<ManagedUserPassphraseDialogMessageHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserPassphraseDialogMessageHandler);
};

ManagedUserPassphraseDialogMessageHandler
    ::ManagedUserPassphraseDialogMessageHandler() : weak_factory_(this) {
}

void ManagedUserPassphraseDialogMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "checkPassphrase",
      base::Bind(&ManagedUserPassphraseDialogMessageHandler::CheckPassphrase,
                 weak_factory_.GetWeakPtr()));
}

void ManagedUserPassphraseDialogMessageHandler::CheckPassphrase(
    const base::ListValue* args) const {
  // Extract the passphrase from the provided ListValue parameter.
  const base::Value* passphrase_arg = NULL;
  args->Get(0, &passphrase_arg);
  std::string passphrase;
  passphrase_arg->GetAsString(&passphrase);

  // Get the hashed passphrase and the salt that was used to calculate it.
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* pref_service = profile->GetPrefs();
  std::string stored_passphrase_hash =
      pref_service->GetString(prefs::kManagedModeLocalPassphrase);
  std::string salt = pref_service->GetString(prefs::kManagedModeLocalSalt);

  // Calculate the hash of the entered passphrase.
  ManagedUserPassphrase passphrase_key_generator(salt);
  std::string encoded_passphrase_hash;
  passphrase_key_generator.GenerateHashFromPassphrase(passphrase,
                                                      &encoded_passphrase_hash);

  // Check if the entered passphrase is correct and possibly set the managed
  // user into the elevated state which for example allows to modify settings.
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile);
  if (stored_passphrase_hash == encoded_passphrase_hash) {
    managed_user_service->SetElevated(true);
    web_ui()->CallJavascriptFunction("passphraseCorrect");
  } else {
    managed_user_service->SetElevated(false);
    web_ui()->CallJavascriptFunction("passphraseIncorrect");
  }
}

}  // namespace

ManagedUserPassphraseDialog::ManagedUserPassphraseDialog(
    content::WebContents* web_contents,
    const PassphraseCheckedCallback& callback) : callback_(callback) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  CreateDataSource(profile);
  CreateConstrainedWebDialog(profile, this, NULL, web_contents);
}


ui::ModalType ManagedUserPassphraseDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

string16 ManagedUserPassphraseDialog::GetDialogTitle() const {
  return string16();
}

GURL ManagedUserPassphraseDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIManagedUserPassphrasePageURL);
}

void ManagedUserPassphraseDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {
  DCHECK(handlers);
  // The constrained window delegate takes care of registering the handler.
  // The handler is also deleted automatically.
  handlers->push_back(new ManagedUserPassphraseDialogMessageHandler());
}

void ManagedUserPassphraseDialog::GetDialogSize(gfx::Size* size) const {
  const int kDialogWidth = 400;
  const int kDialogHeight = 310;
  size->SetSize(kDialogWidth, kDialogHeight);
}

std::string ManagedUserPassphraseDialog::GetDialogArgs() const {
  return std::string();
}

void ManagedUserPassphraseDialog::OnDialogClosed(
    const std::string& json_retval) {
  if (!callback_.is_null()) {
    callback_.Run(!json_retval.empty());
    callback_.Reset();
  }
}

void ManagedUserPassphraseDialog::OnCloseContents(
    content::WebContents* source, bool* out_close_dialog) {
}

bool ManagedUserPassphraseDialog::ShouldShowDialogTitle() const {
  return false;
}

ManagedUserPassphraseDialog::~ManagedUserPassphraseDialog() {
}

void ManagedUserPassphraseDialog::CreateDataSource(Profile* profile) const {
  content::WebUIDataSource* data_source = content::WebUIDataSource::Create(
      chrome::kChromeUIManagedUserPassphrasePageHost);
  data_source->SetDefaultResource(IDR_MANAGED_USER_PASSPHRASE_DIALOG_HTML);
  data_source->AddResourcePath("managed_user_passphrase_dialog.js",
                               IDR_MANAGED_USER_PASSPHRASE_DIALOG_JS);
  data_source->AddResourcePath("managed_user_passphrase_dialog.css",
                               IDR_MANAGED_USER_PASSPHRASE_DIALOG_CSS);
  data_source->AddResourcePath("managed_user.png",
                               IDR_MANAGED_USER_PASSPHRASE_DIALOG_IMG);
  data_source->AddLocalizedString("managedModePassphrasePage",
                                  IDS_PASSPHRASE_TITLE);
  data_source->AddLocalizedString("unlockPassphraseButton",
                                  IDS_UNLOCK_PASSPHRASE_BUTTON);
  data_source->AddLocalizedString("passphraseInstruction",
                                  IDS_PASSPHRASE_INSTRUCTION);
  data_source->AddLocalizedString("incorrectPassphraseWarning",
                                  IDS_INCORRECT_PASSPHRASE_WARNING);
  data_source->AddLocalizedString("cancelPassphraseButton", IDS_CANCEL);
  data_source->SetJsonPath("strings.js");
  data_source->SetUseJsonJSFormatV2();
  content::WebUIDataSource::Add(profile, data_source);
}
