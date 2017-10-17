// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_H_

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"

class ProfileOAuth2TokenService;
class SigninClient;

class AccountReconcilor : public KeyedService,
                          public content_settings::Observer,
                          public GaiaCookieManagerService::Observer,
                          public OAuth2TokenService::Observer,
                          public SigninManagerBase::Observer {
 public:
  // When an instance of this class exists, the account reconcilor is suspended.
  // It will automatically restart when all instances of Lock have been
  // destroyed.
  class Lock final {
   public:
    explicit Lock(AccountReconcilor* reconcilor);
    ~Lock();

   private:
    AccountReconcilor* reconcilor_;
    THREAD_CHECKER(thread_checker_);
    DISALLOW_COPY_AND_ASSIGN(Lock);
  };

  class Observer {
   public:
    virtual ~Observer() {}

    // The typical order of events is:
    // - OnStartReconcile() called at the beginning of StartReconcile().
    // - When reconcile is blocked:
    //   1. current reconcile is aborted with AbortReconcile(),
    //   2. OnBlockReconcile() is called.
    // - When reconcile is unblocked:
    //   1. OnUnblockReconcile() is called,
    //   2. reconcile is restarted if needed with StartReconcile(), which
    //     triggers a call to OnStartReconcile().

    // Called whe reconcile starts.
    virtual void OnStartReconcile() {}
    // Called when the AccountReconcilor is blocked.
    virtual void OnBlockReconcile() {}
    // Called when the AccountReconcilor is unblocked.
    virtual void OnUnblockReconcile() {}
  };

  AccountReconcilor(ProfileOAuth2TokenService* token_service,
                    SigninManagerBase* signin_manager,
                    SigninClient* client,
                    GaiaCookieManagerService* cookie_manager_service);
  ~AccountReconcilor() override;

  void Initialize(bool start_reconcile_if_tokens_available);

  // Signal that the status of the new_profile_management flag has changed.
  // Pass the new status as an explicit parameter since disabling the flag
  // doesn't remove it from the CommandLine::ForCurrentProcess().
  void OnNewProfileManagementFlagChanged(bool new_flag_status);

  // Signal that an X-Chrome-Manage-Accounts was received from GAIA. Pass the
  // ServiceType specified by GAIA in the 204 response.
  // Virtual for testing.
  virtual void OnReceivedManageAccountsResponse(
      signin::GAIAServiceType service_type);

  // KeyedService implementation.
  void Shutdown() override;

  // Determine what the reconcilor is currently doing.
  signin_metrics::AccountReconcilorState GetState();

  // Adds ands removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class Lock;
  friend class AccountReconcilorTest;
  friend class DiceBrowserTestBase;
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, SigninManagerRegistration);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, Reauth);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, ProfileAlreadyConnected);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTestDice, TableRowTest);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, DiceTokenServiceRegistration);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, DiceReconcileWhithoutSignin);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, DiceReconcileNoop);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, DiceLastKnownFirstAccount);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           DiceReconcileReuseGaiaFirstAccount);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, TokensNotLoaded);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileCookiesDisabled);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileContentSettings);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileContentSettingsGaiaUrl);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileContentSettingsNonGaiaUrl);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileContentSettingsInvalidPattern);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, GetAccountsFromCookieSuccess);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, GetAccountsFromCookieFailure);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileNoop);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileNoopWithDots);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileNoopMultiple);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileAddToCookie);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           SignoutAfterErrorDoesNotRecordUma);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileRemoveFromCookie);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileAddToCookieTwice);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileBadPrimary);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileOnlyOnce);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, Lock);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileWithSessionInfoExpiredDefault);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           AddAccountToCookieCompletedWithBogusAccount);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, NoLoopWithBadPrimary);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, WontMergeAccountsWithError);

  bool IsRegisteredWithTokenService() const {
    return registered_with_token_service_;
  }

  // Register and unregister with dependent services.
  void RegisterWithSigninManager();
  void UnregisterWithSigninManager();
  void RegisterWithTokenService();
  void UnregisterWithTokenService();
  void RegisterWithCookieManagerService();
  void UnregisterWithCookieManagerService();
  void RegisterWithContentSettings();
  void UnregisterWithContentSettings();

  // The reconcilor is enabled if Sync or Dice is enabled.
  bool IsEnabled();
  // Returns true if account consistency is enabled (Mirror or Dice).
  bool IsAccountConsistencyEnabled();

  // All actions with side effects, only doing meaningful work if account
  // consistency is enabled. Virtual so that they can be overridden in tests.
  virtual void PerformMergeAction(const std::string& account_id);
  virtual void PerformLogoutAllAccountsAction();

  // Used during periodic reconciliation.
  void StartReconcile();
  void FinishReconcile();
  void AbortReconcile();
  void CalculateIfReconcileIsDone();
  void ScheduleStartReconcileIfChromeAccountsChanged();
  // Revokes tokens for all accounts in chrome_accounts_ but primary_account_.
  void RevokeAllSecondaryTokens();

  void ValidateAccountsFromTokenService();
  // Note internally that this |account_id| is added to the cookie jar.
  bool MarkAccountAsAddedToCookie(const std::string& account_id);

  // The reconcilor only starts when the token service is ready.
  bool IsTokenServiceReady();

  // Returns the first account to add in the Gaia cookie.
  // If this returns an empty string, the user must be logged out of all
  // accounts.
  std::string GetFirstGaiaAccountForReconcile() const;

  // Overriden from content_settings::Observer.
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      std::string resource_identifier) override;

  // Overriden from GaiaGookieManagerService::Observer.
  void OnAddAccountToCookieCompleted(
      const std::string& account_id,
      const GoogleServiceAuthError& error) override;
  void OnGaiaAccountsInCookieUpdated(
        const std::vector<gaia::ListedAccount>& accounts,
        const std::vector<gaia::ListedAccount>& signed_out_accounts,
        const GoogleServiceAuthError& error) override;

  // Overriden from OAuth2TokenService::Observer.
  void OnEndBatchChanges() override;
  void OnRefreshTokensLoaded() override;

  // Overriden from SigninManagerBase::Observer.
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username) override;
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

  // Lock related methods.
  void IncrementLockCount();
  void DecrementLockCount();
  void BlockReconcile();
  void UnblockReconcile();
  bool IsReconcileBlocked() const;

  // The ProfileOAuth2TokenService associated with this reconcilor.
  ProfileOAuth2TokenService* token_service_;

  // The SigninManager associated with this reconcilor.
  SigninManagerBase* signin_manager_;

  // The SigninClient associated with this reconcilor.
  SigninClient* client_;

  // The GaiaCookieManagerService associated with this reconcilor.
  GaiaCookieManagerService* cookie_manager_service_;

  bool registered_with_token_service_;
  bool registered_with_cookie_manager_service_;
  bool registered_with_content_settings_;

  // True while the reconcilor is busy checking or managing the accounts in
  // this profile.
  bool is_reconcile_started_;
  base::Time reconcile_start_time_;

  // True iff this is the first time the reconcilor is executing.
  bool first_execution_;

  // True iff an error occured during the last attempt to reconcile.
  bool error_during_last_reconcile_;

  // Used during reconcile action.
  // These members are used to validate the gaia cookie.  |gaia_accounts_|
  // holds the state of google accounts in the gaia cookie.  Each element is
  // holds the email address, gaia id and validity as returned from GAIA.  The
  // accounts in the vector are ordered the in same way as the gaia cookie.
  std::vector<gaia::ListedAccount> gaia_accounts_;

  // Used during reconcile action.
  // These members are used to validate the tokens in OAuth2TokenService.
  std::string primary_account_;
  std::vector<std::string> chrome_accounts_;
  std::vector<std::string> add_to_cookie_;
  bool chrome_accounts_changed_;
  // Last known "first account". Used when cookies are lost as a best guess.
  std::string last_known_first_account_;

  // Used for the Lock.
  // StartReconcile() is blocked while this is > 0.
  int account_reconcilor_lock_count_;
  // StartReconcile() should be started when the reconcilor is unblocked.
  bool reconcile_on_unblock_;

  base::ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilor);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_H_
