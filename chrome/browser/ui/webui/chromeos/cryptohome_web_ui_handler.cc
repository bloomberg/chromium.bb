// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cryptohome_web_ui_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "crypto/nss_util.h"

using content::BrowserThread;

namespace {
void GetNSSUtilInfoOnIOThread(const base::Callback<
    void(bool, const std::string&, const std::string&)>& ui_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool is_tpm_ready = crypto::IsTPMTokenReady();
  std::string token_name;
  std::string user_pin;
  if (is_tpm_ready)
    crypto::GetTPMTokenInfo(&token_name, &user_pin);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(ui_callback, is_tpm_ready, token_name, user_pin));
}
}  // namespace

namespace chromeos {

CryptohomeWebUIHandler::CryptohomeWebUIHandler() : weak_ptr_factory_(this) {}

CryptohomeWebUIHandler::~CryptohomeWebUIHandler() {}

void CryptohomeWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "pageLoaded",
      base::Bind(&CryptohomeWebUIHandler::OnPageLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CryptohomeWebUIHandler::OnPageLoaded(const base::ListValue* args) {
  CryptohomeClient* cryptohome_client =
      DBusThreadManager::Get()->GetCryptohomeClient();

  cryptohome_client->IsMounted(GetCryptohomeBoolCallback("is-mounted"));
  cryptohome_client->TpmIsReady(GetCryptohomeBoolCallback("tpm-is-ready"));
  cryptohome_client->TpmIsEnabled(GetCryptohomeBoolCallback("tpm-is-enabled"));
  cryptohome_client->TpmIsOwned(GetCryptohomeBoolCallback("tpm-is-owned"));
  cryptohome_client->TpmIsBeingOwned(
      GetCryptohomeBoolCallback("tpm-is-being-owned"));
  cryptohome_client->Pkcs11IsTpmTokenReady(
      GetCryptohomeBoolCallback("pkcs11-is-tpm-token-ready"));

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &GetNSSUtilInfoOnIOThread,
          base::Bind(&CryptohomeWebUIHandler::DidGetNSSUtilInfoOnUIThread,
                     weak_ptr_factory_.GetWeakPtr())));
}

void CryptohomeWebUIHandler::DidGetNSSUtilInfoOnUIThread(
    bool is_tpm_token_ready,
    const std::string& token_name,
    const std::string& user_pin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::FundamentalValue is_tpm_token_ready_value(is_tpm_token_ready);
  SetCryptohomeProperty("is-tpm-token-ready", is_tpm_token_ready_value);

  if (is_tpm_token_ready) {
    base::StringValue token_name_value(token_name);
    SetCryptohomeProperty("token-name", token_name_value);
    // Hide user_pin.
    base::StringValue user_pin_value(std::string(user_pin.length(), '*'));
    SetCryptohomeProperty("user-pin", user_pin_value);
  }
}

BoolDBusMethodCallback CryptohomeWebUIHandler::GetCryptohomeBoolCallback(
    const std::string& destination_id) {
  return base::Bind(&CryptohomeWebUIHandler::OnCryptohomeBoolProperty,
                    weak_ptr_factory_.GetWeakPtr(),
                    destination_id);
}

void CryptohomeWebUIHandler::OnCryptohomeBoolProperty(
    const std::string& destination_id,
    DBusMethodCallStatus call_status,
    bool value) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS)
    value = false;
  base::FundamentalValue fundamental_value(value);
  SetCryptohomeProperty(destination_id, fundamental_value);
}

void CryptohomeWebUIHandler::SetCryptohomeProperty(
    const std::string& destination_id,
    const base::Value& value) {
  base::StringValue destination_id_value(destination_id);
  web_ui()->CallJavascriptFunction(
      "SetCryptohomeProperty", destination_id_value, value);
}

}  // namespace chromeos
