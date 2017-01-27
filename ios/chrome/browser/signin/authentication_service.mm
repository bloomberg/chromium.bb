// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/authentication_service.h"

#import <UIKit/UIKit.h>

#include "base/auto_reset.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"
#include "components/sync/driver/sync_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#import "ios/chrome/browser/signin/browser_state_data_remover.h"
#include "ios/chrome/browser/signin/constants.h"
#include "ios/chrome/browser/signin/oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/signin/signin_util.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

namespace {

// Enum describing the different sync states per login methods.
enum LoginMethodAndSyncState {
  // Legacy values retained to keep definitions in histograms.xml in sync.
  CLIENT_LOGIN_SYNC_OFF,
  CLIENT_LOGIN_SYNC_ON,
  SHARED_AUTHENTICATION_SYNC_OFF,
  SHARED_AUTHENTICATION_SYNC_ON,
  // NOTE: Add new login methods and sync states only immediately above this
  // line. Also, make sure the enum list in tools/histogram/histograms.xml is
  // updated with any change in here.
  LOGIN_METHOD_AND_SYNC_STATE_COUNT
};

// A fake account id used in the list of last signed in accounts when migrating
// an email for which the corresponding account was removed.
const char* kFakeAccountIdForRemovedAccount = "0000000000000";

// Returns the account id associated with |identity|.
std::string ChromeIdentityToAccountID(ios::ChromeBrowserState* browser_state,
                                      ChromeIdentity* identity) {
  AccountTrackerService* account_tracker =
      ios::AccountTrackerServiceFactory::GetForBrowserState(browser_state);
  std::string gaia_id = base::SysNSStringToUTF8([identity gaiaID]);
  return account_tracker->FindAccountInfoByGaiaId(gaia_id).account_id;
}

}  // namespace

AuthenticationService::AuthenticationService(
    ios::ChromeBrowserState* browser_state,
    ProfileOAuth2TokenService* token_service,
    SyncSetupService* sync_setup_service)
    : browser_state_(browser_state),
      token_service_(token_service),
      sync_setup_service_(sync_setup_service),
      have_accounts_changed_(false),
      is_in_foreground_(false),
      is_reloading_credentials_(false),
      identity_service_observer_(this),
      weak_pointer_factory_(this) {
  DCHECK(browser_state_);
  DCHECK(sync_setup_service_);
  token_service_->AddObserver(this);
}

AuthenticationService::~AuthenticationService() {
  token_service_->RemoveObserver(this);
}

// static
void AuthenticationService::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kSigninSharedAuthenticationUserId,
                               std::string());
  registry->RegisterBooleanPref(prefs::kSigninShouldPromptForSigninAgain,
                                false);
  registry->RegisterListPref(prefs::kSigninLastAccounts);
  registry->RegisterBooleanPref(prefs::kSigninLastAccountsMigrated, false);
}

void AuthenticationService::Initialize() {
  MigrateAccountsStoredInPrefsIfNeeded();

  HandleForgottenIdentity(nil, true /* should_prompt */);

  bool isSignedIn = IsAuthenticated();
  if (isSignedIn) {
    if (!sync_setup_service_->HasFinishedInitialSetup()) {
      // Sign out the user if sync was not configured after signing
      // in (see PM comments in http://crbug.com/339831 ).
      SignOut(signin_metrics::ABORT_SIGNIN, nil);
      SetPromptForSignIn(true);
      isSignedIn = false;
    }
  }
  breakpad_helper::SetCurrentlySignedIn(isSignedIn);

  OnApplicationEnterForeground();

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  foreground_observer_.reset(
      [[center addObserverForName:UIApplicationWillEnterForegroundNotification
                           object:nil
                            queue:nil
                       usingBlock:^(NSNotification* notification) {
                         OnApplicationEnterForeground();
                       }] retain]);
  background_observer_.reset(
      [[center addObserverForName:UIApplicationDidEnterBackgroundNotification
                           object:nil
                            queue:nil
                       usingBlock:^(NSNotification* notification) {
                         OnApplicationEnterBackground();
                       }] retain]);

  identity_service_observer_.Add(
      ios::GetChromeBrowserProvider()->GetChromeIdentityService());
}

void AuthenticationService::Shutdown() {
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center removeObserver:foreground_observer_];
  [center removeObserver:background_observer_];
}

void AuthenticationService::OnApplicationEnterForeground() {
  if (is_in_foreground_) {
    return;
  }

  // A change might have happened while in background, and SSOAuth didn't send
  // the corresponding notifications yet. Reload the credentials to catch up
  // with potentials changes.
  ReloadCredentialsFromIdentities(true /* should_prompt */);

  // Set |is_in_foreground_| only after handling forgotten identity.
  // This ensures that any changes made to the SSOAuth identities before this
  // are correctly seen as made while in background.
  is_in_foreground_ = true;

  // Accounts might have changed while the AuthenticationService was in
  // background. Check whether they changed, then store the current accounts.
  ComputeHaveAccountsChanged();
  StoreAccountsInPrefs();

  if (IsAuthenticated()) {
    bool sync_enabled = sync_setup_service_->IsSyncEnabled();
    LoginMethodAndSyncState loginMethodAndSyncState =
        sync_enabled ? SHARED_AUTHENTICATION_SYNC_ON
                     : SHARED_AUTHENTICATION_SYNC_OFF;
    UMA_HISTOGRAM_ENUMERATION("Signin.IOSLoginMethodAndSyncState",
                              loginMethodAndSyncState,
                              LOGIN_METHOD_AND_SYNC_STATE_COUNT);
  }
  UMA_HISTOGRAM_COUNTS_100("Signin.IOSNumberOfDeviceAccounts",
                           [ios::GetChromeBrowserProvider()
                                   ->GetChromeIdentityService()
                                   ->GetAllIdentities() count]);

  // Clear signin errors on the accounts that had a specific MDM device status.
  // This will trigger services to fetch data for these accounts again.
  ProfileOAuth2TokenServiceIOSDelegate* token_service_delegate =
      static_cast<ProfileOAuth2TokenServiceIOSDelegate*>(
          token_service_->GetDelegate());
  std::map<std::string, base::scoped_nsobject<NSDictionary>> cached_mdm_infos(
      cached_mdm_infos_);
  cached_mdm_infos_.clear();
  for (const auto& cached_mdm_info : cached_mdm_infos) {
    token_service_delegate->AddOrUpdateAccount(cached_mdm_info.first);
  }
}

void AuthenticationService::OnApplicationEnterBackground() {
  is_in_foreground_ = false;
}

void AuthenticationService::SetPromptForSignIn(bool should_prompt) {
  if (ShouldPromptForSignIn() != should_prompt) {
    browser_state_->GetPrefs()->SetBoolean(
        prefs::kSigninShouldPromptForSigninAgain, should_prompt);
  }
}

bool AuthenticationService::ShouldPromptForSignIn() {
  return browser_state_->GetPrefs()->GetBoolean(
      prefs::kSigninShouldPromptForSigninAgain);
}

void AuthenticationService::ComputeHaveAccountsChanged() {
  // Reload credentials to ensure the accounts from the token service are
  // up-to-date.
  // While the AuthenticationService is in background, changes should be shown
  // to the user and |should_prompt| is true.
  ReloadCredentialsFromIdentities(!is_in_foreground_ /* should_prompt */);
  std::vector<std::string> newAccounts = token_service_->GetAccounts();
  std::vector<std::string> oldAccounts = GetAccountsInPrefs();
  std::sort(newAccounts.begin(), newAccounts.end());
  std::sort(oldAccounts.begin(), oldAccounts.end());
  have_accounts_changed_ = oldAccounts != newAccounts;
}

bool AuthenticationService::HaveAccountsChanged() {
  if (!is_in_foreground_) {
    // While AuthenticationService is in background, the value can change
    // without warning and needs to be recomputed every time.
    ComputeHaveAccountsChanged();
  }
  return have_accounts_changed_;
}

void AuthenticationService::MigrateAccountsStoredInPrefsIfNeeded() {
  AccountTrackerService* account_tracker =
      ios::AccountTrackerServiceFactory::GetForBrowserState(browser_state_);
  if (account_tracker->GetMigrationState() ==
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    return;
  }
  DCHECK_EQ(AccountTrackerService::MIGRATION_DONE,
            account_tracker->GetMigrationState());
  if (browser_state_->GetPrefs()->GetBoolean(
          prefs::kSigninLastAccountsMigrated)) {
    // Already migrated.
    return;
  }

  std::vector<std::string> emails = GetAccountsInPrefs();
  base::ListValue accounts_pref_value;
  for (const std::string& email : emails) {
    AccountInfo account_info = account_tracker->FindAccountInfoByEmail(email);
    if (!account_info.email.empty()) {
      DCHECK(!account_info.gaia.empty());
      accounts_pref_value.AppendString(account_info.account_id);
    } else {
      // The account for |email| was removed since the last application cold
      // start. Insert |kFakeAccountIdForRemovedAccount| to ensure
      // |have_accounts_changed_| will be set to true and the removal won't be
      // silently ignored.
      accounts_pref_value.AppendString(kFakeAccountIdForRemovedAccount);
    }
  }
  browser_state_->GetPrefs()->Set(prefs::kSigninLastAccounts,
                                  accounts_pref_value);
  browser_state_->GetPrefs()->SetBoolean(prefs::kSigninLastAccountsMigrated,
                                         true);
}

void AuthenticationService::StoreAccountsInPrefs() {
  std::vector<std::string> accounts(token_service_->GetAccounts());
  base::ListValue accountsPrefValue;
  for (const std::string& account : accounts)
    accountsPrefValue.AppendString(account);
  browser_state_->GetPrefs()->Set(prefs::kSigninLastAccounts,
                                  accountsPrefValue);
}

std::vector<std::string> AuthenticationService::GetAccountsInPrefs() {
  std::vector<std::string> accounts;
  const base::ListValue* accountsPref =
      browser_state_->GetPrefs()->GetList(prefs::kSigninLastAccounts);
  for (size_t i = 0; i < accountsPref->GetSize(); ++i) {
    std::string account;
    if (accountsPref->GetString(i, &account) && !account.empty()) {
      accounts.push_back(account);
    } else {
      NOTREACHED();
    }
  }
  return accounts;
}

ChromeIdentity* AuthenticationService::GetAuthenticatedIdentity() {
  // There is no authenticated identity if there is no signed in user or if the
  // user signed in via the client login flow.
  if (!IsAuthenticated())
    return nil;
  std::string authenticatedAccountId =
      ios::SigninManagerFactory::GetForBrowserState(browser_state_)
          ->GetAuthenticatedAccountId();

  AccountTrackerService* accountTracker =
      ios::AccountTrackerServiceFactory::GetForBrowserState(browser_state_);
  std::string authenticatedGaiaId =
      accountTracker->GetAccountInfo(authenticatedAccountId).gaia;
  if (authenticatedGaiaId.empty())
    return nil;

  return ios::GetChromeBrowserProvider()
      ->GetChromeIdentityService()
      ->GetIdentityWithGaiaID(authenticatedGaiaId);
}

void AuthenticationService::SignIn(ChromeIdentity* identity,
                                   const std::string& hosted_domain) {
  DCHECK(ios::GetChromeBrowserProvider()
             ->GetChromeIdentityService()
             ->IsValidIdentity(identity));

  // The account info needs to be seeded for the primary account id before
  // signing in.
  // TODO(msarda): http://crbug.com/478770 Seed account information for
  // all secondary accounts.
  AccountTrackerService* accountTracker =
      ios::AccountTrackerServiceFactory::GetForBrowserState(browser_state_);
  AccountInfo info;
  info.gaia = base::SysNSStringToUTF8([identity gaiaID]);
  info.email = GetCanonicalizedEmailForIdentity(identity);
  info.hosted_domain = hosted_domain;
  std::string newAuthenticatedAccountId = accountTracker->SeedAccountInfo(info);
  std::string oldAuthenticatedAccountId =
      ios::SigninManagerFactory::GetForBrowserState(browser_state_)
          ->GetAuthenticatedAccountId();
  // |SigninManager::SetAuthenticatedAccountId| simply ignores the call if
  // there is already a signed in user. Check that there is no signed in account
  // or that the new signed in account matches the old one to avoid a mismatch
  // between the old and the new authenticated accounts.
  if (!oldAuthenticatedAccountId.empty())
    CHECK_EQ(newAuthenticatedAccountId, oldAuthenticatedAccountId);

  SetPromptForSignIn(false);
  sync_setup_service_->PrepareForFirstSyncSetup();

  // Update the SigninManager with the new logged in identity.
  std::string newAuthenticatedUsername =
      accountTracker->GetAccountInfo(newAuthenticatedAccountId).email;
  ios::SigninManagerFactory::GetForBrowserState(browser_state_)
      ->OnExternalSigninCompleted(newAuthenticatedUsername);

  // Reload all credentials to match the desktop model. Exclude all the
  // accounts ids that are the primary account ids on other profiles.
  ProfileOAuth2TokenServiceIOSDelegate* tokenServiceDelegate =
      static_cast<ProfileOAuth2TokenServiceIOSDelegate*>(
          token_service_->GetDelegate());
  tokenServiceDelegate->ReloadCredentials(newAuthenticatedAccountId);
  StoreAccountsInPrefs();

  // Kick-off sync: The authentication error UI (sign in infobar and warning
  // badge in settings screen) check the sync auth error state. Sync
  // needs to be kicked off so that it resets the auth error quickly once
  // |identity| is reauthenticated.
  // TODO(msarda): Remove this code once the authentication error UI checks
  // SigninGlobalError instead of the sync auth error state.
  // crbug.com/289493
  syncer::SyncService* sync_service =
      IOSChromeProfileSyncServiceFactory::GetForBrowserState(browser_state_);
  sync_service->RequestStart();
  breakpad_helper::SetCurrentlySignedIn(true);
}

void AuthenticationService::SignOut(
    signin_metrics::ProfileSignout signout_source,
    ProceduralBlock completion) {
  if (!IsAuthenticated()) {
    if (completion)
      completion();
    return;
  }

  bool is_managed = IsAuthenticatedIdentityManaged();

  IOSChromeProfileSyncServiceFactory::GetForBrowserState(browser_state_)
      ->RequestStop(syncer::SyncService::CLEAR_DATA);
  ios::SigninManagerFactory::GetForBrowserState(browser_state_)
      ->SignOut(signout_source, signin_metrics::SignoutDelete::IGNORE_METRIC);
  breakpad_helper::SetCurrentlySignedIn(false);
  cached_mdm_infos_.clear();
  if (is_managed) {
    BrowserStateDataRemover::ClearData(browser_state_, completion);
  } else if (completion) {
    completion();
  }
}

NSDictionary* AuthenticationService::GetCachedMDMInfo(
    ChromeIdentity* identity) {
  auto it = cached_mdm_infos_.find(
      ChromeIdentityToAccountID(browser_state_, identity));

  if (it == cached_mdm_infos_.end()) {
    return nil;
  }

  ProfileOAuth2TokenServiceIOSDelegate* token_service_delegate =
      static_cast<ProfileOAuth2TokenServiceIOSDelegate*>(
          token_service_->GetDelegate());
  if (!token_service_delegate->RefreshTokenHasError(it->first)) {
    // Account has no error, invalidate the cache.
    cached_mdm_infos_.erase(it);
    return nil;
  }

  return it->second.get();
}

bool AuthenticationService::HasCachedMDMErrorForIdentity(
    ChromeIdentity* identity) {
  return GetCachedMDMInfo(identity) != nil;
}

bool AuthenticationService::ShowMDMErrorDialogForIdentity(
    ChromeIdentity* identity) {
  NSDictionary* cached_info = GetCachedMDMInfo(identity);
  if (!cached_info) {
    return false;
  }

  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  identity_service->HandleMDMNotification(identity, cached_info, ^(bool){
                                                    });
  return true;
}

void AuthenticationService::ResetChromeIdentityServiceObserverForTesting() {
  identity_service_observer_.RemoveAll();
  identity_service_observer_.Add(
      ios::GetChromeBrowserProvider()->GetChromeIdentityService());
}

base::WeakPtr<AuthenticationService> AuthenticationService::GetWeakPtr() {
  return weak_pointer_factory_.GetWeakPtr();
}

void AuthenticationService::OnEndBatchChanges() {
  if (is_in_foreground_) {
    // Accounts maybe have been excluded or included from the current browser
    // state, without any change to the identity list.
    // Store the current list of accounts to make sure it is up-to-date.
    StoreAccountsInPrefs();
  }
}

void AuthenticationService::OnIdentityListChanged() {
  // The list of identities may change while in an authorized call. Signing out
  // the authenticated user at this time may lead to crashes (e.g.
  // http://crbug.com/398431 ).
  // Handle the change of the identity list on the next message loop cycle.
  // If the identity list changed while the authentication service was in
  // background, the user should be warned about it.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AuthenticationService::HandleIdentityListChanged,
                            base::Unretained(this), !is_in_foreground_));
}

bool AuthenticationService::HandleMDMNotification(ChromeIdentity* identity,
                                                  NSDictionary* user_info) {
  if (!experimental_flags::IsMDMIntegrationEnabled()) {
    return false;
  }

  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  ios::MDMDeviceStatus status = identity_service->GetMDMDeviceStatus(user_info);
  NSDictionary* cached_info = GetCachedMDMInfo(identity);

  if (cached_info &&
      identity_service->GetMDMDeviceStatus(cached_info) == status) {
    // Same status as the last error, ignore it to avoid spamming users.
    return false;
  }

  ios::MDMStatusCallback callback = ^(bool is_blocked) {
    if (is_blocked) {
      // If the identiy is blocked, sign out of the account. As only managed
      // account can be blocked, this will clear the associated browsing data.
      if (identity == GetAuthenticatedIdentity()) {
        SignOut(signin_metrics::ABORT_SIGNIN, nil);
      }
    }
  };
  if (identity_service->HandleMDMNotification(identity, user_info, callback)) {
    cached_mdm_infos_[ChromeIdentityToAccountID(browser_state_, identity)]
        .reset([user_info retain]);
    return true;
  }
  return false;
}

void AuthenticationService::OnAccessTokenRefreshFailed(
    ChromeIdentity* identity,
    NSDictionary* user_info) {
  if (HandleMDMNotification(identity, user_info)) {
    return;
  }

  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  if (!identity_service->IsInvalidGrantError(user_info)) {
    // If the failure is not due to an invalid grant, the identity is not
    // invalid and there is nothing to do.
    return;
  }

  // Handle the failure of access token refresh on the next message loop cycle.
  // |identity| is now invalid and the authentication service might need to
  // react to this loss of identity.
  // Note that no reload of the credentials is necessary here, as |identity|
  // might still be accessible in SSO, and |OnIdentityListChanged| will handle
  // this when |identity| will actually disappear from SSO.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AuthenticationService::HandleForgottenIdentity,
                            base::Unretained(this), identity, true));
}

void AuthenticationService::OnChromeIdentityServiceWillBeDestroyed() {
  identity_service_observer_.RemoveAll();
}

void AuthenticationService::HandleIdentityListChanged(bool should_prompt) {
  ReloadCredentialsFromIdentities(should_prompt);

  if (is_in_foreground_) {
    // Update the accounts currently stored in the profile prefs.
    StoreAccountsInPrefs();
  }
}

void AuthenticationService::HandleForgottenIdentity(
    ChromeIdentity* invalid_identity,
    bool should_prompt) {
  if (!IsAuthenticated()) {
    // User is not signed in. Nothing to do here.
    return;
  }

  ChromeIdentity* authenticated_identity = GetAuthenticatedIdentity();
  if (authenticated_identity && authenticated_identity != invalid_identity) {
    // |authenticated_identity| exists and is a valid identity. Nothing to do
    // here.
    return;
  }

  // Sign the user out.
  //
  // The authenticated id is removed from the device (either by the user or
  // when an invalid credentials is received from the server). There is no
  // upstream entry in enum |signin_metrics::ProfileSignout| for this event. The
  // temporary solution is to map this to |ABORT_SIGNIN|.
  //
  // TODO(msarda): http://crbug.com/416823 Add another entry in Chromium
  // upstream for |signin_metrics| that matches the device identity was lost.
  SignOut(signin_metrics::ABORT_SIGNIN, nil);
  SetPromptForSignIn(should_prompt);
}

void AuthenticationService::ReloadCredentialsFromIdentities(
    bool should_prompt) {
  if (is_reloading_credentials_) {
    return;
  }
  base::AutoReset<bool> is_reloading_credentials(&is_reloading_credentials_,
                                                 true);
  HandleForgottenIdentity(nil, should_prompt);
  if (GetAuthenticatedUserEmail()) {
    ProfileOAuth2TokenServiceIOSDelegate* token_service_delegate =
        static_cast<ProfileOAuth2TokenServiceIOSDelegate*>(
            token_service_->GetDelegate());
    token_service_delegate->ReloadCredentials();
  }
}

bool AuthenticationService::IsAuthenticated() {
  if (!is_in_foreground_) {
    // While AuthenticationService is in background, the list of accounts can
    // change without a OnIdentityListChanged notification being fired.
    // Reload credentials to ensure that the user is still authenticated.
    ReloadCredentialsFromIdentities(true /* should_prompt */);
  }
  return ios::SigninManagerFactory::GetForBrowserState(browser_state_)
      ->IsAuthenticated();
}

NSString* AuthenticationService::GetAuthenticatedUserEmail() {
  if (!IsAuthenticated())
    return nil;
  std::string authenticatedUsername =
      ios::SigninManagerFactory::GetForBrowserState(browser_state_)
          ->GetAuthenticatedAccountInfo()
          .email;
  DCHECK_LT(0U, authenticatedUsername.length());
  return base::SysUTF8ToNSString(authenticatedUsername);
}

bool AuthenticationService::IsAuthenticatedIdentityManaged() {
  if (!experimental_flags::IsMDMIntegrationEnabled()) {
    // Behavior based on whether an account is managed are only enabled if MDM
    // integration is enabled.
    return false;
  }
  std::string hosted_domain =
      ios::SigninManagerFactory::GetForBrowserState(browser_state_)
          ->GetAuthenticatedAccountInfo()
          .hosted_domain;
  return !hosted_domain.empty() &&
         hosted_domain != AccountTrackerService::kNoHostedDomainFound;
}
