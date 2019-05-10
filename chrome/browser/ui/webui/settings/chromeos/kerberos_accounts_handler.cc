// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/kerberos_accounts_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
namespace settings {

namespace {

KerberosCredentialsManager::ResultCallback EmptyResultCallback() {
  return base::BindOnce([](kerberos::ErrorType error) {
    // Do nothing.
  });
}

}  // namespace

KerberosAccountsHandler::KerberosAccountsHandler()
    : credentials_manager_observer_(this), weak_factory_(this) {}

KerberosAccountsHandler::~KerberosAccountsHandler() = default;

void KerberosAccountsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getKerberosAccounts",
      base::BindRepeating(&KerberosAccountsHandler::HandleGetKerberosAccounts,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "addKerberosAccount",
      base::BindRepeating(&KerberosAccountsHandler::HandleAddKerberosAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "reauthenticateKerberosAccount",
      base::BindRepeating(
          &KerberosAccountsHandler::HandleReauthenticateKerberosAccount,
          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeKerberosAccount",
      base::BindRepeating(&KerberosAccountsHandler::HandleRemoveKerberosAccount,
                          weak_factory_.GetWeakPtr()));
}

void KerberosAccountsHandler::HandleGetKerberosAccounts(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK(!args->GetList().empty());
  base::Value callback_id = args->GetList()[0].Clone();

  KerberosCredentialsManager::Get().ListAccounts(
      base::BindOnce(&KerberosAccountsHandler::OnListAccounts,
                     weak_factory_.GetWeakPtr(), std::move(callback_id)));
}

void KerberosAccountsHandler::OnListAccounts(
    base::Value callback_id,
    const kerberos::ListAccountsResponse& response) {
  base::ListValue accounts;

  // Default icon is a briefcase.
  gfx::ImageSkia skia_default_icon =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGIN_DEFAULT_USER_2);
  std::string default_icon = webui::GetBitmapDataUrl(
      skia_default_icon.GetRepresentation(1.0f).GetBitmap());

  for (int n = 0; n < response.accounts_size(); ++n) {
    const kerberos::Account& account = response.accounts(n);

    base::DictionaryValue account_dict;
    account_dict.SetString("principalName", account.principal_name());
    account_dict.SetString("krb5conf", account.krb5conf());
    account_dict.SetBoolean("isSignedIn", account.tgt_validity_seconds() > 0);
    account_dict.SetString("pic", default_icon);
    accounts.GetList().push_back(std::move(account_dict));
  }

  ResolveJavascriptCallback(callback_id, accounts);
}

void KerberosAccountsHandler::HandleAddKerberosAccount(
    const base::ListValue* args) {
  AllowJavascript();

  // TODO(https://crbug.com/961246):
  //   - Prevent account changes when Kerberos is disabled.
  //   - Remove all accounts when Kerberos is disabled.

  // TODO(ljusten): Call KerberosAddAccountDialog::Show() instead when that's
  // implemented.

  static int count = 0;
  KerberosCredentialsManager::Get().AddAccountAndAuthenticate(
      base::StringPrintf("user%i@realm", ++count), "password",
      EmptyResultCallback());
}

void KerberosAccountsHandler::HandleReauthenticateKerberosAccount(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK(!args->GetList().empty());

  // TODO(ljusten): Add KerberosAddAccountDialog::Show(principal_name) when
  // that's implemented.
}

void KerberosAccountsHandler::HandleRemoveKerberosAccount(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK(!args->GetList().empty());
  const std::string& principal_name = args->GetList()[0].GetString();

  // Note that we're observing the credentials manager, so OnAccountsChanged()
  // is called when an account is removed, which calls RefreshUI(). Thus, it's
  // fine to pass an EmptyResultCallback() in here and not something that calls
  // RefreshUI().
  KerberosCredentialsManager::Get().RemoveAccount(principal_name,
                                                  EmptyResultCallback());
}

void KerberosAccountsHandler::OnJavascriptAllowed() {
  credentials_manager_observer_.Add(&KerberosCredentialsManager::Get());
}

void KerberosAccountsHandler::OnJavascriptDisallowed() {
  credentials_manager_observer_.RemoveAll();
}

void KerberosAccountsHandler::OnAccountsChanged() {
  RefreshUI();
}

void KerberosAccountsHandler::RefreshUI() {
  FireWebUIListener("kerberos-accounts-changed");
}

}  // namespace settings
}  // namespace chromeos
