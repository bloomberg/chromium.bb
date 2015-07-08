// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_fetcher_service.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_split.h"
#include "base/trace_event/trace_event.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/refresh_token_annotation_request.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/common/signin_switches.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

const base::TimeDelta kRefreshFromTokenServiceDelay =
    base::TimeDelta::FromHours(24);

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// IsRefreshTokenDeviceIdExperimentEnabled is called from
// SendRefreshTokenAnnotationRequest only on desktop platforms.
bool IsRefreshTokenDeviceIdExperimentEnabled() {
  const std::string group_name =
      base::FieldTrialList::FindFullName("RefreshTokenDeviceId");
  return group_name == "Enabled";
}
#endif

}

// This pref used to be in the AccountTrackerService, hence its string value.
const char AccountFetcherService::kLastUpdatePref[] =
    "account_tracker_service_last_update";

class AccountFetcherService::AccountInfoFetcher :
    public OAuth2TokenService::Consumer,
    public gaia::GaiaOAuthClient::Delegate,
    public GaiaAuthConsumer {
 public:
  AccountInfoFetcher(OAuth2TokenService* token_service,
                     net::URLRequestContextGetter* request_context_getter,
                     AccountFetcherService* service,
                     const std::string& account_id);
  ~AccountInfoFetcher() override;

  const std::string& account_id() { return account_id_; }

  void Start();
  void SendSuccessIfDoneFetching();

  // OAuth2TokenService::Consumer implementation.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnGetUserInfoResponse(
      scoped_ptr<base::DictionaryValue> user_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  // Overridden from GaiaAuthConsumer:
  void OnClientLoginSuccess(const ClientLoginResult& result) override;
  void OnClientLoginFailure(const GoogleServiceAuthError& error) override;
  void OnGetUserInfoSuccess(const UserInfoMap& data) override;
  void OnGetUserInfoFailure(const GoogleServiceAuthError& error) override;

 private:
  OAuth2TokenService* token_service_;
  net::URLRequestContextGetter* request_context_getter_;
  AccountFetcherService* service_;
  const std::string account_id_;

  scoped_ptr<OAuth2TokenService::Request> login_token_request_;
  scoped_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
  scoped_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;

  scoped_ptr<base::DictionaryValue> fetched_user_info_;
  scoped_ptr<std::vector<std::string> > fetched_service_flags_;
};

AccountFetcherService::AccountInfoFetcher::AccountInfoFetcher(
    OAuth2TokenService* token_service,
    net::URLRequestContextGetter* request_context_getter,
    AccountFetcherService* service,
    const std::string& account_id)
    : OAuth2TokenService::Consumer("gaia_account_tracker"),
      token_service_(token_service),
      request_context_getter_(request_context_getter),
      service_(service),
      account_id_(account_id) {
  TRACE_EVENT_ASYNC_BEGIN1(
      "AccountFetcherService", "AccountIdFetcher", this,
      "account_id", account_id);
}

AccountFetcherService::AccountInfoFetcher::~AccountInfoFetcher() {
  TRACE_EVENT_ASYNC_END0("AccountFetcherService", "AccountIdFetcher", this);
}

void AccountFetcherService::AccountInfoFetcher::Start() {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kGoogleUserInfoEmail);
  scopes.insert(GaiaConstants::kGoogleUserInfoProfile);
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  login_token_request_ = token_service_->StartRequest(
      account_id_, scopes, this);
}

void AccountFetcherService::AccountInfoFetcher::SendSuccessIfDoneFetching() {
  if (fetched_user_info_ && fetched_service_flags_) {
    service_->OnUserInfoFetchSuccess(
        this, fetched_user_info_.get(), fetched_service_flags_.get());
  }
}

void AccountFetcherService::AccountInfoFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  TRACE_EVENT_ASYNC_STEP_PAST0(
      "AccountFetcherService", "AccountIdFetcher", this, "OnGetTokenSuccess");
  DCHECK_EQ(request, login_token_request_.get());

  gaia_oauth_client_.reset(new gaia::GaiaOAuthClient(request_context_getter_));
  const int kMaxRetries = 3;
  gaia_oauth_client_->GetUserInfo(access_token, kMaxRetries, this);

  gaia_auth_fetcher_.reset(service_->signin_client_->CreateGaiaAuthFetcher(
      this, GaiaConstants::kChromeSource, request_context_getter_));
  gaia_auth_fetcher_->StartOAuthLogin(
      access_token, GaiaConstants::kGaiaService);
}

void AccountFetcherService::AccountInfoFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  TRACE_EVENT_ASYNC_STEP_PAST1("AccountFetcherService",
                               "AccountIdFetcher",
                               this,
                               "OnGetTokenFailure",
                               "google_service_auth_error",
                               error.ToString());
  LOG(ERROR) << "OnGetTokenFailure: " << error.ToString();
  DCHECK_EQ(request, login_token_request_.get());
  service_->OnUserInfoFetchFailure(this);
}

void AccountFetcherService::AccountInfoFetcher::OnGetUserInfoResponse(
    scoped_ptr<base::DictionaryValue> user_info) {
  TRACE_EVENT_ASYNC_STEP_PAST1("AccountFetcherService",
                               "AccountIdFetcher",
                               this,
                               "OnGetUserInfoResponse",
                               "account_id",
                               account_id_);
  fetched_user_info_ = user_info.Pass();
  SendSuccessIfDoneFetching();
}

void AccountFetcherService::AccountInfoFetcher::OnClientLoginSuccess(
    const ClientLoginResult& result) {
  gaia_auth_fetcher_->StartGetUserInfo(result.lsid);
}

void AccountFetcherService::AccountInfoFetcher::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  service_->OnUserInfoFetchFailure(this);
}

void AccountFetcherService::AccountInfoFetcher::OnGetUserInfoSuccess(
    const UserInfoMap& data) {
  fetched_service_flags_.reset(new std::vector<std::string>);
  UserInfoMap::const_iterator services_iter = data.find("allServices");
  if (services_iter != data.end()) {
    base::SplitString(services_iter->second, ',', fetched_service_flags_.get());
    SendSuccessIfDoneFetching();
  } else {
    DLOG(WARNING) << "AccountInfoFetcher::OnGetUserInfoSuccess: "
                  << "GetUserInfo response didn't include allServices field.";
    service_->OnUserInfoFetchFailure(this);
  }
}

void AccountFetcherService::AccountInfoFetcher::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  service_->OnUserInfoFetchFailure(this);
}

void AccountFetcherService::AccountInfoFetcher::OnOAuthError() {
  TRACE_EVENT_ASYNC_STEP_PAST0(
      "AccountFetcherService", "AccountIdFetcher", this, "OnOAuthError");
  LOG(ERROR) << "OnOAuthError";
  service_->OnUserInfoFetchFailure(this);
}

void AccountFetcherService::AccountInfoFetcher::OnNetworkError(
    int response_code) {
  TRACE_EVENT_ASYNC_STEP_PAST1("AccountFetcherService",
                               "AccountIdFetcher",
                               this,
                               "OnNetworkError",
                               "response_code",
                               response_code);
  LOG(ERROR) << "OnNetworkError " << response_code;
  service_->OnUserInfoFetchFailure(this);
}

// AccountFetcherService implementation
AccountFetcherService::AccountFetcherService()
    : account_tracker_service_(nullptr),
      token_service_(nullptr),
      signin_client_(nullptr),
      network_fetches_enabled_(false),
      shutdown_called_(false) {}

AccountFetcherService::~AccountFetcherService() {
  DCHECK(shutdown_called_);
}

void AccountFetcherService::Initialize(
    SigninClient* signin_client,
    OAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service) {
  DCHECK(signin_client);
  DCHECK(!signin_client_);
  signin_client_ = signin_client;
  DCHECK(account_tracker_service);
  DCHECK(!account_tracker_service_);
  account_tracker_service_ = account_tracker_service;
  DCHECK(token_service);
  DCHECK(!token_service_);
  token_service_ = token_service;
  token_service_->AddObserver(this);

  last_updated_ = base::Time::FromInternalValue(
      signin_client_->GetPrefs()->GetInt64(kLastUpdatePref));

  StartFetchingInvalidAccounts();
}

void AccountFetcherService::Shutdown() {
  STLDeleteValues(&user_info_requests_);
  token_service_->RemoveObserver(this);
  shutdown_called_ = true;
}

void AccountFetcherService::EnableNetworkFetches() {
  DCHECK(CalledOnValidThread());
  DCHECK(!network_fetches_enabled_);
  network_fetches_enabled_ = true;
  // If there are accounts in |pending_user_info_fetches_|, they were deemed
  // invalid after being loaded from prefs and need to be fetched now instead of
  // waiting after the timer.
  for (std::string account_id : pending_user_info_fetches_)
    StartFetchingUserInfo(account_id);
  pending_user_info_fetches_.clear();

  // Now that network fetches are enabled, schedule the next refresh.
  ScheduleNextRefreshFromTokenService();
}

void AccountFetcherService::StartFetchingInvalidAccounts() {
  std::vector<std::string> accounts = token_service_->GetAccounts();
  for (std::vector<std::string>::const_iterator it = accounts.begin();
       it != accounts.end(); ++it) {
      RefreshAccountInfo(*it, false);
  }
}

bool AccountFetcherService::IsAllUserInfoFetched() const {
  return user_info_requests_.empty();
}

void AccountFetcherService::LoadFromTokenService() {
  std::vector<std::string> accounts = token_service_->GetAccounts();
  for (std::vector<std::string>::const_iterator it = accounts.begin();
       it != accounts.end(); ++it) {
    RefreshAccountInfo(*it, true);
  }
}

void AccountFetcherService::RefreshFromTokenService() {
  DCHECK(network_fetches_enabled_);
  LoadFromTokenService();

  last_updated_ = base::Time::Now();
  signin_client_->GetPrefs()->SetInt64(kLastUpdatePref,
                                       last_updated_.ToInternalValue());
  ScheduleNextRefreshFromTokenService();
}

void AccountFetcherService::ScheduleNextRefreshFromTokenService() {
  DCHECK(!timer_.IsRunning());
  DCHECK(network_fetches_enabled_);

  const base::TimeDelta time_since_update = base::Time::Now() - last_updated_;
  if(time_since_update > kRefreshFromTokenServiceDelay) {
    RefreshFromTokenService();
  } else {
    timer_.Start(FROM_HERE,
                 kRefreshFromTokenServiceDelay - time_since_update,
                 this,
                 &AccountFetcherService::RefreshFromTokenService);
  }
}

void AccountFetcherService::StartFetchingUserInfo(
    const std::string& account_id) {
  DCHECK(CalledOnValidThread());
  if (!network_fetches_enabled_) {
    pending_user_info_fetches_.push_back(account_id);
    return;
  }

  if (!ContainsKey(user_info_requests_, account_id)) {
    DVLOG(1) << "StartFetching " << account_id;
    AccountInfoFetcher* fetcher =
        new AccountInfoFetcher(token_service_,
                               signin_client_->GetURLRequestContext(),
                               this,
                               account_id);
    user_info_requests_[account_id] = fetcher;
    fetcher->Start();
  }
}

void AccountFetcherService::RefreshAccountInfo(const std::string& account_id,
                                               bool force_remote_fetch) {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 AccountFetcherService::OnRefreshTokenAvailable"));

  TRACE_EVENT1("AccountFetcherService",
               "AccountFetcherService::RefreshAccountInfo",
               "account_id",
               account_id);
  DVLOG(1) << "AVAILABLE " << account_id;

  account_tracker_service_->StartTrackingAccount(account_id);
  const AccountTrackerService::AccountInfo& info =
      account_tracker_service_->GetAccountInfo(account_id);

  // Don't bother fetching for supervised users since this causes the token
  // service to raise spurious auth errors.
  // TODO(treib): this string is also used in supervised_user_constants.cc.
  // Should put in a common place.
  if (account_id == "managed_user@localhost")
    return;

  // Only fetch the user info if it's currently invalid or if
  // |force_remote_fetch| is true (meaning the service is due for a timed
  // update).
#if defined(OS_ANDROID)
  // TODO(mlerman): Change this condition back to info.IsValid() and ensure the
  // Fetch doesn't occur until after ProfileImpl::OnPrefsLoaded().
  if (force_remote_fetch || info.gaia.empty())
#else
  if (force_remote_fetch || !info.IsValid())
#endif
    StartFetchingUserInfo(account_id);

  SendRefreshTokenAnnotationRequest(account_id);
}

void AccountFetcherService::DeleteFetcher(AccountInfoFetcher* fetcher) {
  DVLOG(1) << "DeleteFetcher " << fetcher->account_id();
  const std::string& account_id = fetcher->account_id();
  DCHECK(ContainsKey(user_info_requests_, account_id));
  DCHECK_EQ(fetcher, user_info_requests_[account_id]);
  user_info_requests_.erase(account_id);
  delete fetcher;
}

void AccountFetcherService::SendRefreshTokenAnnotationRequest(
    const std::string& account_id) {
// We only need to send RefreshTokenAnnotationRequest from desktop platforms.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  if (IsRefreshTokenDeviceIdExperimentEnabled() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableRefreshTokenAnnotationRequest)) {
    scoped_ptr<RefreshTokenAnnotationRequest> request =
        RefreshTokenAnnotationRequest::SendIfNeeded(
            signin_client_->GetPrefs(), token_service_, signin_client_,
            signin_client_->GetURLRequestContext(), account_id,
            base::Bind(
                &AccountFetcherService::RefreshTokenAnnotationRequestDone,
                base::Unretained(this), account_id));
    // If request was sent AccountFetcherService needs to own request till it
    // finishes.
    if (request)
      refresh_token_annotation_requests_.set(account_id, request.Pass());
  }
#endif
}

void AccountFetcherService::RefreshTokenAnnotationRequestDone(
    const std::string& account_id) {
  refresh_token_annotation_requests_.erase(account_id);
}

void AccountFetcherService::OnUserInfoFetchSuccess(
    AccountInfoFetcher* fetcher,
    const base::DictionaryValue* user_info,
    const std::vector<std::string>* service_flags) {
  const std::string& account_id = fetcher->account_id();

  account_tracker_service_->SetAccountStateFromUserInfo(
      account_id, user_info, service_flags);
  DeleteFetcher(fetcher);
}

void AccountFetcherService::OnUserInfoFetchFailure(
    AccountInfoFetcher* fetcher) {
  LOG(WARNING) << "Failed to get UserInfo for " << fetcher->account_id();
  account_tracker_service_->NotifyAccountUpdateFailed(fetcher->account_id());
  DeleteFetcher(fetcher);
}

void AccountFetcherService::OnRefreshTokenAvailable(
    const std::string& account_id) {
  // The SigninClient needs a "final init" in order to perform some actions
  // (such as fetching the signin token "handle" in order to look for password
  // changes) once everything is initialized and the refresh token is present.
  signin_client_->DoFinalInit();

  RefreshAccountInfo(account_id, false);
}

void AccountFetcherService::OnRefreshTokenRevoked(
    const std::string& account_id) {
  TRACE_EVENT1("AccountFetcherService",
               "AccountFetcherService::OnRefreshTokenRevoked",
               "account_id",
               account_id);

  DVLOG(1) << "REVOKED " << account_id;

  if (ContainsKey(user_info_requests_, account_id))
    DeleteFetcher(user_info_requests_[account_id]);

  account_tracker_service_->StopTrackingAccount(account_id);
}

void AccountFetcherService::OnRefreshTokensLoaded() {
  StartFetchingInvalidAccounts();
}
