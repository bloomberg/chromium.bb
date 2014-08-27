// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_TRACKER_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_TRACKER_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "google_apis/gaia/oauth2_token_service.h"

class AccountInfoFetcher;
class OAuth2TokenService;
class PrefService;

namespace base {
class DictionaryValue;
}

// AccountTrackerService is a KeyedService that retrieves and caches GAIA
// information about Google Accounts.
class AccountTrackerService : public KeyedService,
                              public OAuth2TokenService::Observer {
 public:
  // Name of the preference property that persists the account information
  // tracked by this service.
  static const char kAccountInfoPref[];

  // Information about a specific account.
  struct AccountInfo {
    std::string account_id;  // The account ID used by OAuth2TokenService.
    std::string gaia;
    std::string email;
    // TODO(rogerta): eventually this structure will include other information
    // about the account, like full name, profile picture URL, etc.
  };

  // Clients of AccountTrackerService can implement this interface and register
  // with AddObserver() to learn about account information changes.
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnAccountUpdated(const AccountInfo& info) = 0;
    virtual void OnAccountRemoved(const AccountInfo& info) = 0;
  };

  // Possible values for the kAccountIdMigrationState preference.
  enum AccountIdMigrationState {
    MIGRATION_NOT_STARTED,
    MIGRATION_IN_PROGRESS,
    MIGRATION_DONE
  };

  AccountTrackerService();
  virtual ~AccountTrackerService();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void Initialize(OAuth2TokenService* token_service,
                  PrefService* pref_service,
                  net::URLRequestContextGetter* request_context_getter);

  // Returns the list of known accounts and for which gaia IDs
  // have been fetched.
  std::vector<AccountInfo> GetAccounts() const;
  AccountInfo GetAccountInfo(const std::string& account_id);
  AccountInfo FindAccountInfoByGaiaId(const std::string& gaia_id);
  AccountInfo FindAccountInfoByEmail(const std::string& email);

  // Indicates if all user information has been fetched. If the result is false,
  // there are still unfininshed fetchers.
  virtual bool IsAllUserInfoFetched() const;

  AccountIdMigrationState GetMigrationState();
  static AccountIdMigrationState GetMigrationState(PrefService* pref_service);

 private:
  friend class AccountInfoFetcher;

  // These methods are called by fetchers.
  void OnUserInfoFetchSuccess(AccountInfoFetcher* fetcher,
                              const base::DictionaryValue* user_info);
  void OnUserInfoFetchFailure(AccountInfoFetcher* fetcher);

  // OAuth2TokenService::Observer implementation.
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE;

  struct AccountState {
    AccountInfo info;
  };

  void NotifyAccountUpdated(const AccountState& state);
  void NotifyAccountRemoved(const AccountState& state);

  void StartTrackingAccount(const std::string& account_id);
  void StopTrackingAccount(const std::string& account_id);

  // Virtual so that tests can override the network fetching behaviour.
  virtual void StartFetchingUserInfo(const std::string& account_id);
  void DeleteFetcher(AccountInfoFetcher* fetcher);

  // Load the current state of the account info from the preferences file.
  void LoadFromPrefs();
  void SaveToPrefs(const AccountState& account);
  void RemoveFromPrefs(const AccountState& account);

  void LoadFromTokenService();

  OAuth2TokenService* token_service_;  // Not owned.
  PrefService* pref_service_;  // Not owned.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  std::map<std::string, AccountInfoFetcher*> user_info_requests_;
  std::map<std::string, AccountState> accounts_;
  ObserverList<Observer> observer_list_;
  bool shutdown_called_;

  DISALLOW_COPY_AND_ASSIGN(AccountTrackerService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_TRACKER_SERVICE_H_
