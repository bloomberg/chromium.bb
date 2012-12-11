// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/web_history_service.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/signin/oauth2_token_service.h"
#include "chrome/browser/signin/oauth2_token_service_factory.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace history {

namespace {

const char kHistoryOAuthScope[] =
    "https://www.googleapis.com/auth/chromesync";

const char kHistoryGetHistoryUrl[] =
    "https://history.google.com/history/api/lookup?client=chrome";

// The maximum number of retries for the URLFetcher requests.
const size_t kMaxRetries = 1;

class RequestImpl : public WebHistoryService::Request,
                    private OAuth2TokenService::Consumer,
                    private net::URLFetcherDelegate {
 public:
  virtual ~RequestImpl() {
  }

 private:
  friend class history::WebHistoryService;

  RequestImpl(Profile* profile,
              const std::string& url,
              const WebHistoryService::QueryWebHistoryCallback& callback)
      : profile_(profile), url_(GURL(url)), query_history_callback_(callback) {
  }

  // Tells the request to do its thang.
  void Start() {
    OAuth2TokenService::ScopeSet oauth_scopes;
    oauth_scopes.insert(kHistoryOAuthScope);

    OAuth2TokenService* token_service =
        OAuth2TokenServiceFactory::GetForProfile(profile_);
    token_request_ = token_service->StartRequest(oauth_scopes, this);
  }

  // content::URLFetcherDelegate interface.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    DCHECK_EQ(source, url_fetcher_.get());
    std::string result_body;
    int response_code = url_fetcher_->GetResponseCode();
    url_fetcher_->GetResponseAsString(&result_body);
    url_fetcher_.reset();

    if (response_code == net::HTTP_OK) {
      scoped_ptr<base::Value> value(base::JSONReader::Read(result_body));
      if (value.get() && value->IsType(base::Value::TYPE_DICTIONARY)) {
        query_history_callback_.Run(this,
                                    static_cast<DictionaryValue*>(value.get()));
        return;
      }
    }
    query_history_callback_.Run(this, NULL);
  }

  // OAuth2TokenService::Consumer interface.
  virtual void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const std::string& access_token,
      const base::Time& expiration_time) OVERRIDE {
    token_request_.reset();
    DCHECK(!access_token.empty());

    // Got an access token -- start the actual API request.
    url_fetcher_.reset(CreateUrlFetcher(access_token));
    url_fetcher_->Start();
  }

  virtual void OnGetTokenFailure(
      const OAuth2TokenService::Request* request,
      const GoogleServiceAuthError& error) OVERRIDE {
    token_request_.reset();
    LOG(WARNING) << "Failed to get OAuth token: " << error.ToString();
    query_history_callback_.Run(this, NULL);
  }

  // Helper for creating a new URLFetcher for the API request.
  net::URLFetcher* CreateUrlFetcher(const std::string& access_token) {
    net::URLFetcher* fetcher = net::URLFetcher::Create(
        url_, net::URLFetcher::GET, this);
    fetcher->SetRequestContext(profile_->GetRequestContext());
    fetcher->SetMaxRetries(kMaxRetries);
    fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                          net::LOAD_DO_NOT_SAVE_COOKIES);
    fetcher->AddExtraRequestHeader("Authorization: Bearer " + access_token);
    return fetcher;
  }

  Profile* profile_;

  // The URL of the API endpoint.
  GURL url_;

  // The OAuth2 access token request.
  scoped_ptr<OAuth2TokenService::Request> token_request_;

  // Handles the actual API requests after the OAuth token is acquired.
  scoped_ptr<net::URLFetcher> url_fetcher_;

  // The callback to execute when the query is complete, whether successful
  // or not.
  WebHistoryService::QueryWebHistoryCallback query_history_callback_;
};

}  // namespace

WebHistoryService::Request::~Request() {
}

WebHistoryService::Request::Request() {
}

WebHistoryService::WebHistoryService(Profile* profile)
    : profile_(profile) {
}

WebHistoryService::~WebHistoryService() {
}

scoped_ptr<WebHistoryService::Request> WebHistoryService::QueryHistory(
    const string16& text_query,
    const QueryOptions& options,
    const WebHistoryService::QueryWebHistoryCallback& callback) {
  scoped_ptr<RequestImpl> request(
      new RequestImpl(profile_, kHistoryGetHistoryUrl, callback));
  request->Start();
  return request.PassAs<Request>();
}

}  // namespace history
