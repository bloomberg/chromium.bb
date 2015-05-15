// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_tracker_service.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/signin/core/browser/refresh_token_annotation_request.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/signin/core/common/signin_switches.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

const char kAccountKeyPath[] = "account_id";
const char kAccountEmailPath[] = "email";
const char kAccountGaiaPath[] = "gaia";
const char kAccountHostedDomainPath[] = "hd";
const char kAccountFullNamePath[] = "full_name";
const char kAccountGivenNamePath[] = "given_name";
const char kAccountLocalePath[] = "locale";
const char kAccountServiceFlagsPath[] = "service_flags";

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

// This must be a string which can never be a valid domain.
const char AccountTrackerService::kNoHostedDomainFound[] = "NO_HOSTED_DOMAIN";

class AccountInfoFetcher : public OAuth2TokenService::Consumer,
                           public gaia::GaiaOAuthClient::Delegate,
                           public GaiaAuthConsumer {
 public:
  AccountInfoFetcher(OAuth2TokenService* token_service,
                     net::URLRequestContextGetter* request_context_getter,
                     AccountTrackerService* service,
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
  AccountTrackerService* service_;
  const std::string account_id_;

  scoped_ptr<OAuth2TokenService::Request> login_token_request_;
  scoped_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
  scoped_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;

  scoped_ptr<base::DictionaryValue> fetched_user_info_;
  scoped_ptr<std::vector<std::string> > fetched_service_flags_;
};

AccountInfoFetcher::AccountInfoFetcher(
    OAuth2TokenService* token_service,
    net::URLRequestContextGetter* request_context_getter,
    AccountTrackerService* service,
    const std::string& account_id)
    : OAuth2TokenService::Consumer("gaia_account_tracker"),
      token_service_(token_service),
      request_context_getter_(request_context_getter),
      service_(service),
      account_id_(account_id) {
  TRACE_EVENT_ASYNC_BEGIN1(
      "AccountTrackerService", "AccountIdFetcher", this,
      "account_id", account_id);
}

AccountInfoFetcher::~AccountInfoFetcher() {
  TRACE_EVENT_ASYNC_END0("AccountTrackerService", "AccountIdFetcher", this);
}

void AccountInfoFetcher::Start() {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kGoogleUserInfoEmail);
  scopes.insert(GaiaConstants::kGoogleUserInfoProfile);
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  login_token_request_ = token_service_->StartRequest(
      account_id_, scopes, this);
}

void AccountInfoFetcher::SendSuccessIfDoneFetching() {
  if (fetched_user_info_ && fetched_service_flags_) {
    service_->OnUserInfoFetchSuccess(
        this, fetched_user_info_.get(), fetched_service_flags_.get());
  }
}

void AccountInfoFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  TRACE_EVENT_ASYNC_STEP_PAST0(
      "AccountTrackerService", "AccountIdFetcher", this, "OnGetTokenSuccess");
  DCHECK_EQ(request, login_token_request_.get());

  gaia_oauth_client_.reset(new gaia::GaiaOAuthClient(request_context_getter_));
  const int kMaxRetries = 3;
  gaia_oauth_client_->GetUserInfo(access_token, kMaxRetries, this);

  gaia_auth_fetcher_.reset(
      new GaiaAuthFetcher(
          this, GaiaConstants::kChromeSource, request_context_getter_));
  gaia_auth_fetcher_->StartOAuthLogin(
      access_token, GaiaConstants::kGaiaService);
}

void AccountInfoFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  TRACE_EVENT_ASYNC_STEP_PAST1("AccountTrackerService",
                               "AccountIdFetcher",
                               this,
                               "OnGetTokenFailure",
                               "google_service_auth_error",
                               error.ToString());
  LOG(ERROR) << "OnGetTokenFailure: " << error.ToString();
  DCHECK_EQ(request, login_token_request_.get());
  service_->OnUserInfoFetchFailure(this);
}

void AccountInfoFetcher::OnGetUserInfoResponse(
    scoped_ptr<base::DictionaryValue> user_info) {
  TRACE_EVENT_ASYNC_STEP_PAST1("AccountTrackerService",
                               "AccountIdFetcher",
                               this,
                               "OnGetUserInfoResponse",
                               "account_id",
                               account_id_);
  fetched_user_info_ = user_info.Pass();
  SendSuccessIfDoneFetching();
}

void AccountInfoFetcher::OnClientLoginSuccess(
    const ClientLoginResult& result) {
  gaia_auth_fetcher_->StartGetUserInfo(result.lsid);
}

void AccountInfoFetcher::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  service_->OnUserInfoFetchFailure(this);
}

void AccountInfoFetcher::OnGetUserInfoSuccess(const UserInfoMap& data) {
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

void AccountInfoFetcher::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  service_->OnUserInfoFetchFailure(this);
}

void AccountInfoFetcher::OnOAuthError() {
  TRACE_EVENT_ASYNC_STEP_PAST0(
      "AccountTrackerService", "AccountIdFetcher", this, "OnOAuthError");
  LOG(ERROR) << "OnOAuthError";
  service_->OnUserInfoFetchFailure(this);
}

void AccountInfoFetcher::OnNetworkError(int response_code) {
  TRACE_EVENT_ASYNC_STEP_PAST1("AccountTrackerService",
                               "AccountIdFetcher",
                               this,
                               "OnNetworkError",
                               "response_code",
                               response_code);
  LOG(ERROR) << "OnNetworkError " << response_code;
  service_->OnUserInfoFetchFailure(this);
}

AccountTrackerService::AccountInfo::AccountInfo() {}
AccountTrackerService::AccountInfo::~AccountInfo() {}

bool AccountTrackerService::AccountInfo::IsValid() {
  return !account_id.empty() && !email.empty() && !gaia.empty() &&
         !hosted_domain.empty() && !full_name.empty() && !given_name.empty() &&
         !locale.empty();
}


const char AccountTrackerService::kAccountInfoPref[] = "account_info";
const char AccountTrackerService::kAccountTrackerServiceLastUpdate[] =
    "account_tracker_service_last_update";

AccountTrackerService::AccountTrackerService()
    : token_service_(NULL),
      signin_client_(NULL),
      shutdown_called_(false),
      network_fetches_enabled_(false) {
}

AccountTrackerService::~AccountTrackerService() {
  DCHECK(shutdown_called_);
}

void AccountTrackerService::Initialize(
    OAuth2TokenService* token_service,
    SigninClient* signin_client) {
  DCHECK(token_service);
  DCHECK(!token_service_);
  DCHECK(signin_client);
  DCHECK(!signin_client_);
  token_service_ = token_service;
  signin_client_ = signin_client;
  token_service_->AddObserver(this);
  LoadFromPrefs();
  StartFetchingInvalidAccounts();
}

void AccountTrackerService::EnableNetworkFetches() {
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

void AccountTrackerService::Shutdown() {
  shutdown_called_ = true;
  STLDeleteValues(&user_info_requests_);
  token_service_->RemoveObserver(this);
}

void AccountTrackerService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AccountTrackerService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool AccountTrackerService::IsAllUserInfoFetched() const {
  return user_info_requests_.empty();
}

std::vector<AccountTrackerService::AccountInfo>
AccountTrackerService::GetAccounts() const {
  std::vector<AccountInfo> accounts;

  for (std::map<std::string, AccountState>::const_iterator it =
           accounts_.begin();
       it != accounts_.end();
       ++it) {
    const AccountState& state = it->second;
    accounts.push_back(state.info);
  }
  return accounts;
}

AccountTrackerService::AccountInfo AccountTrackerService::GetAccountInfo(
    const std::string& account_id) {
  if (ContainsKey(accounts_, account_id))
    return accounts_[account_id].info;

  return AccountInfo();
}

AccountTrackerService::AccountInfo
AccountTrackerService::FindAccountInfoByGaiaId(
    const std::string& gaia_id) {
  if (!gaia_id.empty()) {
    for (std::map<std::string, AccountState>::const_iterator it =
             accounts_.begin();
         it != accounts_.end();
         ++it) {
      const AccountState& state = it->second;
      if (state.info.gaia == gaia_id)
        return state.info;
    }
  }

  return AccountInfo();
}

AccountTrackerService::AccountInfo
AccountTrackerService::FindAccountInfoByEmail(
    const std::string& email) {
  if (!email.empty()) {
    for (std::map<std::string, AccountState>::const_iterator it =
             accounts_.begin();
         it != accounts_.end();
         ++it) {
      const AccountState& state = it->second;
      if (gaia::AreEmailsSame(state.info.email, email))
        return state.info;
    }
  }

  return AccountInfo();
}

AccountTrackerService::AccountIdMigrationState
AccountTrackerService::GetMigrationState() {
  return GetMigrationState(signin_client_->GetPrefs());
}

// static
AccountTrackerService::AccountIdMigrationState
AccountTrackerService::GetMigrationState(PrefService* pref_service) {
  return static_cast<AccountTrackerService::AccountIdMigrationState>(
      pref_service->GetInteger(prefs::kAccountIdMigrationState));
}

void AccountTrackerService::RefreshAccountInfo(const std::string& account_id,
                                               bool force_remote_fetch) {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 AccountTrackerService::OnRefreshTokenAvailable"));

  TRACE_EVENT1("AccountTrackerService",
               "AccountTracker::OnRefreshTokenAvailable",
               "account_id",
               account_id);
  DVLOG(1) << "AVAILABLE " << account_id;

  StartTrackingAccount(account_id);
  AccountState& state = accounts_[account_id];

  // Don't bother fetching for supervised users since this causes the token
  // service to raise spurious auth errors.
  // TODO(treib): this string is also used in supervised_user_constants.cc.
  // Should put in a common place.
  if (account_id == "managed_user@localhost")
    return;

  if (signin_client_->UpdateAccountInfo(&state.info))
    SaveToPrefs(state);

  // Only fetch the user info if it's currently invalid or if
  // |force_remote_fetch| is true (meaning the service is due for a timed
  // update).
#if defined(OS_ANDROID)
  // TODO(mlerman): Change this condition back to state.info.IsValid() and
  // ensure the Fetch doesn't occur until after ProfileImpl::OnPrefsLoaded().
  if (force_remote_fetch || state.info.gaia.empty())
#else
  if (force_remote_fetch || !state.info.IsValid())
#endif
    StartFetchingUserInfo(account_id);

  SendRefreshTokenAnnotationRequest(account_id);
}

void AccountTrackerService::OnRefreshTokenAvailable(
    const std::string& account_id) {
  RefreshAccountInfo(account_id, false);
}

void AccountTrackerService::OnRefreshTokenRevoked(
    const std::string& account_id) {
  TRACE_EVENT1("AccountTrackerService",
               "AccountTracker::OnRefreshTokenRevoked",
               "account_id",
               account_id);

  DVLOG(1) << "REVOKED " << account_id;
  StopTrackingAccount(account_id);
}

void AccountTrackerService::NotifyAccountUpdated(const AccountState& state) {
  DCHECK(!state.info.gaia.empty());
  FOR_EACH_OBSERVER(
      Observer, observer_list_, OnAccountUpdated(state.info));
}

void AccountTrackerService::NotifyAccountUpdateFailed(
    const std::string& account_id) {
  FOR_EACH_OBSERVER(
      Observer, observer_list_, OnAccountUpdateFailed(account_id));
}

void AccountTrackerService::NotifyAccountRemoved(const AccountState& state) {
  DCHECK(!state.info.gaia.empty());
  FOR_EACH_OBSERVER(
      Observer, observer_list_, OnAccountRemoved(state.info));
}

void AccountTrackerService::StartFetchingInvalidAccounts() {
  std::vector<std::string> accounts = token_service_->GetAccounts();
  for (std::vector<std::string>::const_iterator it = accounts.begin();
       it != accounts.end(); ++it) {
      RefreshAccountInfo(*it, false);
  }
}

void AccountTrackerService::StartTrackingAccount(
    const std::string& account_id) {
  if (!ContainsKey(accounts_, account_id)) {
    DVLOG(1) << "StartTracking " << account_id;
    AccountState state;
    state.info.account_id = account_id;
    accounts_.insert(make_pair(account_id, state));
  }
}

void AccountTrackerService::StopTrackingAccount(const std::string& account_id) {
  DVLOG(1) << "StopTracking " << account_id;
  if (ContainsKey(accounts_, account_id)) {
    AccountState& state = accounts_[account_id];
    RemoveFromPrefs(state);
    if (!state.info.gaia.empty())
      NotifyAccountRemoved(state);

    accounts_.erase(account_id);
  }

  if (ContainsKey(user_info_requests_, account_id))
    DeleteFetcher(user_info_requests_[account_id]);
}

void AccountTrackerService::StartFetchingUserInfo(
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

void AccountTrackerService::SetAccountStateFromUserInfo(
    const std::string& account_id,
    const base::DictionaryValue* user_info,
    const std::vector<std::string>* service_flags) {
  AccountState& state = accounts_[account_id];

  std::string gaia_id;
  std::string email;
  if (user_info->GetString("id", &gaia_id) &&
      user_info->GetString("email", &email)) {
    state.info.gaia = gaia_id;
    state.info.email = email;

    std::string hosted_domain;
    if (user_info->GetString("hd", &hosted_domain) && !hosted_domain.empty()) {
      state.info.hosted_domain = hosted_domain;
    } else {
      state.info.hosted_domain = kNoHostedDomainFound;
    }

    user_info->GetString("name", &state.info.full_name);
    user_info->GetString("given_name", &state.info.given_name);
    user_info->GetString("locale", &state.info.locale);

    state.info.service_flags = *service_flags;

    NotifyAccountUpdated(state);
    SaveToPrefs(state);
  }
}

void AccountTrackerService::OnUserInfoFetchSuccess(
    AccountInfoFetcher* fetcher,
    const base::DictionaryValue* user_info,
    const std::vector<std::string>* service_flags) {
  const std::string& account_id = fetcher->account_id();
  DCHECK(ContainsKey(accounts_, account_id));

  SetAccountStateFromUserInfo(account_id, user_info, service_flags);
  DeleteFetcher(fetcher);
}

void AccountTrackerService::OnUserInfoFetchFailure(
    AccountInfoFetcher* fetcher) {
  LOG(WARNING) << "Failed to get UserInfo for " << fetcher->account_id();
  NotifyAccountUpdateFailed(fetcher->account_id());
  DeleteFetcher(fetcher);
}

void AccountTrackerService::DeleteFetcher(AccountInfoFetcher* fetcher) {
  DVLOG(1) << "DeleteFetcher " << fetcher->account_id();
  const std::string& account_id = fetcher->account_id();
  DCHECK(ContainsKey(user_info_requests_, account_id));
  DCHECK_EQ(fetcher, user_info_requests_[account_id]);
  user_info_requests_.erase(account_id);
  delete fetcher;
}

void AccountTrackerService::LoadFromPrefs() {
  const base::ListValue* list =
      signin_client_->GetPrefs()->GetList(kAccountInfoPref);
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* dict;
    if (list->GetDictionary(i, &dict)) {
      base::string16 value;
      if (dict->GetString(kAccountKeyPath, &value)) {
        std::string account_id = base::UTF16ToUTF8(value);
        StartTrackingAccount(account_id);
        AccountState& state = accounts_[account_id];

        if (dict->GetString(kAccountGaiaPath, &value))
          state.info.gaia = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountEmailPath, &value))
          state.info.email = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountHostedDomainPath, &value))
          state.info.hosted_domain = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountFullNamePath, &value))
          state.info.full_name = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountGivenNamePath, &value))
          state.info.given_name = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountLocalePath, &value))
          state.info.locale = base::UTF16ToUTF8(value);

        const base::ListValue* service_flags_list;
        if (dict->GetList(kAccountServiceFlagsPath, &service_flags_list)) {
          std::string flag;
          for(base::Value* flag: *service_flags_list) {
            std::string flag_string;
            if(flag->GetAsString(&flag_string)) {
              state.info.service_flags.push_back(flag_string);
            }
          }
        }

        if (state.info.IsValid())
          NotifyAccountUpdated(state);
      }
    }
  }
  last_updated_ = base::Time::FromInternalValue(
      signin_client_->GetPrefs()->GetInt64(kAccountTrackerServiceLastUpdate));
}

void AccountTrackerService::SaveToPrefs(const AccountState& state) {
  if (!signin_client_->GetPrefs())
    return;

  base::DictionaryValue* dict = NULL;
  base::string16 account_id_16 = base::UTF8ToUTF16(state.info.account_id);
  ListPrefUpdate update(signin_client_->GetPrefs(), kAccountInfoPref);
  for(size_t i = 0; i < update->GetSize(); ++i, dict = NULL) {
    if (update->GetDictionary(i, &dict)) {
      base::string16 value;
      if (dict->GetString(kAccountKeyPath, &value) && value == account_id_16)
        break;
    }
  }

  if (!dict) {
    dict = new base::DictionaryValue();
    update->Append(dict);  // |update| takes ownership.
    dict->SetString(kAccountKeyPath, account_id_16);
  }

  dict->SetString(kAccountEmailPath, state.info.email);
  dict->SetString(kAccountGaiaPath, state.info.gaia);
  dict->SetString(kAccountHostedDomainPath, state.info.hosted_domain);
  dict->SetString(kAccountFullNamePath, state.info.full_name);
  dict->SetString(kAccountGivenNamePath, state.info.given_name);
  dict->SetString(kAccountLocalePath, state.info.locale);

  scoped_ptr<base::ListValue> service_flags_list;
  service_flags_list.reset(new base::ListValue);
  service_flags_list->AppendStrings(state.info.service_flags);

  dict->Set(kAccountServiceFlagsPath, service_flags_list.Pass());
}

void AccountTrackerService::RemoveFromPrefs(const AccountState& state) {
  if (!signin_client_->GetPrefs())
    return;

  base::string16 account_id_16 = base::UTF8ToUTF16(state.info.account_id);
  ListPrefUpdate update(signin_client_->GetPrefs(), kAccountInfoPref);
  for(size_t i = 0; i < update->GetSize(); ++i) {
    base::DictionaryValue* dict = NULL;
    if (update->GetDictionary(i, &dict)) {
      base::string16 value;
      if (dict->GetString(kAccountKeyPath, &value) && value == account_id_16) {
        update->Remove(i, NULL);
        break;
      }
    }
  }
}

void AccountTrackerService::LoadFromTokenService() {
  std::vector<std::string> accounts = token_service_->GetAccounts();
  for (std::vector<std::string>::const_iterator it = accounts.begin();
       it != accounts.end(); ++it) {
    RefreshAccountInfo(*it, true);
  }
}

void AccountTrackerService::RefreshFromTokenService() {
  DCHECK(network_fetches_enabled_);
  LoadFromTokenService();

  last_updated_ = base::Time::Now();
  signin_client_->GetPrefs()->SetInt64(kAccountTrackerServiceLastUpdate,
                                       last_updated_.ToInternalValue());
  ScheduleNextRefreshFromTokenService();
}

void AccountTrackerService::SendRefreshTokenAnnotationRequest(
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
                &AccountTrackerService::RefreshTokenAnnotationRequestDone,
                base::Unretained(this), account_id));
    // If request was sent AccountTrackerService needs to own request till it
    // finishes.
    if (request)
      refresh_token_annotation_requests_.set(account_id, request.Pass());
  }
#endif
}

void AccountTrackerService::RefreshTokenAnnotationRequestDone(
    const std::string& account_id) {
  refresh_token_annotation_requests_.erase(account_id);
}

std::string AccountTrackerService::PickAccountIdForAccount(
    const std::string& gaia,
    const std::string& email) {
  return PickAccountIdForAccount(signin_client_->GetPrefs(), gaia, email);
}

void AccountTrackerService::ScheduleNextRefreshFromTokenService() {
  DCHECK(!timer_.IsRunning());
  DCHECK(network_fetches_enabled_);

  const base::TimeDelta time_since_update = base::Time::Now() - last_updated_;
  if(time_since_update > kRefreshFromTokenServiceDelay) {
    RefreshFromTokenService();
  } else {
    timer_.Start(FROM_HERE,
                 kRefreshFromTokenServiceDelay - time_since_update,
                 this,
                 &AccountTrackerService::RefreshFromTokenService);
  }
}

// static
std::string AccountTrackerService::PickAccountIdForAccount(
    PrefService* pref_service,
    const std::string& gaia,
    const std::string& email) {
  DCHECK(!gaia.empty() ||
      GetMigrationState(pref_service) == MIGRATION_NOT_STARTED);
  DCHECK(!email.empty());
  switch(GetMigrationState(pref_service)) {
    case MIGRATION_NOT_STARTED:
    case MIGRATION_IN_PROGRESS:
      // Some tests don't use a real email address.  To support these cases,
      // don't try to canonicalize these strings.
      return (email.find('@') == std::string::npos) ? email :
          gaia::CanonicalizeEmail(email);
    case MIGRATION_DONE:
      return gaia;
    default:
      NOTREACHED();
      return email;
  }
}

std::string AccountTrackerService::SeedAccountInfo(const std::string& gaia,
                                                   const std::string& email) {
  const std::string account_id = PickAccountIdForAccount(gaia, email);
  const bool already_exists = ContainsKey(accounts_, account_id);
  StartTrackingAccount(account_id);
  AccountState& state = accounts_[account_id];
  DCHECK(!already_exists || state.info.gaia.empty() || state.info.gaia == gaia);
  state.info.gaia = gaia;
  state.info.email = email;
  SaveToPrefs(state);

  DVLOG(1) << "AccountTrackerService::SeedAccountInfo"
           << " account_id=" << account_id
           << " gaia_id=" << gaia
           << " email=" << email;

  return account_id;
}
