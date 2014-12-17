// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_TRACKER_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_TRACKER_SERVICE_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "components/keyed_service/core/keyed_service.h"
#include "google_apis/gaia/oauth2_token_service.h"

class AccountInfoFetcher;
class OAuth2TokenService;
class PrefService;
class RefreshTokenAnnotationRequest;
class SigninClient;

namespace base {
class DictionaryValue;
}

// AccountTrackerService is a KeyedService that retrieves and caches GAIA
// information about Google Accounts.
class AccountTrackerService : public KeyedService,
                              public OAuth2TokenService::Observer,
                              public base::NonThreadSafe {
 public:
  // Name of the preference property that persists the account information
  // tracked by this service.
  static const char kAccountInfoPref[];

  // TODO(mlerman): Remove all references to Profile::kNoHostedDomainFound in
  // favour of this.
  // Value representing no hosted domain in the kProfileHostedDomain preference.
  static const char kNoHostedDomainFound[];

  // Information about a specific account.
  struct AccountInfo {
    AccountInfo();
    ~AccountInfo();

    std::string account_id;  // The account ID used by OAuth2TokenService.
    std::string gaia;
    std::string email;
    std::string hosted_domain;
    // TODO(rogerta): eventually this structure will include other information
    // about the account, like full name, profile picture URL, etc.

    bool IsValid();
  };

  // Clients of AccountTrackerService can implement this interface and register
  // with AddObserver() to learn about account information changes.
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnAccountUpdated(const AccountInfo& info) {}
    virtual void OnAccountUpdateFailed(const std::string& account_id) {}
    virtual void OnAccountRemoved(const AccountInfo& info) {}
  };

  // Possible values for the kAccountIdMigrationState preference.
  enum AccountIdMigrationState {
    MIGRATION_NOT_STARTED,
    MIGRATION_IN_PROGRESS,
    MIGRATION_DONE
  };

  AccountTrackerService();
  ~AccountTrackerService() override;

  // KeyedService implementation.
  void Shutdown() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Take a SigninClient rather than a PrefService and a URLRequestContextGetter
  // since RequestContext cannot be created at startup.
  // (see http://crbug.com/171406)
  void Initialize(OAuth2TokenService* token_service,
                  SigninClient* signin_client);

  // To be called after the Profile is fully initialized; permits network
  // calls to be executed.
  void EnableNetworkFetches();

  // Returns the list of known accounts and for which gaia IDs
  // have been fetched.
  std::vector<AccountInfo> GetAccounts() const;
  AccountInfo GetAccountInfo(const std::string& account_id);
  AccountInfo FindAccountInfoByGaiaId(const std::string& gaia_id);
  AccountInfo FindAccountInfoByEmail(const std::string& email);

  // Indicates if all user information has been fetched. If the result is false,
  // there are still unfininshed fetchers.
  virtual bool IsAllUserInfoFetched() const;

  // Picks the correct account_id for the specified account depending on the
  // migration state.
  std::string PickAccountIdForAccount(const std::string& gaia,
                                      const std::string& email);
  static std::string PickAccountIdForAccount(PrefService* pref_service,
                                             const std::string& gaia,
                                             const std::string& email);

  // Seeds the account whose account_id is given by PickAccountIdForAccount()
  // with its corresponding gaia id and email address.
  void SeedAccountInfo(const std::string& gaia, const std::string& email);

  AccountIdMigrationState GetMigrationState();
  static AccountIdMigrationState GetMigrationState(PrefService* pref_service);

 protected:
  // Available to be called in tests.
  void SetAccountStateFromUserInfo(const std::string& account_id,
                                   const base::DictionaryValue* user_info);

 private:
  friend class AccountInfoFetcher;

  // These methods are called by fetchers.
  void OnUserInfoFetchSuccess(AccountInfoFetcher* fetcher,
                              const base::DictionaryValue* user_info);
  void OnUserInfoFetchFailure(AccountInfoFetcher* fetcher);

  // OAuth2TokenService::Observer implementation.
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;

  struct AccountState {
    AccountInfo info;
  };

  void NotifyAccountUpdated(const AccountState& state);
  void NotifyAccountUpdateFailed(const std::string& account_id);
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

  // Virtual so that tests can override the network fetching behaviour.
  virtual void SendRefreshTokenAnnotationRequest(const std::string& account_id);
  void RefreshTokenAnnotationRequestDone(const std::string& account_id);

  OAuth2TokenService* token_service_;  // Not owned.
  SigninClient* signin_client_;  // Not owned.
  std::map<std::string, AccountInfoFetcher*> user_info_requests_;
  std::map<std::string, AccountState> accounts_;
  ObserverList<Observer> observer_list_;
  bool shutdown_called_;
  bool network_fetches_enabled_;
  std::list<std::string> pending_user_info_fetches_;

  // Holds references to refresh token annotation requests keyed by account_id.
  base::ScopedPtrHashMap<std::string, RefreshTokenAnnotationRequest>
      refresh_token_annotation_requests_;

  DISALLOW_COPY_AND_ASSIGN(AccountTrackerService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_TRACKER_SERVICE_H_
