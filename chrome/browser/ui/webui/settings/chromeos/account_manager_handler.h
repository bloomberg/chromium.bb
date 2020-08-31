// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCOUNT_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCOUNT_MANAGER_HANDLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace chromeos {
namespace settings {

class AccountManagerUIHandler : public ::settings::SettingsPageUIHandler,
                                public AccountManager::Observer,
                                public signin::IdentityManager::Observer {
 public:
  // Accepts non-owning pointers to |AccountManager|, |AccountTrackerService|
  // and |IdentityManager|. Both of these must outlive |this| instance.
  AccountManagerUIHandler(AccountManager* account_manager,
                          signin::IdentityManager* identity_manager);
  ~AccountManagerUIHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // |AccountManager::Observer| overrides.
  // |AccountManager| is considered to be the source of truth for account
  // information.
  void OnTokenUpserted(const AccountManager::Account& account) override;
  void OnAccountRemoved(const AccountManager::Account& account) override;

  // |signin::IdentityManager::Observer| overrides.
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override;

 private:
  friend class AccountManagerUIHandlerTest;

  void SetProfileForTesting(Profile* profile);

  // WebUI "getAccounts" message callback.
  void HandleGetAccounts(const base::ListValue* args);

  // WebUI "addAccount" message callback.
  void HandleAddAccount(const base::ListValue* args);

  // WebUI "reauthenticateAccount" message callback.
  void HandleReauthenticateAccount(const base::ListValue* args);

  // WebUI "migrateAccount" message callback.
  void HandleMigrateAccount(const base::ListValue* args);

  // WebUI "removeAccount" message callback.
  void HandleRemoveAccount(const base::ListValue* args);

  // WebUI "showWelcomeDialogIfRequired" message callback.
  void HandleShowWelcomeDialogIfRequired(const base::ListValue* args);

  // |AccountManager::GetAccounts| callback.
  void OnGetAccounts(
      base::Value callback_id,
      const std::vector<AccountManager::Account>& stored_accounts);

  // Returns secondary Gaia accounts from |stored_accounts| list. If the Device
  // Account is a Gaia account, populates |device_account| with information
  // about that account, otherwise does not modify |device_account|.
  // If user (device account) is child - |is_child_user| should be set to true,
  // in this case "unmigrated" property will be always false for secondary
  // accounts.
  base::ListValue GetSecondaryGaiaAccounts(
      const std::vector<AccountManager::Account>& stored_accounts,
      const AccountId device_account_id,
      const bool is_child_user,
      base::DictionaryValue* device_account);

  // Refreshes the UI.
  void RefreshUI();

  Profile* profile_ = nullptr;

  // A non-owning pointer to |AccountManager|.
  AccountManager* const account_manager_;

  // A non-owning pointer to |IdentityManager|.
  signin::IdentityManager* const identity_manager_;

  // An observer for |AccountManager|. Automatically deregisters when |this| is
  // destructed.
  ScopedObserver<AccountManager, AccountManager::Observer>
      account_manager_observer_;

  // An observer for |signin::IdentityManager|. Automatically deregisters when
  // |this| is destructed.
  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      identity_manager_observer_;

  base::WeakPtrFactory<AccountManagerUIHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AccountManagerUIHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCOUNT_MANAGER_HANDLER_H_
