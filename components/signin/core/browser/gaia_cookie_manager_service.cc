// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/gaia_cookie_manager_service.h"

#include <stddef.h>

#include <queue>
#include <set>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/ubertoken_fetcher_impl.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace signin {
MultiloginParameters::MultiloginParameters(
    const gaia::MultiloginMode mode,
    const std::vector<std::string>& accounts_to_send)
    : mode(mode), accounts_to_send(accounts_to_send) {}

MultiloginParameters::~MultiloginParameters() {}

MultiloginParameters::MultiloginParameters(const MultiloginParameters& other) {
  mode = other.mode;
  accounts_to_send = other.accounts_to_send;
}

MultiloginParameters& MultiloginParameters::operator=(
    const MultiloginParameters& other) {
  mode = other.mode;
  accounts_to_send = other.accounts_to_send;
  return *this;
}
}  // namespace signin

namespace {

// In case of an error while fetching using the GaiaAuthFetcher or
// SimpleURLLoader, retry with exponential backoff. Try up to 7 times within 15
// minutes.
const net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential backoff in ms.
    1000,

    // Factor by which the waiting time will be multiplied.
    3,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    1000 * 60 * 15,  // 15 minutes.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

// Name of the GAIA cookie that is being observed to detect when available
// accounts have changed in the content-area.
const char* const kGaiaCookieName = "APISID";

// State of requests to Gaia logout endpoint. Used as entry for histogram
// |Signin.GaiaCookieManager.Logout|.
enum LogoutRequestState {
  kStarted = 0,
  kSuccess = 1,
  kFailed = 2,
  kMaxValue = kFailed
};

// Records metrics for ListAccounts failures.
void RecordListAccountsFailure(GoogleServiceAuthError::State error_state) {
  UMA_HISTOGRAM_ENUMERATION("Signin.ListAccountsFailure", error_state,
                            GoogleServiceAuthError::NUM_STATES);
}

void RecordGetAccessTokenFinished(GoogleServiceAuthError error) {
  UMA_HISTOGRAM_ENUMERATION("Signin.GetAccessTokenFinished", error.state(),
                            GoogleServiceAuthError::NUM_STATES);
}

void RecordLogoutRequestState(LogoutRequestState logout_state) {
  UMA_HISTOGRAM_ENUMERATION("Signin.GaiaCookieManager.Logout", logout_state);
}

void RecordMultiloginFinished(GoogleServiceAuthError error) {
  UMA_HISTOGRAM_ENUMERATION("Signin.MultiloginFinished", error.state(),
                            GoogleServiceAuthError::NUM_STATES);
}

// Record ListAccounts errors for individual retries.
void RecordListAccountsRetryResult(GoogleServiceAuthError error,
                                   int retry_attempt_number) {
  int net_error = net::OK;
  switch (error.state()) {
    case GoogleServiceAuthError::NONE:
      net_error = net::OK;
      break;
    case GoogleServiceAuthError::CONNECTION_FAILED:
      net_error = error.network_error();
      break;
    case GoogleServiceAuthError::REQUEST_CANCELED:
      net_error = net::ERR_ABORTED;
      break;
    default:
      return;  // There is an error not related to network.
  }

  std::string histogram_name =
      base::StringPrintf("Gaia.AuthFetcher.ListAccounts.NetErrorCodes.Retry_%i",
                         retry_attempt_number);
  base::UmaHistogramSparse(histogram_name, -net_error);
}

}  // namespace

GaiaCookieManagerService::GaiaCookieRequest::GaiaCookieRequest(
    GaiaCookieRequestType request_type,
    const std::vector<std::string>& account_ids,
    gaia::GaiaSource source)
    : request_type_(request_type), account_ids_(account_ids), source_(source) {}

GaiaCookieManagerService::GaiaCookieRequest::GaiaCookieRequest(
    GaiaCookieRequestType request_type,
    const std::vector<std::string>& account_ids,
    gaia::GaiaSource source,
    SetAccountsInCookieCompletedCallback callback)
    : request_type_(request_type),
      account_ids_(account_ids),
      source_(source),
      set_accounts_in_cookie_completed_callback_(std::move(callback)) {}

GaiaCookieManagerService::GaiaCookieRequest::GaiaCookieRequest(
    GaiaCookieRequestType request_type,
    const std::vector<std::string>& account_ids,
    gaia::GaiaSource source,
    AddAccountToCookieCompletedCallback callback)
    : request_type_(request_type),
      account_ids_(account_ids),
      source_(source),
      add_account_to_cookie_completed_callback_(std::move(callback)) {}

GaiaCookieManagerService::GaiaCookieRequest::~GaiaCookieRequest() {}

GaiaCookieManagerService::GaiaCookieRequest::GaiaCookieRequest(
    GaiaCookieRequest&&) = default;

GaiaCookieManagerService::GaiaCookieRequest&
GaiaCookieManagerService::GaiaCookieRequest::operator=(GaiaCookieRequest&&) =
    default;

const std::string GaiaCookieManagerService::GaiaCookieRequest::GetAccountID() {
  DCHECK_EQ(request_type_, GaiaCookieRequestType::ADD_ACCOUNT);
  DCHECK_EQ(1u, account_ids_.size());
  return account_ids_[0];
}

void GaiaCookieManagerService::GaiaCookieRequest::
    RunSetAccountsInCookieCompletedCallback(
        const GoogleServiceAuthError& error) {
  if (set_accounts_in_cookie_completed_callback_)
    std::move(set_accounts_in_cookie_completed_callback_).Run(error);
}

void GaiaCookieManagerService::GaiaCookieRequest::
    RunAddAccountToCookieCompletedCallback(
        const std::string& account_id,
        const GoogleServiceAuthError& error) {
  if (add_account_to_cookie_completed_callback_)
    std::move(add_account_to_cookie_completed_callback_).Run(account_id, error);
}

// static
GaiaCookieManagerService::GaiaCookieRequest
GaiaCookieManagerService::GaiaCookieRequest::CreateAddAccountRequest(
    const std::string& account_id,
    gaia::GaiaSource source,
    AddAccountToCookieCompletedCallback callback) {
  return GaiaCookieManagerService::GaiaCookieRequest(
      GaiaCookieRequestType::ADD_ACCOUNT, {account_id}, source,
      std::move(callback));
}

// static
GaiaCookieManagerService::GaiaCookieRequest
GaiaCookieManagerService::GaiaCookieRequest::CreateSetAccountsRequest(
    const std::vector<std::string>& account_ids,
    gaia::GaiaSource source,
    SetAccountsInCookieCompletedCallback callback) {
  return GaiaCookieManagerService::GaiaCookieRequest(
      GaiaCookieRequestType::SET_ACCOUNTS, account_ids, source,
      std::move(callback));
}

// static
GaiaCookieManagerService::GaiaCookieRequest
GaiaCookieManagerService::GaiaCookieRequest::CreateLogOutRequest(
    gaia::GaiaSource source) {
  return GaiaCookieManagerService::GaiaCookieRequest(
      GaiaCookieRequestType::LOG_OUT, {}, source);
}

// static
GaiaCookieManagerService::GaiaCookieRequest
GaiaCookieManagerService::GaiaCookieRequest::CreateListAccountsRequest() {
  return GaiaCookieManagerService::GaiaCookieRequest(
      GaiaCookieRequestType::LIST_ACCOUNTS, {}, gaia::GaiaSource::kChrome);
}

GaiaCookieManagerService::ExternalCcResultFetcher::ExternalCcResultFetcher(
    GaiaCookieManagerService* helper)
    : helper_(helper) {
  DCHECK(helper_);
}

GaiaCookieManagerService::ExternalCcResultFetcher::~ExternalCcResultFetcher() {
  CleanupTransientState();
}

std::string
GaiaCookieManagerService::ExternalCcResultFetcher::GetExternalCcResult() {
  std::vector<std::string> results;
  for (ResultMap::const_iterator it = results_.begin(); it != results_.end();
       ++it) {
    results.push_back(it->first + ":" + it->second);
  }
  return base::JoinString(results, ",");
}

void GaiaCookieManagerService::ExternalCcResultFetcher::Start(
    base::OnceClosure callback) {
  DCHECK(!helper_->external_cc_result_fetched_);
  m_external_cc_result_start_time_ = base::Time::Now();
  callback_ = std::move(callback);

  CleanupTransientState();
  results_.clear();
  helper_->gaia_auth_fetcher_ = helper_->signin_client_->CreateGaiaAuthFetcher(
      this, gaia::GaiaSource::kChrome, helper_->GetURLLoaderFactory());
  helper_->gaia_auth_fetcher_->StartGetCheckConnectionInfo();

  // Some fetches may timeout.  Start a timer to decide when the result fetcher
  // has waited long enough. See https://crbug.com/750316#c36 for details on
  // exact timeout duration.
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(7), this,
               &GaiaCookieManagerService::ExternalCcResultFetcher::Timeout);
}

bool GaiaCookieManagerService::ExternalCcResultFetcher::IsRunning() {
  return helper_->gaia_auth_fetcher_ || loaders_.size() > 0u ||
         timer_.IsRunning();
}

void GaiaCookieManagerService::ExternalCcResultFetcher::TimeoutForTests() {
  Timeout();
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    OnGetCheckConnectionInfoSuccess(const std::string& data) {
  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(data);
  const base::ListValue* list;
  if (!value || !value->GetAsList(&list)) {
    CleanupTransientState();
    GetCheckConnectionInfoCompleted(false);
    return;
  }

  // If there is nothing to check, terminate immediately.
  if (list->GetSize() == 0) {
    CleanupTransientState();
    GetCheckConnectionInfoCompleted(true);
    return;
  }

  // Start a fetcher for each connection URL that needs to be checked.
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* dict;
    if (list->GetDictionary(i, &dict)) {
      std::string token;
      std::string url;
      if (dict->GetString("carryBackToken", &token) &&
          dict->GetString("url", &url)) {
        results_[token] = "null";
        network::SimpleURLLoader* loader =
            CreateAndStartLoader(GURL(url)).release();
        loaders_[loader] = token;
      }
    }
  }
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    OnGetCheckConnectionInfoError(const GoogleServiceAuthError& error) {
  VLOG(1) << "GaiaCookieManagerService::ExternalCcResultFetcher::"
          << "OnGetCheckConnectionInfoError " << error.ToString();

  // Chrome does not have any retry logic for fetching ExternalCcResult. The
  // ExternalCcResult is only used to inform Gaia that Chrome has already
  // checked the connection to other sites.
  //
  // In case fetching the ExternalCcResult fails:
  // * The result of merging accounts to Gaia cookies will not be affected.
  // * Gaia will need make its own call about whether to check them itself,
  //   of make some other assumptions.
  CleanupTransientState();
  GetCheckConnectionInfoCompleted(false);
}

std::unique_ptr<network::SimpleURLLoader>
GaiaCookieManagerService::ExternalCcResultFetcher::CreateAndStartLoader(
    const GURL& url) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "gaia_cookie_manager_external_cc_result", R"(
          semantics {
            sender: "Gaia Cookie Manager"
            description:
              "This request is used by the GaiaCookieManager when adding an "
              "account to the Google authentication cookies to check the "
              "authentication server's connection state."
            trigger:
              "This is used at most once per lifetime of the application "
              "during the first merge session flow (the flow used to add an "
              "account for which Chrome has a valid OAuth2 refresh token to "
              "the Gaia authentication cookies). The value of the first fetch "
              "is stored in RAM for future uses."
            data: "None."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting: "This feature cannot be disabled in settings."
            policy_exception_justification:
              "Not implemented. Disabling GaiaCookieManager would break "
              "features that depend on it (like account consistency and "
              "support for child accounts). It makes sense to control top "
              "level features that use the GaiaCookieManager."
          })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  // TODO(https://crbug.com/808498) re-add data use measurement once
  // SimpleURLLoader supports it: data_use_measurement::DataUseUserData::SIGNIN

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);

  // Fetchers are sometimes cancelled because a network change was detected,
  // especially at startup and after sign-in on ChromeOS.
  loader->SetRetryOptions(1, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      helper_->GetURLLoaderFactory().get(),
      base::BindOnce(&ExternalCcResultFetcher::OnURLLoadComplete,
                     base::Unretained(this), loader.get()));

  return loader;
}

void GaiaCookieManagerService::ExternalCcResultFetcher::OnURLLoadComplete(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> body) {
  if (source->NetError() != net::OK || !source->ResponseInfo() ||
      !source->ResponseInfo()->headers ||
      source->ResponseInfo()->headers->response_code() != net::HTTP_OK) {
    return;
  }

  auto it = loaders_.find(source);
  if (it == loaders_.end())
    return;

  std::string data;
  if (body)
    data = std::move(*body);

  // Only up to the first 16 characters of the response are important to GAIA.
  // Truncate if needed to keep amount data sent back to GAIA down.
  if (data.size() > 16)
    data.resize(16);
  results_[it->second] = data;

  // Clean up tracking of this fetcher.  The rest will be cleaned up after
  // the timer expires in CleanupTransientState().
  DCHECK_EQ(source, it->first);
  loaders_.erase(it);
  delete source;

  // If all expected responses have been received, cancel the timer and
  // report the result.
  if (loaders_.empty()) {
    CleanupTransientState();
    GetCheckConnectionInfoCompleted(true);
  }
}

void GaiaCookieManagerService::ExternalCcResultFetcher::Timeout() {
  VLOG(1) << " GaiaCookieManagerService::ExternalCcResultFetcher::Timeout";
  CleanupTransientState();
  GetCheckConnectionInfoCompleted(false);
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    CleanupTransientState() {
  timer_.Stop();
  helper_->gaia_auth_fetcher_.reset();

  for (const auto& loader_token_pair : loaders_) {
    delete loader_token_pair.first;
  }
  loaders_.clear();
}

void GaiaCookieManagerService::ExternalCcResultFetcher::
    GetCheckConnectionInfoCompleted(bool succeeded) {
  base::TimeDelta time_to_check_connections =
      base::Time::Now() - m_external_cc_result_start_time_;
  signin_metrics::LogExternalCcResultFetches(succeeded,
                                             time_to_check_connections);

  helper_->external_cc_result_fetched_ = true;
  std::move(callback_).Run();
}

GaiaCookieManagerService::GaiaCookieManagerService(
    OAuth2TokenService* token_service,
    SigninClient* signin_client)
    : GaiaCookieManagerService(
          token_service,
          signin_client,
          base::BindRepeating(&SigninClient::GetURLLoaderFactory,
                              base::Unretained(signin_client))) {}

GaiaCookieManagerService::GaiaCookieManagerService(
    OAuth2TokenService* token_service,
    SigninClient* signin_client,
    base::RepeatingCallback<scoped_refptr<network::SharedURLLoaderFactory>()>
        shared_url_loader_factory_getter)
    : OAuth2TokenService::Consumer("gaia_cookie_manager"),
      token_service_(token_service),
      signin_client_(signin_client),
      shared_url_loader_factory_getter_(shared_url_loader_factory_getter),
      external_cc_result_fetcher_(this),
      fetcher_backoff_(&kBackoffPolicy),
      fetcher_retries_(0),
      cookie_listener_binding_(this),
      external_cc_result_fetched_(false),
      list_accounts_stale_(true),
      weak_ptr_factory_(this) {}

GaiaCookieManagerService::~GaiaCookieManagerService() {
  CancelAll();
  DCHECK(requests_.empty());
}

void GaiaCookieManagerService::InitCookieListener() {
  DCHECK(!cookie_listener_binding_);
  network::mojom::CookieManager* cookie_manager =
      signin_client_->GetCookieManager();

  // NOTE: |cookie_manager| can be nullptr when TestSigninClient is used in
  // testing contexts.
  if (cookie_manager) {
    network::mojom::CookieChangeListenerPtr listener_ptr;
    cookie_listener_binding_.Bind(mojo::MakeRequest(&listener_ptr));
    cookie_listener_binding_.set_connection_error_handler(base::BindOnce(
        &GaiaCookieManagerService::OnCookieListenerConnectionError,
        base::Unretained(this)));

    cookie_manager->AddCookieChangeListener(
        GaiaUrls::GetInstance()->google_url(), kGaiaCookieName,
        std::move(listener_ptr));
  }
}

void GaiaCookieManagerService::Shutdown() {
  cookie_listener_binding_.Close();
}

void GaiaCookieManagerService::SetAccountsInCookie(
    const std::vector<std::string>& account_ids,
    gaia::GaiaSource source,
    SetAccountsInCookieCompletedCallback
        set_accounts_in_cookies_completed_callback) {
  VLOG(1) << "GaiaCookieManagerService::SetAccountsInCookie: "
          << base::JoinString(account_ids, " ");
  requests_.push_back(GaiaCookieRequest::CreateSetAccountsRequest(
      account_ids, source,
      std::move(set_accounts_in_cookies_completed_callback)));
  if (!signin_client_->AreSigninCookiesAllowed()) {
    OnSetAccountsFinished(
        GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
    return;
  }
  if (requests_.size() == 1) {
    fetcher_retries_ = 0;
    signin_client_->DelayNetworkCall(base::BindOnce(
        &GaiaCookieManagerService::StartFetchingAccessTokensForMultilogin,
        base::Unretained(this)));
  }
}

void GaiaCookieManagerService::SetAccountsInCookieWithTokens() {
#ifndef NDEBUG
  // Check that there is no duplicate accounts.
  std::set<std::string> accounts_no_duplicates(
      requests_.front().account_ids().begin(),
      requests_.front().account_ids().end());
  DCHECK_EQ(requests_.front().account_ids().size(),
            accounts_no_duplicates.size());
#endif

  std::vector<GaiaAuthFetcher::MultiloginTokenIDPair> accounts =
      std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>();
  accounts.reserve(requests_.front().account_ids().size());
  int i = 0;
  for (const std::string& account_id : requests_.front().account_ids()) {
    accounts.emplace_back(account_id, access_tokens_[account_id]);
    ++i;
  }
  StartFetchingMultiLogin(accounts);
}

void GaiaCookieManagerService::AddAccountToCookieInternal(
    const std::string& account_id,
    gaia::GaiaSource source,
    AddAccountToCookieCompletedCallback completion_callback) {
  DCHECK(!account_id.empty());
  requests_.push_back(GaiaCookieRequest::CreateAddAccountRequest(
      account_id, source, std::move(completion_callback)));

  if (!signin_client_->AreSigninCookiesAllowed()) {
    SignalAddToCookieComplete(
        requests_.begin(),
        GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
    return;
  }

  if (requests_.size() == 1) {
    signin_client_->DelayNetworkCall(
        base::BindOnce(&GaiaCookieManagerService::StartFetchingUbertoken,
                       base::Unretained(this)));
  }
}

void GaiaCookieManagerService::AddAccountToCookie(
    const std::string& account_id,
    gaia::GaiaSource source,
    AddAccountToCookieCompletedCallback completion_callback) {
  VLOG(1) << "GaiaCookieManagerService::AddAccountToCookie: " << account_id;
  access_token_ = std::string();
  AddAccountToCookieInternal(account_id, source,
                             std::move(completion_callback));
}

void GaiaCookieManagerService::AddAccountToCookieWithToken(
    const std::string& account_id,
    const std::string& access_token,
    gaia::GaiaSource source,
    AddAccountToCookieCompletedCallback completion_callback) {
  VLOG(1) << "GaiaCookieManagerService::AddAccountToCookieWithToken: "
          << account_id;
  DCHECK(!access_token.empty());
  access_token_ = access_token;
  AddAccountToCookieInternal(account_id, source,
                             std::move(completion_callback));
}

bool GaiaCookieManagerService::ListAccounts(
    std::vector<gaia::ListedAccount>* accounts,
    std::vector<gaia::ListedAccount>* signed_out_accounts) {
  if (accounts)
    accounts->assign(listed_accounts_.begin(), listed_accounts_.end());

  if (signed_out_accounts) {
    signed_out_accounts->assign(signed_out_accounts_.begin(),
                                signed_out_accounts_.end());
  }

  if (list_accounts_stale_) {
    TriggerListAccounts();
    return false;
  }

  return true;
}

void GaiaCookieManagerService::TriggerListAccounts() {
  if (requests_.empty()) {
    fetcher_retries_ = 0;
    requests_.push_back(GaiaCookieRequest::CreateListAccountsRequest());
    signin_client_->DelayNetworkCall(
        base::BindOnce(&GaiaCookieManagerService::StartFetchingListAccounts,
                       base::Unretained(this)));
  } else if (std::find_if(requests_.begin(), requests_.end(),
                          [](const GaiaCookieRequest& request) {
                            return request.request_type() == LIST_ACCOUNTS;
                          }) == requests_.end()) {
    requests_.push_back(GaiaCookieRequest::CreateListAccountsRequest());
  }
}

void GaiaCookieManagerService::ForceOnCookieChangeProcessing() {
  GURL google_url = GaiaUrls::GetInstance()->google_url();
  std::unique_ptr<net::CanonicalCookie> cookie(
      std::make_unique<net::CanonicalCookie>(
          kGaiaCookieName, std::string(), "." + google_url.host(), "/",
          base::Time(), base::Time(), base::Time(), false, false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT));
  OnCookieChange(*cookie, network::mojom::CookieChangeCause::UNKNOWN_DELETION);
}

void GaiaCookieManagerService::LogOutAllAccounts(gaia::GaiaSource source) {
  VLOG(1) << "GaiaCookieManagerService::LogOutAllAccounts";

  bool log_out_queued = false;
  if (!requests_.empty()) {
    // Track requests to keep; all other unstarted requests will be removed.
    std::vector<GaiaCookieRequest> requests_to_keep;

    // Check all pending, non-executing requests.
    for (auto it = requests_.begin() + 1; it != requests_.end(); ++it) {
      if (it->request_type() == GaiaCookieRequestType::ADD_ACCOUNT) {
        // We have a pending log in request for an account followed by
        // a signout.
        GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
        SignalAddToCookieComplete(it, error);
      }

      // Keep all requests except for ADD_ACCOUNTS.
      if (it->request_type() != GaiaCookieRequestType::ADD_ACCOUNT)
        requests_to_keep.push_back(std::move(*it));

      // Verify a LOG_OUT isn't already queued.
      if (it->request_type() == GaiaCookieRequestType::LOG_OUT)
        log_out_queued = true;
    }

    // Verify a LOG_OUT isn't currently being processed.
    if (requests_.front().request_type() == GaiaCookieRequestType::LOG_OUT)
      log_out_queued = true;

    // Remove all but the executing request. Re-add all requests being kept.
    if (requests_.size() > 1) {
      requests_.erase(requests_.begin() + 1, requests_.end());
      requests_.insert(requests_.end(),
                       std::make_move_iterator(requests_to_keep.begin()),
                       std::make_move_iterator(requests_to_keep.end()));
    }
  }

  if (!log_out_queued) {
    requests_.push_back(GaiaCookieRequest::CreateLogOutRequest(source));
    if (requests_.size() == 1) {
      fetcher_retries_ = 0;
      signin_client_->DelayNetworkCall(base::BindOnce(
          &GaiaCookieManagerService::StartGaiaLogOut, base::Unretained(this)));
    }
  }
}

void GaiaCookieManagerService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void GaiaCookieManagerService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void GaiaCookieManagerService::CancelAll() {
  VLOG(1) << "GaiaCookieManagerService::CancelAll";
  gaia_auth_fetcher_.reset();
  uber_token_fetcher_.reset();
  requests_.clear();
  fetcher_timer_.Stop();
}

scoped_refptr<network::SharedURLLoaderFactory>
GaiaCookieManagerService::GetURLLoaderFactory() {
  return shared_url_loader_factory_getter_.Run();
}

void GaiaCookieManagerService::MarkListAccountsStale() {
  list_accounts_stale_ = true;
#if defined(OS_IOS)
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&GaiaCookieManagerService::ForceOnCookieChangeProcessing,
                     weak_ptr_factory_.GetWeakPtr()));
#endif  // defined(OS_IOS)
}

void GaiaCookieManagerService::OnCookieChange(
    const net::CanonicalCookie& cookie,
    network::mojom::CookieChangeCause cause) {
  DCHECK_EQ(kGaiaCookieName, cookie.Name());
  DCHECK(cookie.IsDomainMatch(GaiaUrls::GetInstance()->google_url().host()));
  list_accounts_stale_ = true;

  if (cause == network::mojom::CookieChangeCause::EXPLICIT) {
    DCHECK(net::CookieChangeCauseIsDeletion(net::CookieChangeCause::EXPLICIT));
    for (auto& observer : observer_list_) {
      observer.OnGaiaCookieDeletedByUserAction();
    }
  }

  // Ignore changes to the cookie while requests are pending.  These changes
  // are caused by the service itself as it adds accounts.  A side effects is
  // that any changes to the gaia cookie outside of this class, while requests
  // are pending, will be lost.  However, trying to process these changes could
  // cause an endless loop (see crbug.com/516070).
  if (requests_.empty()) {
    requests_.push_back(GaiaCookieRequest::CreateListAccountsRequest());
    fetcher_retries_ = 0;
    signin_client_->DelayNetworkCall(
        base::BindOnce(&GaiaCookieManagerService::StartFetchingListAccounts,
                       base::Unretained(this)));
  }
}

void GaiaCookieManagerService::OnCookieListenerConnectionError() {
  // A connection error from the CookieManager likely means that the network
  // service process has crashed. Try again to set up a listener.
  cookie_listener_binding_.Close();
  InitCookieListener();
}

void GaiaCookieManagerService::SignalAddToCookieComplete(
    const base::circular_deque<GaiaCookieRequest>::iterator& request,
    const GoogleServiceAuthError& error) {
  // SignalAddToCookieComplete is called in two circumstances:
  //
  // - normal flow: this happens when SignalAddToCookieComplete is called at
  // the end of processing a ADD_ACCOUNT request.
  //
  // - during a LogOut operation: When logging out, any queue request to
  // ADD_ACCOUNT is canceled (which implies that it is possible to run the
  // completion callback of a request that it not front of the queue).

  request->RunAddAccountToCookieCompletedCallback(request->GetAccountID(),
                                                  error);
}

void GaiaCookieManagerService::SignalSetAccountsComplete(
    const GoogleServiceAuthError& error) {
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::SET_ACCOUNTS);
  requests_.front().RunSetAccountsInCookieCompletedCallback(error);
}

void GaiaCookieManagerService::OnUbertokenFetchComplete(
    GoogleServiceAuthError error,
    const std::string& uber_token) {
  if (error != GoogleServiceAuthError::AuthErrorNone()) {
    // Note that the UberToken fetcher already retries transient errors.
    const std::string account_id = requests_.front().GetAccountID();
    VLOG(1) << "Failed to retrieve ubertoken"
            << " account=" << account_id << " error=" << error.ToString();
    SignalAddToCookieComplete(requests_.begin(), error);
    HandleNextRequest();
    return;
  }

  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::ADD_ACCOUNT);
  VLOG(1) << "GaiaCookieManagerService::OnUbertokenSuccess"
          << " account=" << requests_.front().GetAccountID();
  fetcher_retries_ = 0;
  uber_token_ = uber_token;

  if (!external_cc_result_fetched_ &&
      !external_cc_result_fetcher_.IsRunning()) {
    external_cc_result_fetcher_.Start(
        base::BindOnce(&GaiaCookieManagerService::StartFetchingMergeSession,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  signin_client_->DelayNetworkCall(
      base::BindOnce(&GaiaCookieManagerService::StartFetchingMergeSession,
                     base::Unretained(this)));
}

void GaiaCookieManagerService::OnTokenFetched(const std::string& account_id,
                                              const std::string& token) {
  access_tokens_.insert(std::make_pair(account_id, token));
  if (access_tokens_.size() == requests_.front().account_ids().size()) {
    fetcher_retries_ = 0;
    token_requests_.clear();
    signin_client_->DelayNetworkCall(
        base::BindOnce(&GaiaCookieManagerService::SetAccountsInCookieWithTokens,
                       base::Unretained(this)));
  }
}

void GaiaCookieManagerService::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::SET_ACCOUNTS);
  fetcher_backoff_.InformOfRequest(true);
  OnTokenFetched(request->GetAccountId(), token_response.access_token);
}

void GaiaCookieManagerService::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Failed to retrieve accesstoken"
          << " account=" << request->GetAccountId()
          << " error=" << error.ToString();
  if (++fetcher_retries_ < signin::kMaxFetcherRetries &&
      error.IsTransientError()) {
    fetcher_backoff_.InformOfRequest(false);
    UMA_HISTOGRAM_ENUMERATION("Signin.GetAccessTokenRetry", error.state(),
                              GoogleServiceAuthError::NUM_STATES);
    OAuth2TokenService::ScopeSet scopes;
    scopes.insert(GaiaConstants::kOAuth1LoginScope);
    fetcher_timer_.Start(
        FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(),
        base::BindOnce(
            &SigninClient::DelayNetworkCall, base::Unretained(signin_client_),
            base::BindOnce(&GaiaCookieManagerService::
                               StartFetchingAccessTokenForMultilogin,
                           base::Unretained(this), request->GetAccountId())));
    return;
  }
  RecordGetAccessTokenFinished(error);
  OnSetAccountsFinished(error);
}

void GaiaCookieManagerService::OnMergeSessionSuccess(const std::string& data) {
  const std::string account_id = requests_.front().GetAccountID();
  VLOG(1) << "MergeSession successful account=" << account_id;
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::ADD_ACCOUNT);

  MarkListAccountsStale();
  SignalAddToCookieComplete(requests_.begin(),
                            GoogleServiceAuthError::AuthErrorNone());
  HandleNextRequest();

  fetcher_backoff_.InformOfRequest(true);
  uber_token_ = std::string();
}

void GaiaCookieManagerService::OnMergeSessionFailure(
    const GoogleServiceAuthError& error) {
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::ADD_ACCOUNT);
  const std::string account_id = requests_.front().GetAccountID();
  VLOG(1) << "Failed MergeSession"
          << " account=" << account_id << " error=" << error.ToString();
  if (++fetcher_retries_ < signin::kMaxFetcherRetries &&
      error.IsTransientError()) {
    fetcher_backoff_.InformOfRequest(false);
    UMA_HISTOGRAM_ENUMERATION("OAuth2Login.MergeSessionRetry", error.state(),
                              GoogleServiceAuthError::NUM_STATES);
    fetcher_timer_.Start(
        FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(),
        base::BindOnce(
            &SigninClient::DelayNetworkCall, base::Unretained(signin_client_),
            base::BindOnce(&GaiaCookieManagerService::StartFetchingMergeSession,
                           base::Unretained(this))));
    return;
  }

  uber_token_ = std::string();

  UMA_HISTOGRAM_ENUMERATION("OAuth2Login.MergeSessionFailure", error.state(),
                            GoogleServiceAuthError::NUM_STATES);
  SignalAddToCookieComplete(requests_.begin(), error);
  HandleNextRequest();
}

void GaiaCookieManagerService::OnOAuthMultiloginFinished(
    const OAuthMultiloginResult& result) {
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::SET_ACCOUNTS);
  RecordMultiloginFinished(result.error());
  if (result.error().state() == GoogleServiceAuthError::NONE) {
    VLOG(1) << "Multilogin successful accounts="
            << base::JoinString(requests_.front().account_ids(), " ");
    std::vector<std::string> account_ids = requests_.front().account_ids();
    access_tokens_.clear();
    fetcher_backoff_.InformOfRequest(true);
    StartSettingCookies(result);
    return;
  }
  if (++fetcher_retries_ < signin::kMaxFetcherRetries &&
      result.error().IsTransientError()) {
    UMA_HISTOGRAM_ENUMERATION("Signin.MultiloginRetry", result.error().state(),
                              GoogleServiceAuthError::NUM_STATES);
    fetcher_backoff_.InformOfRequest(false);
    fetcher_timer_.Start(
        FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(),
        base::BindOnce(
            &SigninClient::DelayNetworkCall, base::Unretained(signin_client_),
            base::BindOnce(
                &GaiaCookieManagerService::SetAccountsInCookieWithTokens,
                base::Unretained(this))));
    return;
  }
  // If Gaia responded with status: "INVALID_TOKENS", we have to mark tokens as
  // invalid.
  if (result.error().state() ==
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
    for (const std::string& account_id : result.failed_accounts()) {
      DCHECK(base::ContainsKey(access_tokens_, account_id));
      token_service_->InvalidateTokenForMultilogin(account_id,
                                                   access_tokens_[account_id]);
      access_tokens_.erase(account_id);
    }
    for (const std::string& account_id : result.failed_accounts()) {
      // Maybe the access token was expired, try to get a new one.
      StartFetchingAccessTokenForMultilogin(account_id);
    }
    return;
  }
  OnSetAccountsFinished(result.error());
}

void GaiaCookieManagerService::OnListAccountsSuccess(const std::string& data) {
  VLOG(1) << "ListAccounts successful";
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::LIST_ACCOUNTS);
  fetcher_backoff_.InformOfRequest(true);

  if (!gaia::ParseListAccountsData(data, &listed_accounts_,
                                   &signed_out_accounts_)) {
    listed_accounts_.clear();
    signed_out_accounts_.clear();
    GoogleServiceAuthError error(
        GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE);
    RecordListAccountsFailure(error.state());
    OnListAccountsFailure(error);
    return;
  }

  RecordListAccountsFailure(GoogleServiceAuthError::NONE);
  RecordListAccountsRetryResult(GoogleServiceAuthError::AuthErrorNone(),
                                fetcher_retries_);

  for (gaia::ListedAccount& account : listed_accounts_) {
    DCHECK(account.id.empty());
    account.id = AccountTrackerService::PickAccountIdForAccount(
        signin_client_->GetPrefs(), account.gaia_id, account.email);
  }

  list_accounts_stale_ = false;
  HandleNextRequest();
  // HandleNextRequest before sending out the notification because some
  // services, in response to OnGaiaAccountsInCookieUpdated, may try in return
  // to call ListAccounts, which would immediately return false if the
  // ListAccounts request is still sitting in queue.
  for (auto& observer : observer_list_) {
    observer.OnGaiaAccountsInCookieUpdated(
        listed_accounts_, signed_out_accounts_,
        GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  }
}

void GaiaCookieManagerService::OnListAccountsFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "ListAccounts failed";
  DCHECK(requests_.front().request_type() ==
         GaiaCookieRequestType::LIST_ACCOUNTS);
  RecordListAccountsRetryResult(error, fetcher_retries_);

  if (++fetcher_retries_ < signin::kMaxFetcherRetries &&
      error.IsTransientError()) {
    fetcher_backoff_.InformOfRequest(false);
    UMA_HISTOGRAM_ENUMERATION("Signin.ListAccountsRetry", error.state(),
                              GoogleServiceAuthError::NUM_STATES);
    fetcher_timer_.Start(
        FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(),
        base::BindOnce(
            &SigninClient::DelayNetworkCall, base::Unretained(signin_client_),
            base::BindOnce(&GaiaCookieManagerService::StartFetchingListAccounts,
                           base::Unretained(this))));
    return;
  }

  RecordListAccountsFailure(error.state());
  for (auto& observer : observer_list_) {
    observer.OnGaiaAccountsInCookieUpdated(listed_accounts_,
                                           signed_out_accounts_, error);
  }
  HandleNextRequest();
}

void GaiaCookieManagerService::OnLogOutSuccess() {
  DCHECK(requests_.front().request_type() == GaiaCookieRequestType::LOG_OUT);
  VLOG(1) << "GaiaCookieManagerService::OnLogOutSuccess";
  RecordLogoutRequestState(LogoutRequestState::kSuccess);

  MarkListAccountsStale();
  fetcher_backoff_.InformOfRequest(true);
  HandleNextRequest();
}

void GaiaCookieManagerService::OnLogOutFailure(
    const GoogleServiceAuthError& error) {
  DCHECK(requests_.front().request_type() == GaiaCookieRequestType::LOG_OUT);
  VLOG(1) << "GaiaCookieManagerService::OnLogOutFailure";
  RecordLogoutRequestState(LogoutRequestState::kFailed);

  if (++fetcher_retries_ < signin::kMaxFetcherRetries) {
    fetcher_backoff_.InformOfRequest(false);
    fetcher_timer_.Start(
        FROM_HERE, fetcher_backoff_.GetTimeUntilRelease(),
        base::BindOnce(&SigninClient::DelayNetworkCall,
                       base::Unretained(signin_client_),
                       base::Bind(&GaiaCookieManagerService::StartGaiaLogOut,
                                  base::Unretained(this))));
    return;
  }

  HandleNextRequest();
}

void GaiaCookieManagerService::StartFetchingAccessTokenForMultilogin(
    const std::string& account_id) {
  token_requests_.push_back(
      token_service_->StartRequestForMultilogin(account_id, this));
}

void GaiaCookieManagerService::StartFetchingAccessTokensForMultilogin() {
  DCHECK_EQ(SET_ACCOUNTS, requests_.front().request_type());
  VLOG(1) << "GaiaCookieManagerService::StartFetchingAccessToken account_id ="
          << base::JoinString(requests_.front().account_ids(), " ");

  if (!external_cc_result_fetched_ &&
      !external_cc_result_fetcher_.IsRunning()) {
    external_cc_result_fetcher_.Start(base::BindOnce(
        &GaiaCookieManagerService::StartFetchingAccessTokensForMultilogin,
        weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  token_requests_.clear();
  access_tokens_.clear();
  for (const std::string& account_id : requests_.front().account_ids()) {
    StartFetchingAccessTokenForMultilogin(account_id);
  }
}

void GaiaCookieManagerService::StartFetchingUbertoken() {
  const std::string account_id = requests_.front().GetAccountID();
  VLOG(1) << "GaiaCookieManagerService::StartFetchingUbertoken account_id="
          << requests_.front().GetAccountID();
  uber_token_fetcher_ = std::make_unique<signin::UbertokenFetcherImpl>(
      account_id, access_token_, token_service_,
      base::BindOnce(&GaiaCookieManagerService::OnUbertokenFetchComplete,
                     base::Unretained(this)),
      base::BindRepeating(
          [](SigninClient* client,
             scoped_refptr<network::SharedURLLoaderFactory> url_loader,
             GaiaAuthConsumer* consumer) -> std::unique_ptr<GaiaAuthFetcher> {
            return client->CreateGaiaAuthFetcher(
                consumer, gaia::GaiaSource::kChrome, url_loader);
          },
          base::Unretained(signin_client_), GetURLLoaderFactory()));
}

void GaiaCookieManagerService::StartFetchingMultiLogin(
    const std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>& accounts) {
  DCHECK(external_cc_result_fetched_);
  gaia_auth_fetcher_ = signin_client_->CreateGaiaAuthFetcher(
      this, requests_.front().source(), GetURLLoaderFactory());

  gaia_auth_fetcher_->StartOAuthMultilogin(
      accounts, external_cc_result_fetcher_.GetExternalCcResult());
}

void GaiaCookieManagerService::StartFetchingMergeSession() {
  DCHECK(!uber_token_.empty());
  gaia_auth_fetcher_ = signin_client_->CreateGaiaAuthFetcher(
      this, requests_.front().source(), GetURLLoaderFactory());

  gaia_auth_fetcher_->StartMergeSession(
      uber_token_, external_cc_result_fetcher_.GetExternalCcResult());
}

void GaiaCookieManagerService::StartGaiaLogOut() {
  DCHECK(requests_.front().request_type() == GaiaCookieRequestType::LOG_OUT);
  VLOG(1) << "GaiaCookieManagerService::StartGaiaLogOut";

  signin_client_->PreGaiaLogout(base::BindOnce(
      &GaiaCookieManagerService::StartFetchingLogOut, base::Unretained(this)));
}

void GaiaCookieManagerService::StartFetchingLogOut() {
  RecordLogoutRequestState(LogoutRequestState::kStarted);
  gaia_auth_fetcher_ = signin_client_->CreateGaiaAuthFetcher(
      this, requests_.front().source(), GetURLLoaderFactory());
  bool use_continue_url = false;
#if defined(OS_ANDROID)
  use_continue_url = base::FeatureList::IsEnabled(signin::kMiceFeature);
#endif
  if (use_continue_url) {
    gaia_auth_fetcher_->StartLogOutWithBlankContinueURL();
  } else {
    gaia_auth_fetcher_->StartLogOut();
  }
}

void GaiaCookieManagerService::StartFetchingListAccounts() {
  VLOG(1) << "GaiaCookieManagerService::ListAccounts";
  gaia_auth_fetcher_ = signin_client_->CreateGaiaAuthFetcher(
      this, requests_.front().source(), GetURLLoaderFactory());
  gaia_auth_fetcher_->StartListAccounts();
}

void GaiaCookieManagerService::OnSetAccountsFinished(
    const GoogleServiceAuthError& error) {
  MarkListAccountsStale();
  access_tokens_.clear();
  token_requests_.clear();
  cookies_to_set_.clear();
  SignalSetAccountsComplete(error);
  HandleNextRequest();
}

void GaiaCookieManagerService::OnCookieSet(
    const std::string& cookie_name,
    const std::string& cookie_domain,
    net::CanonicalCookie::CookieInclusionStatus status) {
  cookies_to_set_.erase(std::make_pair(cookie_name, cookie_domain));
  bool success =
      (status == net::CanonicalCookie::CookieInclusionStatus::INCLUDE);
  if (!success) {
    VLOG(1) << "Failed to set cookie " << cookie_name
            << " for domain=" << cookie_domain << ".";
  }
  UMA_HISTOGRAM_BOOLEAN("Signin.SetCookieSuccess", success);
  if (cookies_to_set_.empty())
    OnSetAccountsFinished(GoogleServiceAuthError::AuthErrorNone());
}

void GaiaCookieManagerService::StartSettingCookies(
    const OAuthMultiloginResult& result) {
  network::mojom::CookieManager* cookie_manager =
      signin_client_->GetCookieManager();

  DCHECK(cookies_to_set_.empty());

  const std::vector<net::CanonicalCookie>& cookies = result.cookies();

  for (const net::CanonicalCookie& cookie : cookies) {
    cookies_to_set_.insert(std::make_pair(cookie.Name(), cookie.Domain()));
  }
  for (const net::CanonicalCookie& cookie : cookies) {
    if (cookies_to_set_.find(std::make_pair(cookie.Name(), cookie.Domain())) !=
        cookies_to_set_.end()) {
      base::OnceCallback<void(net::CanonicalCookie::CookieInclusionStatus)>
          callback = base::BindOnce(&GaiaCookieManagerService::OnCookieSet,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    cookie.Name(), cookie.Domain());
      net::CookieOptions options;
      options.set_include_httponly();
      // Permit it to set a SameSite cookie if it wants to.
      options.set_same_site_cookie_context(
          net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
      cookie_manager->SetCanonicalCookie(
          cookie, "https", options,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(callback), net::CanonicalCookie::CookieInclusionStatus::
                                       EXCLUDE_UNKNOWN_ERROR));
    } else {
      LOG(ERROR) << "Duplicate cookie found: " << cookie.Name() << " "
                 << cookie.Domain();
    }
  }
}

void GaiaCookieManagerService::HandleNextRequest() {
  VLOG(1) << "GaiaCookieManagerService::HandleNextRequest";
  if (requests_.front().request_type() ==
      GaiaCookieRequestType::LIST_ACCOUNTS) {
    // This and any directly subsequent list accounts would return the same.
    while (!requests_.empty() && requests_.front().request_type() ==
                                     GaiaCookieRequestType::LIST_ACCOUNTS) {
      requests_.pop_front();
    }
  } else {
    // Pop the completed request.
    requests_.pop_front();
  }

  gaia_auth_fetcher_.reset();
  fetcher_retries_ = 0;
  if (requests_.empty()) {
    VLOG(1) << "GaiaCookieManagerService::HandleNextRequest: no more";
    uber_token_fetcher_.reset();
    access_token_ = std::string();
  } else {
    switch (requests_.front().request_type()) {
      case GaiaCookieRequestType::ADD_ACCOUNT:
        DCHECK_EQ(1u, requests_.front().account_ids().size());
        signin_client_->DelayNetworkCall(
            base::BindOnce(&GaiaCookieManagerService::StartFetchingUbertoken,
                           base::Unretained(this)));
        break;
      case GaiaCookieRequestType::SET_ACCOUNTS:
        DCHECK(!requests_.front().account_ids().empty());
        signin_client_->DelayNetworkCall(base::BindOnce(
            &GaiaCookieManagerService::StartFetchingAccessTokensForMultilogin,
            base::Unretained(this)));
        break;
      case GaiaCookieRequestType::LOG_OUT:
        DCHECK(requests_.front().account_ids().empty());
        signin_client_->DelayNetworkCall(
            base::BindOnce(&GaiaCookieManagerService::StartGaiaLogOut,
                           base::Unretained(this)));
        break;
      case GaiaCookieRequestType::LIST_ACCOUNTS:
        DCHECK(requests_.front().account_ids().empty());
        uber_token_fetcher_.reset();
        signin_client_->DelayNetworkCall(
            base::BindOnce(&GaiaCookieManagerService::StartFetchingListAccounts,
                           base::Unretained(this)));
        break;
    }
  }
}
