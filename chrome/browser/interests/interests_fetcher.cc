// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interests/interests_fetcher.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace {

const int kNumRetries = 1;
const char kIdInterests[] = "interests";
const char kIdInterestName[] = "name";
const char kIdInterestImageUrl[] = "imageUrl";
const char kIdInterestRelevance[] = "relevance";

const char kApiScope[] = "https://www.googleapis.com/auth/googlenow";

const char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";

GURL GetInterestsURL() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return GURL(command_line->GetSwitchValueASCII(switches::kInterestsURL));
}

}  // namespace

InterestsFetcher::Interest::Interest(const std::string& name,
                                     const GURL& image_url,
                                     double relevance)
    : name(name), image_url(image_url), relevance(relevance) {}

InterestsFetcher::Interest::~Interest() {}

bool InterestsFetcher::Interest::operator==(const Interest& interest) const {
  return name == interest.name && image_url == interest.image_url &&
         relevance == interest.relevance;
}

InterestsFetcher::InterestsFetcher(
    OAuth2TokenService* oauth2_token_service,
    const std::string& account_id,
    net::URLRequestContextGetter* url_request_context)
    : OAuth2TokenService::Consumer("interests_fetcher"),
      account_id_(account_id),
      url_request_context_(url_request_context),
      access_token_expired_(false),
      token_service_(oauth2_token_service) {
}

InterestsFetcher::~InterestsFetcher() {}

// static
std::unique_ptr<InterestsFetcher> InterestsFetcher::CreateFromProfile(
    Profile* profile) {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);

  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);

  return base::WrapUnique(
      new InterestsFetcher(token_service, signin->GetAuthenticatedAccountId(),
                           profile->GetRequestContext()));
}

void InterestsFetcher::FetchInterests(
    const InterestsFetcher::InterestsCallback& callback) {
  DCHECK(callback_.is_null());
  callback_ = callback;
  StartOAuth2Request();
}

void InterestsFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  const net::URLRequestStatus& status = source->GetStatus();
  if (!status.is_success()) {
    DLOG(WARNING) << "Network error " << status.error();
    callback_.Run(nullptr);
    return;
  }

  int response_code = source->GetResponseCode();
  // If we get an authorization error, refresh token and retry once.
  if (response_code == net::HTTP_UNAUTHORIZED && !access_token_expired_) {
    DLOG(WARNING) << "Authorization error.";
    access_token_expired_ = true;
    token_service_->InvalidateAccessToken(account_id_,
                                          GetApiScopes(),
                                          access_token_);
    StartOAuth2Request();
    return;
  }

  if (response_code != net::HTTP_OK) {
    DLOG(WARNING) << "HTTP error " << response_code;
    callback_.Run(nullptr);
    return;
  }

  std::string response_body;
  source->GetResponseAsString(&response_body);

  callback_.Run(ExtractInterests(response_body));
}

void InterestsFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  access_token_ = access_token;

  fetcher_ = CreateFetcher();
  fetcher_->SetRequestContext(url_request_context_);
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(kNumRetries);

  fetcher_->AddExtraRequestHeader(
      base::StringPrintf(kAuthorizationHeaderFormat, access_token_.c_str()));

  fetcher_->Start();
}

void InterestsFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DLOG(WARNING) << "Failed to get OAuth2 Token: " << error.ToString();

  callback_.Run(nullptr);
}

void InterestsFetcher::StartOAuth2Request() {
  if (account_id_.empty()) {
    DLOG(WARNING) << "AccountId is empty, user is not signed in.";
    return;
  }

  oauth_request_ =
      token_service_->StartRequest(account_id_, GetApiScopes(), this);
}

OAuth2TokenService::ScopeSet InterestsFetcher::GetApiScopes() {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(kApiScope);
  return scopes;
}

std::unique_ptr<net::URLFetcher> InterestsFetcher::CreateFetcher() {
  return
      net::URLFetcher::Create(0, GetInterestsURL(), net::URLFetcher::GET, this);
}

std::unique_ptr<std::vector<InterestsFetcher::Interest>>
InterestsFetcher::ExtractInterests(const std::string& response) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(response);
  DVLOG(2) << response;

  const base::DictionaryValue* dict = nullptr;
  if (!value || !value->GetAsDictionary(&dict)) {
    DLOG(WARNING) << "Failed to parse global dictionary.";
    return nullptr;
  }

  const base::ListValue* interests_list = nullptr;
  if (!dict->GetList(kIdInterests, &interests_list)) {
    DLOG(WARNING) << "Failed to parse interests list.";
    return nullptr;
  }

  std::unique_ptr<std::vector<Interest>> res(new std::vector<Interest>());
  for (const base::Value* entry : *interests_list) {
    const base::DictionaryValue* interest_dict = nullptr;
    if (!entry->GetAsDictionary(&interest_dict)) {
      DLOG(WARNING) << "Failed to parse interest dictionary.";
      continue;
    }

    // Extract the parts of the interest.
    std::string name;
    std::string image_url;
    double relevance = 0;

    if (!interest_dict->GetString(kIdInterestName, &name)) {
      DLOG(WARNING) << "Failed to parse interest name.";
      continue;
    }

    if (!interest_dict->GetString(kIdInterestImageUrl, &image_url)) {
      DLOG(WARNING) << "Failed to parse interest image URL.";
      // image_url is not mandatory, however warn if omitted.
    }

    if (!interest_dict->GetDouble(kIdInterestRelevance, &relevance)) {
      DLOG(WARNING) << "Failed to parse interest relevance.";
      continue;
    }

    res->push_back(Interest{name, GURL(image_url), relevance});
  }

  return res;
}
