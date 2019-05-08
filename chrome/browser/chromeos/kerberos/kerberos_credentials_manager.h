// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KERBEROS_KERBEROS_CREDENTIALS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_KERBEROS_KERBEROS_CREDENTIALS_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/chromeos/authpolicy/kerberos_files_handler.h"
#include "chromeos/dbus/kerberos/kerberos_service.pb.h"

class PrefRegistrySimple;

namespace chromeos {

class KerberosAddAccountRunner;

class KerberosCredentialsManager {
 public:
  using ResultCallback = base::OnceCallback<void(kerberos::ErrorType)>;
  using ListAccountsCallback =
      base::OnceCallback<void(const kerberos::ListAccountsResponse&)>;

  class Observer : public base::CheckedObserver {
   public:
    Observer();
    ~Observer() override;

    // Called when the set of accounts was changed through Kerberos credentials
    // manager.
    virtual void OnAccountsChanged() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  KerberosCredentialsManager();
  ~KerberosCredentialsManager();

  // Singleton accessor.
  static KerberosCredentialsManager& Get();

  // Registers prefs stored in the user profile.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Registers prefs stored in local state.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Start observing this object. |observer| is not owned.
  void AddObserver(Observer* observer);

  // Stop observing this object. |observer| is not owned.
  void RemoveObserver(const Observer* observer);

  // Adds an account for the given |principal_name| and authenticates it using
  // the given |password|. On success, sets |principal_name| as active principal
  // and calls OnAccountsChanged() for observers.
  void AddAccountAndAuthenticate(std::string principal_name,
                                 const std::string& password,
                                 ResultCallback callback);

  // Removes the Kerberos account for the account with given |principal_name|.
  // On success, calls OnAccountsChanged() for observers.
  void RemoveAccount(std::string principal_name, ResultCallback callback);

  // Returns a list of all existing accounts, including current status like
  // remaining Kerberos ticket lifetime.
  void ListAccounts(ListAccountsCallback callback);

  // Sets the contents of the Kerberos configuration (krb5.conf) to |krb5_conf|
  // for the account  with given |principal_name|.
  void SetConfig(std::string principal_name,
                 const std::string& krb5_conf,
                 ResultCallback callback);

  // Gets a Kerberos ticket-granting-ticket for the account with given
  // |principal_name|.
  void AcquireKerberosTgt(std::string principal_name,
                          const std::string& password,
                          ResultCallback callback);

  // Sets the currently active account.
  kerberos::ErrorType SetActiveAccount(std::string principal_name);

 private:
  friend class KerberosAddAccountRunner;

  // Callback on KerberosAddAccountRunner::Done.
  void OnAddAccountRunnerDone(KerberosAddAccountRunner* runner,
                              std::string principal_name,
                              ResultCallback callback,
                              kerberos::ErrorType error);

  // Callback for RemoveAccount().
  void OnRemoveAccount(const std::string& principal_name,
                       ResultCallback callback,
                       const kerberos::RemoveAccountResponse& response);

  // Callback for RemoveAccount().
  void OnListAccounts(ListAccountsCallback callback,
                      const kerberos::ListAccountsResponse& response);

  // Callback for SetConfig().
  void OnSetConfig(ResultCallback callback,
                   const kerberos::SetConfigResponse& response);

  // Callback for AcquireKerberosTgt().
  void OnAcquireKerberosTgt(
      ResultCallback callback,
      const kerberos::AcquireKerberosTgtResponse& response);

  // Calls KerberosClient::GetKerberosFiles().
  void GetKerberosFiles();

  // Callback for GetKerberosFiles().
  void OnGetKerberosFiles(const std::string& principal_name,
                          const kerberos::GetKerberosFilesResponse& response);

  // Callback for 'KerberosFilesChanged' D-Bus signal sent by kerberosd.
  void OnKerberosFilesChanged(const std::string& principal_name);

  // Calls OnAccountsChanged() on all observers.
  void NotifyAccountsChanged();

  // Called by OnSignalConnected(), puts Kerberos files where GSSAPI finds them.
  KerberosFilesHandler kerberos_files_handler_;

  // Handles the steps to add a Kerberos account.
  std::unique_ptr<KerberosAddAccountRunner> add_account_runner_;

  // Currently active principal.
  std::string active_principal_name_;

  // List of objects that observe this instance.
  base::ObserverList<Observer, true /* check_empty */> observers_;

  base::WeakPtrFactory<KerberosCredentialsManager> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(KerberosCredentialsManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KERBEROS_KERBEROS_CREDENTIALS_MANAGER_H_
