// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_KERBEROS_ACCOUNTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_KERBEROS_ACCOUNTS_HANDLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/dbus/kerberos/kerberos_service.pb.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace kerberos {
class ListAccountsResponse;
}

namespace chromeos {
namespace settings {

class KerberosAccountsHandler : public ::settings::SettingsPageUIHandler,
                                public KerberosCredentialsManager::Observer {
 public:
  KerberosAccountsHandler();
  ~KerberosAccountsHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // KerberosCredentialsManager::Observer:
  void OnAccountsChanged() override;

 private:
  // WebUI "getKerberosAccounts" message callback.
  void HandleGetKerberosAccounts(const base::ListValue* args);

  // WebUI "addKerberosAccount" message callback.
  void HandleAddKerberosAccount(const base::ListValue* args);

  // Callback for the credential manager's AddAccountAndAuthenticate method.
  void OnAddAccountAndAuthenticate(const std::string& callback_id,
                                   kerberos::ErrorType error);

  // WebUI "removeKerberosAccount" message callback.
  void HandleRemoveKerberosAccount(const base::ListValue* args);

  // Callback for the credential manager's ListAccounts method.
  void OnListAccounts(base::Value callback_id,
                      const kerberos::ListAccountsResponse& response);

  // Fires the "kerberos-accounts-changed" event, which refreshes the Kerberos
  // Accounts UI.
  void RefreshUI();

  // This instance can be added as observer to KerberosCredentialsManager.
  // This class keeps track of that and removes this instance on destruction.
  ScopedObserver<KerberosCredentialsManager,
                 KerberosCredentialsManager::Observer>
      credentials_manager_observer_;

  base::WeakPtrFactory<KerberosAccountsHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(KerberosAccountsHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_KERBEROS_ACCOUNTS_HANDLER_H_
