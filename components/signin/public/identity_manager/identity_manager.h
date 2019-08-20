// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/ubertoken_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "google_apis/gaia/oauth2_token_service_observer.h"
#include "services/identity/public/cpp/scope_set.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif

namespace gaia {
class GaiaSource;
struct ListedAccount;
}  // namespace gaia

namespace network {
class SharedURLLoaderFactory;
class TestURLLoaderFactory;
}  // namespace network

// Necessary to declare these classes as friends.
class PrefRegistrySimple;
class SigninManagerAndroid;

class AccountFetcherService;
class AccountTrackerService;
class GaiaCookieManagerService;
class PrimaryAccountManager;
class ProfileOAuth2TokenService;

namespace signin {

class AccountsMutator;
class AccountsCookieMutator;
struct AccountsInCookieJarInfo;
class IdentityManagerTest;
class IdentityTestEnvironment;
class DeviceAccountsSynchronizer;
class DiagnosticsProvider;
class PrimaryAccountMutator;
enum class ClearPrimaryAccountPolicy;
struct CookieParamsForTest;

// Gives access to information about the user's Google identities. See
// ./README.md for detailed documentation.
class IdentityManager : public KeyedService,
                        public OAuth2AccessTokenManager::DiagnosticsObserver,
                        public OAuth2TokenServiceObserver {
 public:
  class Observer {
   public:
    Observer() = default;
    virtual ~Observer() = default;

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // Called when an account becomes the user's primary account.
    // This method is not called during a reauth.
    virtual void OnPrimaryAccountSet(
        const CoreAccountInfo& primary_account_info) {}

    // Called when the user moves from having a primary account to no longer
    // having a primary account (note that the user may still have an
    // *unconsented* primary account after this event; see./README.md).
    virtual void OnPrimaryAccountCleared(
        const CoreAccountInfo& previous_primary_account_info) {}

    // When the unconsented primary account (see ./README.md) of the user
    // changes, this callback gets called with the new account as
    // |unconsented_primary_account_info|. If after the change, there is no
    // unconsented primary account, |unconsented_primary_account_info| is empty.
    // This does not get called when the unconsented account becomes consented
    // (as the same account was unconsented before so there is no change). In
    // all other changes (the unconsented primary account gets added, changed or
    // removed), this notification is called only once. Note: we do not use the
    // {Set, Cleared} notifications like for the primary account above because
    // the identity manager does not have clear guarantees that that account
    // cannot change in one atomic operation (without getting cleared in the
    // mean-time).
    virtual void OnUnconsentedPrimaryAccountChanged(
        const CoreAccountInfo& unconsented_primary_account_info) {}

    // Called when a new refresh token is associated with |account_info|.
    // NOTE: On a signin event, the ordering of this callback wrt the
    // OnPrimaryAccountSet() callback is undefined. If you as a client are
    // interested in both callbacks, PrimaryAccountAccessTokenFetcher will
    // likely meet your needs. Otherwise, if this lack of ordering is
    // problematic for your use case, please contact blundell@chromium.org.
    virtual void OnRefreshTokenUpdatedForAccount(
        const CoreAccountInfo& account_info) {}

    // Called when the refresh token previously associated with |account_id|
    // has been removed. At the time that this callback is invoked, there is
    // no longer guaranteed to be any AccountInfo associated with
    // |account_id|.
    // NOTE: It is not guaranteed that a call to
    // OnRefreshTokenUpdatedForAccount() has previously occurred for this
    // account due to corner cases.
    // TODO(https://crbug.com/884731): Eliminate these corner cases.
    // NOTE: On a signout event, the ordering of this callback wrt the
    // OnPrimaryAccountCleared() callback is undefined.If this lack of ordering
    // is problematic for your use case, please contact blundell@chromium.org.
    virtual void OnRefreshTokenRemovedForAccount(
        const CoreAccountId& account_id) {}

    // Called when the error state of the refresh token for |account_id| has
    // changed. Note: It is always called after
    // |OnRefreshTokenUpdatedForAccount| when the refresh token is updated. It
    // is not called when the refresh token is removed.
    virtual void OnErrorStateOfRefreshTokenUpdatedForAccount(
        const CoreAccountInfo& account_info,
        const GoogleServiceAuthError& error) {}

    // Called after refresh tokens are loaded.
    virtual void OnRefreshTokensLoaded() {}

    // Called whenever the list of Gaia accounts in the cookie jar has changed.
    // |accounts_in_cookie_jar_info.accounts| is ordered by the order of the
    // accounts in the cookie.
    //
    // This observer method is also called when fetching the list of accounts
    // in Gaia cookies fails after a number of internal retries. In this case:
    // * |error| hold the last error to fetch the list of accounts;
    // * |accounts_in_cookie_jar_info.accounts_are_fresh| is set to false as
    //   the accounts information is considered stale;
    // * |accounts_in_cookie_jar_info.accounts| holds the last list of known
    //   accounts in the cookie jar.
    virtual void OnAccountsInCookieUpdated(
        const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
        const GoogleServiceAuthError& error) {}

    // Called when the Gaia cookie has been deleted explicitly by a user
    // action, e.g. from the settings or by an extension.
    virtual void OnAccountsCookieDeletedByUserAction() {}

    // Called after a batch of refresh token state chagnes is completed.
    virtual void OnEndBatchOfRefreshTokenStateChanges() {}

    // Called after an account is updated.
    virtual void OnExtendedAccountInfoUpdated(const AccountInfo& info) {}

    // Called after removing an account info.
    virtual void OnExtendedAccountInfoRemoved(const AccountInfo& info) {}
  };

  // Methods to register or remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Provides access to the core information of the user's primary account.
  // Returns an empty struct if no such info is available, either because there
  // is no primary account yet or because the user signed out.
  CoreAccountInfo GetPrimaryAccountInfo() const;

  // Provides access to the account ID of the user's primary account. Simple
  // convenience wrapper over GetPrimaryAccountInfo().account_id.
  CoreAccountId GetPrimaryAccountId() const;

  // Returns whether the user's primary account is available.
  bool HasPrimaryAccount() const;

  // Provides access to the core information of the user's unconsented primary
  // account (see ./README.md). Returns an empty info, if there is no such
  // account.
  CoreAccountInfo GetUnconsentedPrimaryAccountInfo() const;

  // Provides access to the account ID of the user's unconsented primary
  // account (see ./README.md). Returns an empty id if there is no such account.
  CoreAccountId GetUnconsentedPrimaryAccountId() const;

  // Returns whether the user's unconsented primary account (see ./README.md) is
  // available.
  bool HasUnconsentedPrimaryAccount() const;

  // Creates an AccessTokenFetcher given the passed-in information.
  std::unique_ptr<AccessTokenFetcher> CreateAccessTokenFetcherForAccount(
      const CoreAccountId& account_id,
      const std::string& oauth_consumer_name,
      const identity::ScopeSet& scopes,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode) WARN_UNUSED_RESULT;

  // Creates an AccessTokenFetcher given the passed-in information, allowing
  // to specify a custom |url_loader_factory| as well.
  std::unique_ptr<AccessTokenFetcher> CreateAccessTokenFetcherForAccount(
      const CoreAccountId& account_id,
      const std::string& oauth_consumer_name,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const identity::ScopeSet& scopes,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode) WARN_UNUSED_RESULT;

  // Creates an AccessTokenFetcher given the passed-in information, allowing to
  // specify custom |client_id| and |client_secret| to identify the OAuth client
  // app.
  std::unique_ptr<AccessTokenFetcher> CreateAccessTokenFetcherForClient(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const std::string& client_secret,
      const std::string& oauth_consumer_name,
      const identity::ScopeSet& scopes,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode) WARN_UNUSED_RESULT;

  // If an entry exists in the cache of access tokens corresponding to the
  // given information, removes that entry; in this case, the next access token
  // request for |account_id| and |scopes| will fetch a new token from the
  // network. Otherwise, is a no-op.
  void RemoveAccessTokenFromCache(const CoreAccountId& account_id,
                                  const identity::ScopeSet& scopes,
                                  const std::string& access_token);

  // Provides the information of all accounts that have refresh tokens.
  // NOTE: The accounts should not be assumed to be in any particular order; in
  // particular, they are not guaranteed to be in the order in which the
  // refresh tokens were added.
  std::vector<CoreAccountInfo> GetAccountsWithRefreshTokens() const;

  // Same functionality as GetAccountsWithRefreshTokens() but returning the
  // extended account information.
  std::vector<AccountInfo> GetExtendedAccountInfoForAccountsWithRefreshToken()
      const;

  // Returns true if (a) the primary account exists, and (b) a refresh token
  // exists for the primary account.
  bool HasPrimaryAccountWithRefreshToken() const;

  // Returns true if a refresh token exists for |account_id|.
  bool HasAccountWithRefreshToken(const CoreAccountId& account_id) const;

  // Returns true if all refresh tokens have been loaded from disk.
  bool AreRefreshTokensLoaded() const;

  // Returns true if (a) a refresh token exists for |account_id|, and (b) the
  // refresh token is in a persistent error state (defined as
  // GoogleServiceAuthError::IsPersistentError() returning true for the error
  // returned by GetErrorStateOfRefreshTokenForAccount(account_id)).
  bool HasAccountWithRefreshTokenInPersistentErrorState(
      const CoreAccountId& account_id) const;

  // Returns the error state of the refresh token associated with |account_id|.
  // In particular: Returns GoogleServiceAuthError::AuthErrorNone() if either
  // (a) no refresh token exists for |account_id|, or (b) the refresh token is
  // not in a persistent error state. Otherwise, returns the last persistent
  // error that was detected when using the refresh token.
  GoogleServiceAuthError GetErrorStateOfRefreshTokenForAccount(
      const CoreAccountId& account_id) const;

  // Returns extended information for account identified by |account_info|.
  // The information will be returned if the information is available and
  // refresh token is available for account.
  base::Optional<AccountInfo> FindExtendedAccountInfoForAccount(
      const CoreAccountInfo& account_info) const;

  // Looks up and returns information for account with given |account_id|. If
  // the account cannot be found, return an empty optional. This is equivalent
  // to searching on the vector returned by GetAccountsWithRefreshTokens() but
  // without allocating memory for the vector.
  base::Optional<AccountInfo>
  FindAccountInfoForAccountWithRefreshTokenByAccountId(
      const CoreAccountId& account_id) const;

  // Looks up and returns information for account with given |email_address|. If
  // the account cannot be found, return an empty optional. This is equivalent
  // to searching on the vector returned by GetAccountsWithRefreshTokens() but
  // without allocating memory for the vector.
  base::Optional<AccountInfo>
  FindAccountInfoForAccountWithRefreshTokenByEmailAddress(
      const std::string& email_address) const;

  // Looks up and returns information for account with given |gaia_id|. If the
  // account cannot be found, return an empty optional. This is equivalent to
  // searching on the vector returned by GetAccountsWithRefreshTokens() but
  // without allocating memory for the vector.
  base::Optional<AccountInfo> FindAccountInfoForAccountWithRefreshTokenByGaiaId(
      const std::string& gaia_id) const;

  // Creates an UbertokenFetcher given the passed-in information, allowing
  // to specify a custom |url_loader_factory| as well.
  std::unique_ptr<UbertokenFetcher> CreateUbertokenFetcherForAccount(
      const CoreAccountId& account_id,
      UbertokenFetcher::CompletionCallback callback,
      gaia::GaiaSource source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool bound_to_channel_id = true);

  // Provides the information of all accounts that are present in the Gaia
  // cookie in the cookie jar, ordered by their order in the cookie.
  // If the returned accounts are not fresh, an internal update will be
  // triggered and there will be a subsequent invocation of
  // IdentityManager::Observer::OnAccountsInCookieJarChanged().
  AccountsInCookieJarInfo GetAccountsInCookieJar() const;

  // Returns pointer to the object used to change the signed-in state of the
  // primary account, if supported on the current platform. Otherwise, returns
  // null.
  PrimaryAccountMutator* GetPrimaryAccountMutator();

  // Returns pointer to the object used to seed accounts and mutate state of
  // accounts' refresh tokens, if supported on the current platform. Otherwise,
  // returns null.
  AccountsMutator* GetAccountsMutator();

  // Returns pointer to the object used to manipulate the cookies stored and the
  // accounts associated with them. Guaranteed to be non-null.
  AccountsCookieMutator* GetAccountsCookieMutator();

  // Returns pointer to the object used to seed accounts information from the
  // device-level accounts. May be null if the system has no such notion.
  DeviceAccountsSynchronizer* GetDeviceAccountsSynchronizer();

  // Observer interface for classes that want to monitor status of various
  // requests. Mostly useful in tests and debugging contexts (e.g., WebUI).
  class DiagnosticsObserver {
   public:
    DiagnosticsObserver() = default;
    virtual ~DiagnosticsObserver() = default;

    DiagnosticsObserver(const DiagnosticsObserver&) = delete;
    DiagnosticsObserver& operator=(const DiagnosticsObserver&) = delete;

    // Called when receiving request for access token.
    virtual void OnAccessTokenRequested(const CoreAccountId& account_id,
                                        const std::string& consumer_id,
                                        const identity::ScopeSet& scopes) {}

    // Called when an access token request is completed. Contains diagnostic
    // information about the access token request.
    virtual void OnAccessTokenRequestCompleted(const CoreAccountId& account_id,
                                               const std::string& consumer_id,
                                               const identity::ScopeSet& scopes,
                                               GoogleServiceAuthError error,
                                               base::Time expiration_time) {}

    // Called when an access token was removed.
    virtual void OnAccessTokenRemovedFromCache(
        const CoreAccountId& account_id,
        const identity::ScopeSet& scopes) {}

    // Called when a new refresh token is available. Contains diagnostic
    // information about the source of the operation.
    virtual void OnRefreshTokenUpdatedForAccountFromSource(
        const CoreAccountId& account_id,
        bool is_refresh_token_valid,
        const std::string& source) {}

    // Called when a refresh token is removed. Contains diagnostic information
    // about the source that initiated the revokation operation.
    virtual void OnRefreshTokenRemovedForAccountFromSource(
        const CoreAccountId& account_id,
        const std::string& source) {}
  };

  void AddDiagnosticsObserver(DiagnosticsObserver* observer);
  void RemoveDiagnosticsObserver(DiagnosticsObserver* observer);

  //  **************************************************************************
  //  NOTE: All public methods methods below are either intended to be used only
  //  by signin code, or are slated for deletion. Most IdentityManager consumers
  //  should not need to interact with any methods below this line.
  //  **************************************************************************

  IdentityManager(
      std::unique_ptr<AccountTrackerService> account_tracker_service,
      std::unique_ptr<ProfileOAuth2TokenService> token_service,
      std::unique_ptr<GaiaCookieManagerService> gaia_cookie_manager_service,
      std::unique_ptr<PrimaryAccountManager> primary_account_manager,
      std::unique_ptr<AccountFetcherService> account_fetcher_service,
      std::unique_ptr<PrimaryAccountMutator> primary_account_mutator,
      std::unique_ptr<AccountsMutator> accounts_mutator,
      std::unique_ptr<AccountsCookieMutator> accounts_cookie_mutator,
      std::unique_ptr<DiagnosticsProvider> diagnostics_provider,
      std::unique_ptr<DeviceAccountsSynchronizer> device_accounts_synchronizer);
  ~IdentityManager() override;

  // Performs initialization that is dependent on the network being
  // initialized.
  void OnNetworkInitialized();

  // Returns |true| if migration of the account ID from normalized email is
  // supported for the current platform.
  static bool IsAccountIdMigrationSupported();

  // Registers per-install prefs used by this class.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Registers per-profile prefs used by this class.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Picks the correct account_id for the specified account depending on the
  // migration state.
  // TODO(https://crbug.com/883272): Remove once all platform have migrated to
  // the new account_id based on gaia (currently, only Chrome OS remains).
  CoreAccountId PickAccountIdForAccount(const std::string& gaia,
                                        const std::string& email) const;

  // Possible values for the account ID migration state, needs to be kept in
  // sync with AccountTrackerService::AccountIdMigrationState.
  enum AccountIdMigrationState {
    MIGRATION_NOT_STARTED = 0,
    MIGRATION_IN_PROGRESS = 1,
    MIGRATION_DONE = 2,
    NUM_MIGRATION_STATES
  };

  // Returns the currently saved state for the migration of accounts IDs.
  AccountIdMigrationState GetAccountIdMigrationState() const;

#if !defined(OS_IOS) && !defined(OS_ANDROID)
  // Explicitly triggers the loading of accounts in the context of supervised
  // users.
  // TODO(https://crbug.com/860492): Remove this method when supervised users
  // support is eliminated.
  void DeprecatedLoadCredentialsForSupervisedUser(
      const CoreAccountId& primary_account_id);
#endif

  // Returns pointer to the object used to obtain diagnostics about the internal
  // state of IdentityManager.
  DiagnosticsProvider* GetDiagnosticsProvider();

#if defined(OS_IOS)
  // Forces the processing of GaiaCookieManagerService::OnCookieChange. On
  // iOS, it's necessary to force-trigger the processing of cookie changes
  // from the client as the normal mechanism for internally observing them
  // is not wired up.
  // TODO(https://crbug.com/930582) : Remove the need to expose this method
  // or move it to the network::CookieManager.
  void ForceTriggerOnCookieChange();
#endif

#if defined(OS_ANDROID)
  // Reloads the accounts in the token service from the system accounts. This
  // API calls ProfileOAuth2TokenServiceDelegate::ReloadAccountsFromSystem and
  // it triggers platform specific implementation for Android. NOTE: In normal
  // usage, this method SHOULD NOT be called.
  // TODO(https://crbug.com/930094): Eliminate the need to expose this.
  void LegacyReloadAccountsFromSystem();

  // Returns a pointer to the AccountTrackerService Java instance associated
  // with this object.
  // TODO(https://crbug.com/934688): Eliminate this method once
  // AccountTrackerService.java has no more client usage.
  base::android::ScopedJavaLocalRef<jobject>
  LegacyGetAccountTrackerServiceJavaObject();

  // Returns a pointer to the OAuth2TokenService Java instance associated
  // with this object.
  // TODO(https://crbug.com/934688): Eliminate this method once
  // OAuth2TokenService.java has no more client usage.
  base::android::ScopedJavaLocalRef<jobject>
  LegacyGetOAuth2TokenServiceJavaObject();

  // This method has the contractual assumption that the account is a known
  // account and has as its semantics that it fetches the account info for the
  // account, triggering an OnExtendedAccountInfoUpdated() callback if the info
  // was successfully fetched.
  void ForceRefreshOfExtendedAccountInfo(const CoreAccountId& account_id);
#endif

 private:
  // These test helpers need to use some of the private methods below.
  friend CoreAccountInfo SetPrimaryAccount(IdentityManager* identity_manager,
                                           const std::string& email);
  friend void SetRefreshTokenForPrimaryAccount(
      IdentityManager* identity_manager,
      const std::string& token_value);
  friend void SetInvalidRefreshTokenForPrimaryAccount(
      IdentityManager* identity_manager);
  friend void RemoveRefreshTokenForPrimaryAccount(
      IdentityManager* identity_manager);
  friend AccountInfo MakePrimaryAccountAvailable(
      IdentityManager* identity_manager,
      const std::string& email);
  friend void ClearPrimaryAccount(IdentityManager* identity_manager,
                                  ClearPrimaryAccountPolicy policy);
  friend AccountInfo MakeAccountAvailable(IdentityManager* identity_manager,
                                          const std::string& email);
  friend AccountInfo MakeAccountAvailableWithCookies(
      IdentityManager* identity_manager,
      network::TestURLLoaderFactory* test_url_loader_factory,
      const std::string& email,
      const std::string& gaia_id);
  friend void SetRefreshTokenForAccount(IdentityManager* identity_manager,
                                        const std::string& account_id,
                                        const std::string& token_value);
  friend void SetInvalidRefreshTokenForAccount(
      IdentityManager* identity_manager,
      const std::string& account_id);
  friend void RemoveRefreshTokenForAccount(IdentityManager* identity_manager,
                                           const std::string& account_id);
  friend void UpdateAccountInfoForAccount(IdentityManager* identity_manager,
                                          AccountInfo account_info);
  friend void SetFreshnessOfAccountsInGaiaCookie(
      IdentityManager* identity_manager,
      bool accounts_are_fresh);
  friend void UpdatePersistentErrorOfRefreshTokenForAccount(
      IdentityManager* identity_manager,
      const std::string& account_id,
      const GoogleServiceAuthError& auth_error);

  friend void DisableAccessTokenFetchRetries(IdentityManager* identity_manager);

  friend void CancelAllOngoingGaiaCookieOperations(
      IdentityManager* identity_manager);

  friend void SetCookieAccounts(
      IdentityManager* identity_manager,
      network::TestURLLoaderFactory* test_url_loader_factory,
      const std::vector<CookieParamsForTest>& cookie_accounts);

  friend void SimulateSuccessfulFetchOfAccountInfo(
      IdentityManager* identity_manager,
      const std::string& account_id,
      const std::string& email,
      const std::string& gaia,
      const std::string& hosted_domain,
      const std::string& full_name,
      const std::string& given_name,
      const std::string& locale,
      const std::string& picture_url);

  // These friends are temporary during the conversion process.
  // TODO(https://crbug.com/889902): Delete this when conversion is done.
  friend SigninManagerAndroid;

  // Temporary access to getters (e.g. GetTokenService()).
  // TODO(https://crbug.com/944127): Remove this friendship by
  // extending identity_test_utils.h as needed.
  friend IdentityTestEnvironment;

  // IdentityManagerTest reaches into IdentityManager internals in
  // order to drive its behavior.
  // TODO(https://crbug.com/943135): Find a better way to accomplish this.
  friend IdentityManagerTest;
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           PrimaryAccountInfoAfterSigninAndAccountRemoval);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           PrimaryAccountInfoAfterSigninAndRefreshTokenRemoval);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest, RemoveAccessTokenFromCache);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CreateAccessTokenFetcherWithCustomURLLoaderFactory);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest, ObserveAccessTokenFetch);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           ObserveAccessTokenRequestCompletionWithRefreshToken);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           BatchChangeObserversAreNotifiedOnCredentialsUpdate);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest, RemoveAccessTokenFromCache);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CreateAccessTokenFetcherWithCustomURLLoaderFactory);
  FRIEND_TEST_ALL_PREFIXES(
      IdentityManagerTest,
      CallbackSentOnUpdateToAccountsInCookieWithNoAccounts);
  FRIEND_TEST_ALL_PREFIXES(
      IdentityManagerTest,
      CallbackSentOnUpdateToAccountsInCookieWithOneAccount);
  FRIEND_TEST_ALL_PREFIXES(
      IdentityManagerTest,
      CallbackSentOnUpdateToAccountsInCookieWithTwoAccounts);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CallbackSentOnUpdateToSignOutAccountsInCookie);
  FRIEND_TEST_ALL_PREFIXES(
      IdentityManagerTest,
      CallbackSentOnUpdateToAccountsInCookieWithStaleAccounts);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CallbackSentOnSuccessfulAdditionOfAccountToCookie);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CallbackSentOnFailureAdditionOfAccountToCookie);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CallbackSentOnSetAccountsInCookieCompleted_Success);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CallbackSentOnSetAccountsInCookieCompleted_Failure);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           CallbackSentOnAccountsCookieDeletedByUserAction);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest, OnNetworkInitialized);
  FRIEND_TEST_ALL_PREFIXES(IdentityManagerTest,
                           ForceRefreshOfExtendedAccountInfo);

  // Private getters used for testing only (i.e. see identity_test_utils.h).
  PrimaryAccountManager* GetPrimaryAccountManager();
  ProfileOAuth2TokenService* GetTokenService();
  AccountTrackerService* GetAccountTrackerService();
  AccountFetcherService* GetAccountFetcherService();
  GaiaCookieManagerService* GetGaiaCookieManagerService();

  // Populates and returns an AccountInfo object corresponding to |account_id|,
  // which must be an account with a refresh token.
  AccountInfo GetAccountInfoForAccountWithRefreshToken(
      const CoreAccountId& account_id) const;

  // Sets primary account to |account_info| and updates the unconsented primary
  // account.
  void SetPrimaryAccountInternal(base::Optional<CoreAccountInfo> account_info);

  // Updates the cached version of unconsented primary account and notifies the
  // observers if there is any change.
  void UpdateUnconsentedPrimaryAccount();

  // Figures out and returns the current unconsented primary account based on
  // current cookies.
  base::Optional<CoreAccountInfo> ComputeUnconsentedPrimaryAccountInfo() const;

  // PrimaryAccountManager callbacks:
  void GoogleSigninSucceeded(const AccountInfo& account_info);
  void GoogleSignedOut(const AccountInfo& account_info);
  void AuthenticatedAccountSet(const AccountInfo& account_info);
  void AuthenticatedAccountCleared();

  // OAuth2TokenServiceObserver:
  void OnRefreshTokenAvailable(const CoreAccountId& account_id) override;
  void OnRefreshTokenRevoked(const CoreAccountId& account_id) override;
  void OnRefreshTokensLoaded() override;
  void OnEndBatchChanges() override;
  void OnAuthErrorChanged(const CoreAccountId& account_id,
                          const GoogleServiceAuthError& auth_error) override;

  // GaiaCookieManagerService callbacks:
  void OnGaiaAccountsInCookieUpdated(
      const std::vector<gaia::ListedAccount>& signed_in_accounts,
      const std::vector<gaia::ListedAccount>& signed_out_accounts,
      const GoogleServiceAuthError& error);
  void OnGaiaCookieDeletedByUserAction();

  // OAuth2AccessTokenManager::DiagnosticsObserver
  void OnAccessTokenRequested(const CoreAccountId& account_id,
                              const std::string& consumer_id,
                              const identity::ScopeSet& scopes) override;
  void OnFetchAccessTokenComplete(const CoreAccountId& account_id,
                                  const std::string& consumer_id,
                                  const identity::ScopeSet& scopes,
                                  GoogleServiceAuthError error,
                                  base::Time expiration_time) override;
  void OnAccessTokenRemoved(const CoreAccountId& account_id,
                            const identity::ScopeSet& scopes) override;

  // ProfileOAuth2TokenService callbacks:
  void OnRefreshTokenAvailableFromSource(const CoreAccountId& account_id,
                                         bool is_refresh_token_valid,
                                         const std::string& source);
  void OnRefreshTokenRevokedFromSource(const CoreAccountId& account_id,
                                       const std::string& source);

  // AccountTrackerService callbacks:
  void OnAccountUpdated(const AccountInfo& info);
  void OnAccountRemoved(const AccountInfo& info);

  // Backing signin classes.
  std::unique_ptr<AccountTrackerService> account_tracker_service_;
  std::unique_ptr<ProfileOAuth2TokenService> token_service_;
  std::unique_ptr<GaiaCookieManagerService> gaia_cookie_manager_service_;
  std::unique_ptr<PrimaryAccountManager> primary_account_manager_;
  std::unique_ptr<AccountFetcherService> account_fetcher_service_;

  // PrimaryAccountMutator instance. May be null if mutation of the primary
  // account state is not supported on the current platform.
  std::unique_ptr<PrimaryAccountMutator> primary_account_mutator_;

  // AccountsMutator instance. May be null if mutation of accounts is not
  // supported on the current platform.
  std::unique_ptr<AccountsMutator> accounts_mutator_;

  // AccountsCookieMutator instance. Guaranteed to be non-null, as this
  // functionality is supported on all platforms.
  std::unique_ptr<AccountsCookieMutator> accounts_cookie_mutator_;

  // DiagnosticsProvider instance.
  std::unique_ptr<DiagnosticsProvider> diagnostics_provider_;

  // DeviceAccountsSynchronizer instance.
  std::unique_ptr<DeviceAccountsSynchronizer> device_accounts_synchronizer_;

  // Lists of observers.
  // Makes sure lists are empty on destruction.
  base::ObserverList<Observer, true>::Unchecked observer_list_;
  base::ObserverList<DiagnosticsObserver, true>::Unchecked
      diagnostics_observer_list_;

  // If |primary_account_| is set, it must equal |unconsented_primary_account_|.
  base::Optional<CoreAccountInfo> primary_account_;
  base::Optional<CoreAccountInfo> unconsented_primary_account_;

  DISALLOW_COPY_AND_ASSIGN(IdentityManager);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MANAGER_H_
