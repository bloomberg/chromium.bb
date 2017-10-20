// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_reconcilor.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"

namespace {

// String used for source parameter in GAIA cookie manager calls.
const char kSource[] = "ChromiumAccountReconcilor";

class AccountEqualToFunc {
 public:
  explicit AccountEqualToFunc(const gaia::ListedAccount& account)
      : account_(account) {}
  bool operator()(const gaia::ListedAccount& other) const;

 private:
  gaia::ListedAccount account_;
};

bool AccountEqualToFunc::operator()(const gaia::ListedAccount& other) const {
  return account_.valid == other.valid && account_.id == other.id;
}

gaia::ListedAccount AccountForId(const std::string& account_id) {
  gaia::ListedAccount account;
  account.id = account_id;
  account.gaia_id = std::string();
  account.email = std::string();
  account.valid = true;
  return account;
}

}  // namespace

AccountReconcilor::Lock::Lock(AccountReconcilor* reconcilor)
    : reconcilor_(reconcilor) {
  DCHECK(reconcilor_);
  reconcilor_->IncrementLockCount();
}

AccountReconcilor::Lock::~Lock() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  reconcilor_->DecrementLockCount();
}

AccountReconcilor::AccountReconcilor(
    ProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    SigninClient* client,
    GaiaCookieManagerService* cookie_manager_service)
    : token_service_(token_service),
      signin_manager_(signin_manager),
      client_(client),
      cookie_manager_service_(cookie_manager_service),
      registered_with_token_service_(false),
      registered_with_cookie_manager_service_(false),
      registered_with_content_settings_(false),
      is_reconcile_started_(false),
      first_execution_(true),
      error_during_last_reconcile_(false),
      chrome_accounts_changed_(false),
      account_reconcilor_lock_count_(0),
      reconcile_on_unblock_(false) {
  VLOG(1) << "AccountReconcilor::AccountReconcilor";
}

AccountReconcilor::~AccountReconcilor() {
  VLOG(1) << "AccountReconcilor::~AccountReconcilor";
  // Make sure shutdown was called first.
  DCHECK(!registered_with_token_service_);
  DCHECK(!registered_with_cookie_manager_service_);
}

void AccountReconcilor::Initialize(bool start_reconcile_if_tokens_available) {
  VLOG(1) << "AccountReconcilor::Initialize";
  RegisterWithSigninManager();

  if (IsEnabled()) {
    RegisterWithCookieManagerService();
    RegisterWithContentSettings();
    RegisterWithTokenService();

    // Start a reconcile if the tokens are already loaded.
    if (start_reconcile_if_tokens_available && IsTokenServiceReady())
      StartReconcile();
  }
}

void AccountReconcilor::Shutdown() {
  VLOG(1) << "AccountReconcilor::Shutdown";
  UnregisterWithCookieManagerService();
  UnregisterWithTokenService();
  UnregisterWithContentSettings();
  UnregisterWithSigninManager();
}

void AccountReconcilor::RegisterWithSigninManager() {
  if (signin::IsDiceMigrationEnabled()) {
    // Reconcilor is always turned on when DICE is enabled. It does not need to
    // observe the SigninManager events.
    return;
  }

  VLOG(1) << "AccountReconcilor::RegisterWithSigninManager";
  signin_manager_->AddObserver(this);
}

void AccountReconcilor::UnregisterWithSigninManager() {
  if (signin::IsDiceMigrationEnabled())
    return;

  VLOG(1) << "AccountReconcilor::UnregisterWithSigninManager";
  signin_manager_->RemoveObserver(this);
}

void AccountReconcilor::RegisterWithContentSettings() {
  VLOG(1) << "AccountReconcilor::RegisterWithContentSettings";
  // During re-auth, the reconcilor will get a callback about successful signin
  // even when the profile is already connected.  Avoid re-registering
  // with the token service since this will DCHECK.
  if (registered_with_content_settings_)
    return;

  client_->AddContentSettingsObserver(this);
  registered_with_content_settings_ = true;
}

void AccountReconcilor::UnregisterWithContentSettings() {
  VLOG(1) << "AccountReconcilor::UnregisterWithContentSettings";
  if (!registered_with_content_settings_)
    return;

  client_->RemoveContentSettingsObserver(this);
  registered_with_content_settings_ = false;
}

void AccountReconcilor::RegisterWithTokenService() {
  VLOG(1) << "AccountReconcilor::RegisterWithTokenService";
  // During re-auth, the reconcilor will get a callback about successful signin
  // even when the profile is already connected.  Avoid re-registering
  // with the token service since this will DCHECK.
  if (registered_with_token_service_)
    return;

  token_service_->AddObserver(this);
  registered_with_token_service_ = true;
}

void AccountReconcilor::UnregisterWithTokenService() {
  VLOG(1) << "AccountReconcilor::UnregisterWithTokenService";
  if (!registered_with_token_service_)
    return;

  token_service_->RemoveObserver(this);
  registered_with_token_service_ = false;
}

void AccountReconcilor::RegisterWithCookieManagerService() {
  VLOG(1) << "AccountReconcilor::RegisterWithCookieManagerService";
  // During re-auth, the reconcilor will get a callback about successful signin
  // even when the profile is already connected.  Avoid re-registering
  // with the helper since this will DCHECK.
  if (registered_with_cookie_manager_service_)
    return;

  cookie_manager_service_->AddObserver(this);
  registered_with_cookie_manager_service_ = true;
}

void AccountReconcilor::UnregisterWithCookieManagerService() {
  VLOG(1) << "AccountReconcilor::UnregisterWithCookieManagerService";
  if (!registered_with_cookie_manager_service_)
    return;

  cookie_manager_service_->RemoveObserver(this);
  registered_with_cookie_manager_service_ = false;
}

bool AccountReconcilor::IsEnabled() {
  return signin_manager_->IsAuthenticated() || signin::IsDiceMigrationEnabled();
}

signin_metrics::AccountReconcilorState AccountReconcilor::GetState() {
  if (!is_reconcile_started_) {
    return error_during_last_reconcile_
               ? signin_metrics::ACCOUNT_RECONCILOR_ERROR
               : signin_metrics::ACCOUNT_RECONCILOR_OK;
  }

  return signin_metrics::ACCOUNT_RECONCILOR_RUNNING;
}

void AccountReconcilor::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AccountReconcilor::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AccountReconcilor::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  // If this is not a change to cookie settings, just ignore.
  if (content_type != CONTENT_SETTINGS_TYPE_COOKIES)
    return;

  // If this does not affect GAIA, just ignore.  If the primary pattern is
  // invalid, then assume it could affect GAIA.  The secondary pattern is
  // not needed.
  if (primary_pattern.IsValid() &&
      !primary_pattern.Matches(GaiaUrls::GetInstance()->gaia_url())) {
    return;
  }

  VLOG(1) << "AccountReconcilor::OnContentSettingChanged";
  StartReconcile();
}

void AccountReconcilor::OnEndBatchChanges() {
  VLOG(1) << "AccountReconcilor::OnEndBatchChanges. "
          << "Reconcilor state: " << is_reconcile_started_;
  // Remember that accounts have changed if a reconcile is already started.
  chrome_accounts_changed_ = is_reconcile_started_;
  StartReconcile();
}

void AccountReconcilor::OnRefreshTokensLoaded() {
  StartReconcile();
}

void AccountReconcilor::GoogleSigninSucceeded(const std::string& account_id,
                                              const std::string& username) {
  DCHECK(!signin::IsDiceMigrationEnabled());
  VLOG(1) << "AccountReconcilor::GoogleSigninSucceeded: signed in";
  RegisterWithCookieManagerService();
  RegisterWithContentSettings();
  RegisterWithTokenService();
}

void AccountReconcilor::GoogleSignedOut(const std::string& account_id,
                                        const std::string& username) {
  DCHECK(!signin::IsDiceMigrationEnabled());
  VLOG(1) << "AccountReconcilor::GoogleSignedOut: signed out";
  AbortReconcile();
  UnregisterWithCookieManagerService();
  UnregisterWithTokenService();
  UnregisterWithContentSettings();
  PerformLogoutAllAccountsAction();
}

bool AccountReconcilor::IsAccountConsistencyEnabled() {
  return signin::IsAccountConsistencyMirrorEnabled() ||
         signin::IsDiceMigrationEnabled();
}

void AccountReconcilor::PerformMergeAction(const std::string& account_id) {
  if (!IsAccountConsistencyEnabled()) {
    MarkAccountAsAddedToCookie(account_id);
    return;
  }
  VLOG(1) << "AccountReconcilor::PerformMergeAction: " << account_id;
  cookie_manager_service_->AddAccountToCookie(account_id, kSource);
}

void AccountReconcilor::PerformLogoutAllAccountsAction() {
  if (!IsAccountConsistencyEnabled())
    return;
  VLOG(1) << "AccountReconcilor::PerformLogoutAllAccountsAction";
  cookie_manager_service_->LogOutAllAccounts(kSource);
}

void AccountReconcilor::StartReconcile() {
  if (is_reconcile_started_)
    return;

  if (IsReconcileBlocked()) {
    VLOG(1) << "AccountReconcilor::StartReconcile: "
            << "Reconcile is blocked, scheduling for later.";
    // Reconcile is locked, it will be restarted when the lock count reaches 0.
    reconcile_on_unblock_ = true;
    return;
  }


  if (!IsEnabled() || !client_->AreSigninCookiesAllowed()) {
    VLOG(1) << "AccountReconcilor::StartReconcile: !enabled or no cookies";
    return;
  }

  // Do not reconcile if tokens are not loaded yet.
  if (!IsTokenServiceReady()) {
    VLOG(1)
        << "AccountReconcilor::StartReconcile: token service *not* ready yet.";
    return;
  }

  reconcile_start_time_ = base::Time::Now();
  for (auto& observer : observer_list_)
    observer.OnStartReconcile();

  // Reset state for validating gaia cookie.
  gaia_accounts_.clear();

  // Reset state for validating oauth2 tokens.
  primary_account_.clear();
  chrome_accounts_.clear();
  add_to_cookie_.clear();
  ValidateAccountsFromTokenService();

  if (primary_account_.empty() && !signin::IsDiceMigrationEnabled()) {
    VLOG(1) << "AccountReconcilor::StartReconcile: primary has error";
    return;
  }

  is_reconcile_started_ = true;
  error_during_last_reconcile_ = false;

  // ListAccounts() also gets signed out accounts but this class doesn't use
  // them.
  std::vector<gaia::ListedAccount> signed_out_accounts;

  // Rely on the GCMS to manage calls to and responses from ListAccounts.
  if (cookie_manager_service_->ListAccounts(&gaia_accounts_,
                                            &signed_out_accounts,
                                            kSource)) {
    OnGaiaAccountsInCookieUpdated(
        gaia_accounts_,
        signed_out_accounts,
        GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  }
}

void AccountReconcilor::OnGaiaAccountsInCookieUpdated(
        const std::vector<gaia::ListedAccount>& accounts,
        const std::vector<gaia::ListedAccount>& signed_out_accounts,
        const GoogleServiceAuthError& error) {
  VLOG(1) << "AccountReconcilor::OnGaiaAccountsInCookieUpdated: "
          << "CookieJar " << accounts.size() << " accounts, "
          << "Reconcilor's state is " << is_reconcile_started_ << ", "
          << "Error was " << error.ToString();
  if (error.state() == GoogleServiceAuthError::NONE) {
    gaia_accounts_ = accounts;
    is_reconcile_started_ ? FinishReconcile() : StartReconcile();
  } else {
    if (is_reconcile_started_)
      error_during_last_reconcile_ = true;
    AbortReconcile();
  }
}

void AccountReconcilor::ValidateAccountsFromTokenService() {
  primary_account_ = signin_manager_->GetAuthenticatedAccountId();
  DCHECK(signin::IsDiceMigrationEnabled() || !primary_account_.empty());

  chrome_accounts_ = token_service_->GetAccounts();

  // Remove any accounts that have an error.  There is no point in trying to
  // reconcile them, since it won't work anyway.  If the list ends up being
  // empty, or if the primary account is in error, then don't reconcile any
  // accounts.
  for (auto i = chrome_accounts_.begin(); i != chrome_accounts_.end(); ++i) {
    if (token_service_->GetDelegate()->RefreshTokenHasError(*i)) {
      if ((primary_account_ == *i) && !signin::IsDiceMigrationEnabled()) {
        primary_account_.clear();
        chrome_accounts_.clear();
        break;
      } else {
        VLOG(1) << "AccountReconcilor::ValidateAccountsFromTokenService: "
                << *i << " has error, won't reconcile";
        i->clear();
      }
    }
  }
  chrome_accounts_.erase(std::remove(chrome_accounts_.begin(),
                                     chrome_accounts_.end(),
                                     std::string()),
                         chrome_accounts_.end());

  VLOG(1) << "AccountReconcilor::ValidateAccountsFromTokenService: "
          << "Chrome " << chrome_accounts_.size() << " accounts, "
          << "Primary is '" << primary_account_ << "'";
}

void AccountReconcilor::OnNewProfileManagementFlagChanged(
    bool new_flag_status) {
  if (new_flag_status) {
    // The reconciler may have been newly created just before this call, or may
    // have already existed and in mid-reconcile. To err on the safe side, force
    // a restart.
    Shutdown();
    Initialize(true);
  } else {
    Shutdown();
  }
}

void AccountReconcilor::OnReceivedManageAccountsResponse(
    signin::GAIAServiceType service_type) {
  if (service_type == signin::GAIA_SERVICE_TYPE_ADDSESSION) {
    cookie_manager_service_->TriggerListAccounts(kSource);
  }
}

// There are several cases, depending on the account consistency method and
// whether this is the first execution. The logic can be summarized below:
// * With Mirror, always use the primary account as first Gaia account.
// * With Dice,
//   - On first execution, the candidates are examined in this order:
//     1. The primary account
//     2. The current first Gaia account
//     3. The last known first Gaia account
//     4. The first account in the token service
//   - On subsequent executions, the order is:
//     1. The current first Gaia account
//     2. The primary account
//     3. The last known first Gaia account
//     4. The first account in the token service
std::string AccountReconcilor::GetFirstGaiaAccountForReconcile() const {
  if (!signin::IsDiceMigrationEnabled()) {
    // Mirror only uses the primary account, and it is never empty.
    DCHECK(!primary_account_.empty());
    DCHECK(base::ContainsValue(chrome_accounts_, primary_account_));
    return primary_account_;
  }

  DCHECK(signin::IsDiceMigrationEnabled());
  if (chrome_accounts_.empty())
    return std::string();  // No Chrome account, log out.

  bool valid_primary_account =
      !primary_account_.empty() &&
      base::ContainsValue(chrome_accounts_, primary_account_);

  if (gaia_accounts_.empty()) {
    if (valid_primary_account)
      return primary_account_;

    // Try the last known account. This happens when the cookies are cleared
    // while Sync is disabled.
    if (base::ContainsValue(chrome_accounts_, last_known_first_account_))
      return last_known_first_account_;

    // As a last resort, use the first Chrome account.
    return chrome_accounts_[0];
  }

  const std::string& first_gaia_account = gaia_accounts_[0].id;
  bool first_gaia_account_is_valid =
      gaia_accounts_[0].valid &&
      base::ContainsValue(chrome_accounts_, first_gaia_account);

  if (!first_gaia_account_is_valid &&
      (primary_account_ == first_gaia_account)) {
    // The primary account is also the first Gaia account, and is invalid.
    // Logout everything.
    return std::string();
  }

  if (first_execution_) {
    // On first execution, try the primary account, and then the first Gaia
    // account.
    if (valid_primary_account)
      return primary_account_;
    if (first_gaia_account_is_valid)
      return first_gaia_account;
    // As a last resort, use the first Chrome account.
    return chrome_accounts_[0];
  }

  // While Chrome is running, try the first Gaia account, and then the
  // primary account.
  if (first_gaia_account_is_valid)
    return first_gaia_account;
  if (valid_primary_account)
    return primary_account_;

  // Changing the first Gaia account while Chrome is running would be
  // confusing for the user. Logout everything.
  return std::string();
}

void AccountReconcilor::FinishReconcile() {
  VLOG(1) << "AccountReconcilor::FinishReconcile";
  DCHECK(add_to_cookie_.empty());
  std::string first_account = GetFirstGaiaAccountForReconcile();
  // |first_account| must be in |chrome_accounts_|.
  DCHECK(first_account.empty() ||
         (std::find(chrome_accounts_.begin(), chrome_accounts_.end(),
                    first_account) != chrome_accounts_.end()));
  size_t number_gaia_accounts = gaia_accounts_.size();
  bool first_account_mismatch =
      (number_gaia_accounts > 0) && (first_account != gaia_accounts_[0].id);

  // If there are any accounts in the gaia cookie but not in chrome, then
  // those accounts need to be removed from the cookie.  This means we need
  // to blow the cookie away.
  int removed_from_cookie = 0;
  for (size_t i = 0; i < number_gaia_accounts; ++i) {
    if (gaia_accounts_[i].valid &&
        chrome_accounts_.end() == std::find(chrome_accounts_.begin(),
                                            chrome_accounts_.end(),
                                            gaia_accounts_[i].id)) {
      ++removed_from_cookie;
    }
  }

  bool rebuild_cookie = first_account_mismatch || (removed_from_cookie > 0);
  std::vector<gaia::ListedAccount> original_gaia_accounts =
      gaia_accounts_;
  if (rebuild_cookie) {
    VLOG(1) << "AccountReconcilor::FinishReconcile: rebuild cookie";
    // Really messed up state.  Blow away the gaia cookie completely and
    // rebuild it, making sure the primary account as specified by the
    // SigninManager is the first session in the gaia cookie.
    PerformLogoutAllAccountsAction();
    gaia_accounts_.clear();
  }

  if (first_account.empty()) {
    DCHECK(signin::IsDiceMigrationEnabled());
    // Gaia cookie has been cleared or was already empty.
    DCHECK((first_account_mismatch && rebuild_cookie) ||
           (number_gaia_accounts == 0));
    RevokeAllSecondaryTokens();
  } else {
    // Create a list of accounts that need to be added to the Gaia cookie.
    add_to_cookie_.push_back(first_account);
    for (size_t i = 0; i < chrome_accounts_.size(); ++i) {
      if (chrome_accounts_[i] != first_account)
        add_to_cookie_.push_back(chrome_accounts_[i]);
    }
  }

  // For each account known to chrome, PerformMergeAction() if the account is
  // not already in the cookie jar or its state is invalid, or signal merge
  // completed otherwise.  Make a copy of |add_to_cookie_| since calls to
  // SignalComplete() will change the array.
  std::vector<std::string> add_to_cookie_copy = add_to_cookie_;
  int added_to_cookie = 0;
  for (size_t i = 0; i < add_to_cookie_copy.size(); ++i) {
    if (gaia_accounts_.end() !=
        std::find_if(gaia_accounts_.begin(), gaia_accounts_.end(),
                     AccountEqualToFunc(AccountForId(add_to_cookie_copy[i])))) {
      cookie_manager_service_->SignalComplete(
          add_to_cookie_copy[i],
          GoogleServiceAuthError::AuthErrorNone());
    } else {
      PerformMergeAction(add_to_cookie_copy[i]);
      if (original_gaia_accounts.end() ==
          std::find_if(
              original_gaia_accounts.begin(), original_gaia_accounts.end(),
              AccountEqualToFunc(AccountForId(add_to_cookie_copy[i])))) {
        added_to_cookie++;
      }
    }
  }

  signin_metrics::LogSigninAccountReconciliation(
      chrome_accounts_.size(), added_to_cookie, removed_from_cookie,
      !first_account_mismatch, first_execution_, number_gaia_accounts);
  first_execution_ = false;
  CalculateIfReconcileIsDone();
  if (!is_reconcile_started_)
    last_known_first_account_ = first_account;
  ScheduleStartReconcileIfChromeAccountsChanged();
}

void AccountReconcilor::AbortReconcile() {
  VLOG(1) << "AccountReconcilor::AbortReconcile: we'll try again later";
  add_to_cookie_.clear();
  CalculateIfReconcileIsDone();
}

void AccountReconcilor::CalculateIfReconcileIsDone() {
  base::TimeDelta duration = base::Time::Now() - reconcile_start_time_;
  // Record the duration if reconciliation was underway and now it is over.
  if (is_reconcile_started_ && add_to_cookie_.empty()) {
    signin_metrics::LogSigninAccountReconciliationDuration(duration,
        !error_during_last_reconcile_);
  }

  is_reconcile_started_ = !add_to_cookie_.empty();
  if (!is_reconcile_started_)
    VLOG(1) << "AccountReconcilor::CalculateIfReconcileIsDone: done";
}

void AccountReconcilor::ScheduleStartReconcileIfChromeAccountsChanged() {
  if (is_reconcile_started_)
    return;

  // Start a reconcile as the token accounts have changed.
  VLOG(1) << "AccountReconcilor::StartReconcileIfChromeAccountsChanged";
  if (chrome_accounts_changed_) {
    chrome_accounts_changed_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&AccountReconcilor::StartReconcile, base::Unretained(this)));
  }
}

void AccountReconcilor::RevokeAllSecondaryTokens() {
  for (const std::string& account : chrome_accounts_) {
    if (account != primary_account_) {
      VLOG(1) << "Revoking token for " << account;
      token_service_->RevokeCredentials(account);
    }
  }
}

// Remove the account from the list that is being merged.
bool AccountReconcilor::MarkAccountAsAddedToCookie(
    const std::string& account_id) {
  for (std::vector<std::string>::iterator i = add_to_cookie_.begin();
       i != add_to_cookie_.end();
       ++i) {
    if (account_id == *i) {
      add_to_cookie_.erase(i);
      return true;
    }
  }
  return false;
}

bool AccountReconcilor::IsTokenServiceReady() {
#if defined(OS_CHROMEOS)
  // TODO(droger): ChromeOS should use the same logic as other platforms. See
  // https://crbug.com/749535
  // On ChromeOS, there are cases where the token service is never fully
  // initialized and AreAllCredentialsLoaded() always return false.
  return token_service_->AreAllCredentialsLoaded() ||
         (token_service_->GetAccounts().size() > 0);
#else
  return token_service_->AreAllCredentialsLoaded();
#endif
}

void AccountReconcilor::OnAddAccountToCookieCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "AccountReconcilor::OnAddAccountToCookieCompleted: "
          << "Account added: " << account_id << ", "
          << "Error was " << error.ToString();
  // Always listens to GaiaCookieManagerService. Only proceed if reconciling.
  if (is_reconcile_started_ && MarkAccountAsAddedToCookie(account_id)) {
    if (error.state() != GoogleServiceAuthError::State::NONE)
      error_during_last_reconcile_ = true;
    CalculateIfReconcileIsDone();
    ScheduleStartReconcileIfChromeAccountsChanged();
  }
}

void AccountReconcilor::IncrementLockCount() {
  DCHECK_GE(account_reconcilor_lock_count_, 0);
  ++account_reconcilor_lock_count_;
  if (account_reconcilor_lock_count_ == 1)
    BlockReconcile();
}

void AccountReconcilor::DecrementLockCount() {
  DCHECK_GT(account_reconcilor_lock_count_, 0);
  --account_reconcilor_lock_count_;
  if (account_reconcilor_lock_count_ == 0)
    UnblockReconcile();
}

bool AccountReconcilor::IsReconcileBlocked() const {
  DCHECK_GE(account_reconcilor_lock_count_, 0);
  return account_reconcilor_lock_count_ > 0;
}

void AccountReconcilor::BlockReconcile() {
  DCHECK(IsReconcileBlocked());
  VLOG(1) << "AccountReconcilor::BlockReconcile.";
  if (is_reconcile_started_) {
    AbortReconcile();
    reconcile_on_unblock_ = true;
  }
  for (auto& observer : observer_list_)
    observer.OnBlockReconcile();
}

void AccountReconcilor::UnblockReconcile() {
  DCHECK(!IsReconcileBlocked());
  VLOG(1) << "AccountReconcilor::UnblockReconcile.";
  for (auto& observer : observer_list_)
    observer.OnUnblockReconcile();
  if (reconcile_on_unblock_) {
    reconcile_on_unblock_ = false;
    StartReconcile();
  }
}
