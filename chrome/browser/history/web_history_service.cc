// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/web_history_service.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/string_number_conversions.h"
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

const char kHistoryQueryHistoryUrl[] =
    "https://history.google.com/history/api/lookup?client=chrome";

const char kHistoryDeleteHistoryUrl[] =
    "https://history.google.com/history/api/delete?client=chrome";

const char kPostDataMimeType[] = "text/plain";

// The maximum number of retries for the URLFetcher requests.
const size_t kMaxRetries = 1;

class RequestImpl : public WebHistoryService::Request,
                    private OAuth2TokenService::Consumer,
                    private net::URLFetcherDelegate {
 public:
  virtual ~RequestImpl() {
  }

  // Returns the response code received from the server, which will only be
  // valid if the request succeeded.
  int response_code() { return response_code_; }

  // Returns the contents of the response body received from the server.
  const std::string& response_body() { return response_body_; }

 private:
  friend class history::WebHistoryService;

  typedef base::Callback<void(Request*, bool)> CompletionCallback;

  RequestImpl(Profile* profile,
              const std::string& url,
              const CompletionCallback& callback)
      : profile_(profile),
        url_(GURL(url)),
        response_code_(0),
        callback_(callback) {
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
    response_code_ = url_fetcher_->GetResponseCode();
    url_fetcher_->GetResponseAsString(&response_body_);
    url_fetcher_.reset();
    callback_.Run(this, true);
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
    callback_.Run(this, false);
  }

  // Helper for creating a new URLFetcher for the API request.
  net::URLFetcher* CreateUrlFetcher(const std::string& access_token) {
    net::URLFetcher::RequestType request_type = post_data_.empty() ?
        net::URLFetcher::GET : net::URLFetcher::POST;
    net::URLFetcher* fetcher = net::URLFetcher::Create(
        url_, request_type, this);
    fetcher->SetRequestContext(profile_->GetRequestContext());
    fetcher->SetMaxRetriesOn5xx(kMaxRetries);
    fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                          net::LOAD_DO_NOT_SAVE_COOKIES);
    fetcher->AddExtraRequestHeader("Authorization: Bearer " + access_token);
    if (request_type == net::URLFetcher::POST)
      fetcher->SetUploadData(kPostDataMimeType, post_data_);
    return fetcher;
  }

  void set_post_data(const std::string& post_data) {
    post_data_ = post_data;
  }

  Profile* profile_;

  // The URL of the API endpoint.
  GURL url_;

  // POST data to be sent with the request (may be empty).
  std::string post_data_;

  // The OAuth2 access token request.
  scoped_ptr<OAuth2TokenService::Request> token_request_;

  // Handles the actual API requests after the OAuth token is acquired.
  scoped_ptr<net::URLFetcher> url_fetcher_;

  // Holds the response code received from the server.
  int response_code_;

  // Holds the response body received from the server.
  std::string response_body_;

  // The callback to execute when the query is complete.
  CompletionCallback callback_;
};

// Called when a query to web history has completed, successfully or not.
void QueryHistoryCompletionCallback(
    const WebHistoryService::QueryWebHistoryCallback& callback,
    WebHistoryService::Request* request,
    bool success) {
  RequestImpl* request_impl = static_cast<RequestImpl*>(request);
  if (success && request_impl->response_code() == net::HTTP_OK) {
    scoped_ptr<base::Value> value(
        base::JSONReader::Read(request_impl->response_body()));
    if (value.get() && value->IsType(base::Value::TYPE_DICTIONARY))
      callback.Run(request, static_cast<DictionaryValue*>(value.get()));
  }
  callback.Run(request, NULL);
}

}  // namespace

WebHistoryService::Request::Request() {
}

WebHistoryService::Request::~Request() {
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
  // Wrap the original callback into a generic completion callback.
  RequestImpl::CompletionCallback completion_callback = base::Bind(
      &QueryHistoryCompletionCallback, callback);

  scoped_ptr<RequestImpl> request(
      new RequestImpl(profile_, kHistoryQueryHistoryUrl, completion_callback));
  request->Start();
  return request.PassAs<Request>();
}

scoped_ptr<WebHistoryService::Request> WebHistoryService::ExpireHistoryBetween(
    const std::set<GURL>& urls,
    base::Time begin_time,
    base::Time end_time,
    const WebHistoryService::ExpireWebHistoryCallback& callback) {

  // Determine the timestamps representing the beginning and end of the day.
  std::string min_timestamp(base::Int64ToString(
      (begin_time - base::Time::FromJsTime(0)).InMicroseconds()));
  std::string max_timestamp(base::Int64ToString(
      (end_time - base::Time::FromJsTime(0)).InMicroseconds()));

  DictionaryValue delete_request;
  ListValue* deletions = new ListValue;
  delete_request.Set("del", deletions);

  for (std::set<GURL>::const_iterator it = urls.begin();
       it != urls.end(); ++it) {
    DictionaryValue* deletion = new DictionaryValue;
    deletion->SetString("type", "CHROME_HISTORY");
    deletion->SetString("url", it->spec());
    deletion->SetString("min_timestamp_usec", min_timestamp);
    deletion->SetString("max_timestamp_usec", max_timestamp);
    deletions->Append(deletion);
  }
  std::string post_data;
  base::JSONWriter::Write(&delete_request, &post_data);

  scoped_ptr<RequestImpl> request(
      new RequestImpl(profile_, kHistoryDeleteHistoryUrl, callback));
  request->set_post_data(post_data);
  request->Start();
  return request.PassAs<Request>();
}

}  // namespace history
