// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_reconcilor.h"

#include <algorithm>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"


namespace {

class EmailEqualToFunc : public std::equal_to<std::pair<std::string, bool> > {
 public:
  bool operator()(const std::pair<std::string, bool>& p1,
                  const std::pair<std::string, bool>& p2) const;
};

bool EmailEqualToFunc::operator()(
    const std::pair<std::string, bool>& p1,
    const std::pair<std::string, bool>& p2) const {
  return p1.second == p2.second && gaia::AreEmailsSame(p1.first, p2.first);
}

class AreEmailsSameFunc : public std::equal_to<std::string> {
 public:
  bool operator()(const std::string& p1,
                  const std::string& p2) const;
};

bool AreEmailsSameFunc::operator()(
    const std::string& p1,
    const std::string& p2) const {
  return gaia::AreEmailsSame(p1, p2);
}

}  // namespace


AccountReconcilor::AccountReconcilor(ProfileOAuth2TokenService* token_service,
                                     SigninManagerBase* signin_manager,
                                     SigninClient* client)
    : token_service_(token_service),
      signin_manager_(signin_manager),
      client_(client),
      merge_session_helper_(token_service_,
                            GaiaConstants::kReconcilorSource,
                            client->GetURLRequestContext(),
                            NULL),
      registered_with_token_service_(false),
      registered_with_merge_session_helper_(false),
      registered_with_content_settings_(false),
      is_reconcile_started_(false),
      first_execution_(true),
      are_gaia_accounts_set_(false),
      chrome_accounts_changed_(false) {
  VLOG(1) << "AccountReconcilor::AccountReconcilor";
}

AccountReconcilor::~AccountReconcilor() {
  VLOG(1) << "AccountReconcilor::~AccountReconcilor";
  // Make sure shutdown was called first.
  DCHECK(!registered_with_token_service_);
  DCHECK(!registered_with_merge_session_helper_);
}

void AccountReconcilor::Initialize(bool start_reconcile_if_tokens_available) {
  VLOG(1) << "AccountReconcilor::Initialize";
  RegisterWithSigninManager();

  // If this user is not signed in, the reconcilor should do nothing but
  // wait for signin.
  if (IsProfileConnected()) {
    RegisterWithMergeSessionHelper();
    RegisterForCookieChanges();
    RegisterWithContentSettings();
    RegisterWithTokenService();

    // Start a reconcile if the tokens are already loaded.
    if (start_reconcile_if_tokens_available &&
        token_service_->GetAccounts().size() > 0) {
      StartReconcile();
    }
  }
}

void AccountReconcilor::Shutdown() {
  VLOG(1) << "AccountReconcilor::Shutdown";
  merge_session_helper_.CancelAll();
  gaia_fetcher_.reset();
  get_gaia_accounts_callbacks_.clear();
  UnregisterWithMergeSessionHelper();
  UnregisterWithSigninManager();
  UnregisterWithTokenService();
  UnregisterForCookieChanges();
  UnregisterWithContentSettings();
}

void AccountReconcilor::AddMergeSessionObserver(
    MergeSessionHelper::Observer* observer) {
  merge_session_helper_.AddObserver(observer);
}

void AccountReconcilor::RemoveMergeSessionObserver(
    MergeSessionHelper::Observer* observer) {
  merge_session_helper_.RemoveObserver(observer);
}

void AccountReconcilor::RegisterForCookieChanges() {
  // First clear any existing registration to avoid DCHECKs that can otherwise
  // go off in some embedders on reauth (e.g., ChromeSigninClient).
  UnregisterForCookieChanges();
  cookie_changed_subscription_ = client_->AddCookieChangedCallback(
      GaiaUrls::GetInstance()->gaia_url(),
      "LSID",
      base::Bind(&AccountReconcilor::OnCookieChanged, base::Unretained(this)));
}

void AccountReconcilor::UnregisterForCookieChanges() {
  cookie_changed_subscription_.reset();
}

void AccountReconcilor::RegisterWithSigninManager() {
  signin_manager_->AddObserver(this);
}

void AccountReconcilor::UnregisterWithSigninManager() {
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

void AccountReconcilor::RegisterWithMergeSessionHelper() {
  VLOG(1) << "AccountReconcilor::RegisterWithMergeSessionHelper";
  // During re-auth, the reconcilor will get a callback about successful signin
  // even when the profile is already connected.  Avoid re-registering
  // with the helper since this will DCHECK.
  if (registered_with_merge_session_helper_)
    return;

  merge_session_helper_.AddObserver(this);
  registered_with_merge_session_helper_ = true;
}
void AccountReconcilor::UnregisterWithMergeSessionHelper() {
  VLOG(1) << "AccountReconcilor::UnregisterWithMergeSessionHelper";
  if (!registered_with_merge_session_helper_)
    return;

  merge_session_helper_.RemoveObserver(this);
  registered_with_merge_session_helper_ = false;
}

bool AccountReconcilor::IsProfileConnected() {
  return signin_manager_->IsAuthenticated();
}

void AccountReconcilor::OnCookieChanged(const net::CanonicalCookie& cookie,
                                        bool removed) {
  DCHECK_EQ("LSID", cookie.Name());
  DCHECK_EQ(GaiaUrls::GetInstance()->gaia_url().host(), cookie.Domain());
  if (cookie.IsSecure() && cookie.IsHttpOnly()) {
    VLOG(1) << "AccountReconcilor::OnCookieChanged: LSID changed";

    // It is possible that O2RT is not available at this moment.
    if (!token_service_->GetAccounts().size()) {
      VLOG(1) << "AccountReconcilor::OnCookieChanged: cookie change is ingored"
                 "because O2RT is not available yet.";
      return;
    }

    StartReconcile();
  }
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
  VLOG(1) << "AccountReconcilor::OnEndBatchChanges";
  // Remember that accounts have changed if a reconcile is already started.
  chrome_accounts_changed_ = is_reconcile_started_;
  StartReconcile();
}

void AccountReconcilor::GoogleSigninSucceeded(const std::string& account_id,
                                              const std::string& username,
                                              const std::string& password) {
  VLOG(1) << "AccountReconcilor::GoogleSigninSucceeded: signed in";
  RegisterWithMergeSessionHelper();
  RegisterForCookieChanges();
  RegisterWithContentSettings();
  RegisterWithTokenService();
}

void AccountReconcilor::GoogleSignedOut(const std::string& account_id,
                                        const std::string& username) {
  VLOG(1) << "AccountReconcilor::GoogleSignedOut: signed out";
  gaia_fetcher_.reset();
  get_gaia_accounts_callbacks_.clear();
  AbortReconcile();
  UnregisterWithMergeSessionHelper();
  UnregisterWithTokenService();
  UnregisterForCookieChanges();
  UnregisterWithContentSettings();
  PerformLogoutAllAccountsAction();
}

void AccountReconcilor::PerformMergeAction(const std::string& account_id) {
  if (!switches::IsEnableAccountConsistency()) {
    MarkAccountAsAddedToCookie(account_id);
    return;
  }
  VLOG(1) << "AccountReconcilor::PerformMergeAction: " << account_id;
  merge_session_helper_.LogIn(account_id);
}

void AccountReconcilor::PerformLogoutAllAccountsAction() {
  if (!switches::IsEnableAccountConsistency())
    return;
  VLOG(1) << "AccountReconcilor::PerformLogoutAllAccountsAction";
  merge_session_helper_.LogOutAllAccounts();
}

void AccountReconcilor::StartReconcile() {
  if (!IsProfileConnected() || !client_->AreSigninCookiesAllowed()) {
    VLOG(1) << "AccountReconcilor::StartReconcile: !connected or no cookies";
    return;
  }

  if (is_reconcile_started_ || get_gaia_accounts_callbacks_.size() > 0)
    return;

  is_reconcile_started_ = true;
  m_reconcile_start_time_ = base::Time::Now();

  // Reset state for validating gaia cookie.
  are_gaia_accounts_set_ = false;
  gaia_accounts_.clear();

  // Reset state for validating oauth2 tokens.
  primary_account_.clear();
  chrome_accounts_.clear();
  add_to_cookie_.clear();
  ValidateAccountsFromTokenService();

  // Start process by checking connections to external sites.
  merge_session_helper_.StartFetchingExternalCcResult();
}

void AccountReconcilor::GetAccountsFromCookie(
    GetAccountsFromCookieCallback callback) {
  get_gaia_accounts_callbacks_.push_back(callback);
  if (!gaia_fetcher_)
    MayBeDoNextListAccounts();
}

void AccountReconcilor::OnListAccountsSuccess(const std::string& data) {
  gaia_fetcher_.reset();

  // Get account information from response data.
  std::vector<std::pair<std::string, bool> > gaia_accounts;
  bool valid_json = gaia::ParseListAccountsData(data, &gaia_accounts);
  if (!valid_json) {
    VLOG(1) << "AccountReconcilor::OnListAccountsSuccess: parsing error";
  } else if (gaia_accounts.size() > 0) {
    VLOG(1) << "AccountReconcilor::OnListAccountsSuccess: "
            << "Gaia " << gaia_accounts.size() << " accounts, "
            << "Primary is '" << gaia_accounts[0].first << "'";
  } else {
    VLOG(1) << "AccountReconcilor::OnListAccountsSuccess: No accounts";
  }

  // There must be at least one callback waiting for result.
  DCHECK(!get_gaia_accounts_callbacks_.empty());

  GoogleServiceAuthError error =
      !valid_json ? GoogleServiceAuthError(
                        GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE)
                  : GoogleServiceAuthError::AuthErrorNone();
  get_gaia_accounts_callbacks_.front().Run(error, gaia_accounts);
  get_gaia_accounts_callbacks_.pop_front();

  MayBeDoNextListAccounts();
}

void AccountReconcilor::OnListAccountsFailure(
    const GoogleServiceAuthError& error) {
  gaia_fetcher_.reset();
  VLOG(1) << "AccountReconcilor::OnListAccountsFailure: " << error.ToString();
  std::vector<std::pair<std::string, bool> > empty_accounts;

  // There must be at least one callback waiting for result.
  DCHECK(!get_gaia_accounts_callbacks_.empty());

  get_gaia_accounts_callbacks_.front().Run(error, empty_accounts);
  get_gaia_accounts_callbacks_.pop_front();

  MayBeDoNextListAccounts();
}

void AccountReconcilor::MayBeDoNextListAccounts() {
  if (!get_gaia_accounts_callbacks_.empty()) {
    gaia_fetcher_.reset(new GaiaAuthFetcher(
        this, GaiaConstants::kReconcilorSource,
        client_->GetURLRequestContext()));
    gaia_fetcher_->StartListAccounts();
  }
}

void AccountReconcilor::ContinueReconcileActionAfterGetGaiaAccounts(
    const GoogleServiceAuthError& error,
    const std::vector<std::pair<std::string, bool> >& accounts) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    gaia_accounts_ = accounts;
    are_gaia_accounts_set_ = true;
    FinishReconcile();
  } else {
    AbortReconcile();
  }
}

void AccountReconcilor::ValidateAccountsFromTokenService() {
  primary_account_ = signin_manager_->GetAuthenticatedAccountId();
  DCHECK(!primary_account_.empty());

  chrome_accounts_ = token_service_->GetAccounts();

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

void AccountReconcilor::FinishReconcile() {
  VLOG(1) << "AccountReconcilor::FinishReconcile";
  DCHECK(are_gaia_accounts_set_);
  DCHECK(add_to_cookie_.empty());
  int number_gaia_accounts = gaia_accounts_.size();
  bool are_primaries_equal = number_gaia_accounts > 0 &&
      gaia::AreEmailsSame(primary_account_, gaia_accounts_[0].first);

  // If there are any accounts in the gaia cookie but not in chrome, then
  // those accounts need to be removed from the cookie.  This means we need
  // to blow the cookie away.
  int removed_from_cookie = 0;
  for (size_t i = 0; i < gaia_accounts_.size(); ++i) {
    const std::string& gaia_account = gaia_accounts_[i].first;
    if (gaia_accounts_[i].second &&
        chrome_accounts_.end() ==
            std::find_if(chrome_accounts_.begin(),
                         chrome_accounts_.end(),
                         std::bind1st(AreEmailsSameFunc(), gaia_account))) {
      ++removed_from_cookie;
    }
  }

  bool rebuild_cookie = !are_primaries_equal || removed_from_cookie > 0;
  std::vector<std::pair<std::string, bool> > original_gaia_accounts =
      gaia_accounts_;
  if (rebuild_cookie) {
    VLOG(1) << "AccountReconcilor::FinishReconcile: rebuild cookie";
    // Really messed up state.  Blow away the gaia cookie completely and
    // rebuild it, making sure the primary account as specified by the
    // SigninManager is the first session in the gaia cookie.
    PerformLogoutAllAccountsAction();
    gaia_accounts_.clear();
  }

  // Create a list of accounts that need to be added to the gaia cookie.
  // The primary account must be first to make sure it becomes the default
  // account in the case where chrome is completely rebuilding the cookie.
  add_to_cookie_.push_back(primary_account_);
  for (size_t i = 0; i < chrome_accounts_.size(); ++i) {
    if (chrome_accounts_[i] != primary_account_)
      add_to_cookie_.push_back(chrome_accounts_[i]);
  }

  // For each account known to chrome, PerformMergeAction() if the account is
  // not already in the cookie jar or its state is invalid, or signal merge
  // completed otherwise.  Make a copy of |add_to_cookie_| since calls to
  // SignalComplete() will change the array.
  std::vector<std::string> add_to_cookie_copy = add_to_cookie_;
  int added_to_cookie = 0;
  for (size_t i = 0; i < add_to_cookie_copy.size(); ++i) {
    if (gaia_accounts_.end() !=
            std::find_if(gaia_accounts_.begin(),
                         gaia_accounts_.end(),
                         std::bind1st(EmailEqualToFunc(),
                                      std::make_pair(add_to_cookie_copy[i],
                                                     true)))) {
      merge_session_helper_.SignalComplete(
          add_to_cookie_copy[i],
          GoogleServiceAuthError::AuthErrorNone());
    } else {
      PerformMergeAction(add_to_cookie_copy[i]);
      if (original_gaia_accounts.end() ==
              std::find_if(original_gaia_accounts.begin(),
                           original_gaia_accounts.end(),
                           std::bind1st(EmailEqualToFunc(),
                                        std::make_pair(add_to_cookie_copy[i],
                                                       true)))) {
        added_to_cookie++;
      }
    }
  }

  signin_metrics::LogSigninAccountReconciliation(chrome_accounts_.size(),
                                                 added_to_cookie,
                                                 removed_from_cookie,
                                                 are_primaries_equal,
                                                 first_execution_,
                                                 number_gaia_accounts);
  first_execution_ = false;
  CalculateIfReconcileIsDone();
  ScheduleStartReconcileIfChromeAccountsChanged();
}

void AccountReconcilor::AbortReconcile() {
  VLOG(1) << "AccountReconcilor::AbortReconcile: we'll try again later";
  add_to_cookie_.clear();
  CalculateIfReconcileIsDone();
}

void AccountReconcilor::CalculateIfReconcileIsDone() {
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
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AccountReconcilor::StartReconcile, base::Unretained(this)));
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

void AccountReconcilor::MergeSessionCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "AccountReconcilor::MergeSessionCompleted: account_id="
          << account_id << " error=" << error.ToString();
  DCHECK(is_reconcile_started_);

  if (MarkAccountAsAddedToCookie(account_id)) {
    CalculateIfReconcileIsDone();
    ScheduleStartReconcileIfChromeAccountsChanged();
  }
}

void AccountReconcilor::GetCheckConnectionInfoCompleted(bool succeeded) {
  base::TimeDelta time_to_check_connections =
      base::Time::Now() - m_reconcile_start_time_;
  signin_metrics::LogExternalCcResultFetches(succeeded,
                                             time_to_check_connections);
  GetAccountsFromCookie(base::Bind(
      &AccountReconcilor::ContinueReconcileActionAfterGetGaiaAccounts,
      base::Unretained(this)));
}
