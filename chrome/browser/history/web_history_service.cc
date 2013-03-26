// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/web_history_service.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/signin/oauth2_token_service.h"
#include "chrome/browser/signin/oauth2_token_service_factory.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
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
        auth_retry_count_(0),
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

    // If the response code indicates that the token might not be valid,
    // invalidate the token and try again.
    if (response_code_ == net::HTTP_UNAUTHORIZED && ++auth_retry_count_ <= 1) {
      OAuth2TokenService::ScopeSet oauth_scopes;
      oauth_scopes.insert(kHistoryOAuthScope);
      OAuth2TokenServiceFactory::GetForProfile(profile_)->InvalidateToken(
          oauth_scopes, access_token_);

      access_token_ = std::string();
      Start();
      return;
    }
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
    access_token_ = access_token;

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
    fetcher->AddExtraRequestHeader("X-Developer-Key: " +
        GaiaUrls::GetInstance()->oauth2_chrome_client_id());
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

  // The current OAuth2 access token.
  std::string access_token_;

  // Handles the actual API requests after the OAuth token is acquired.
  scoped_ptr<net::URLFetcher> url_fetcher_;

  // Holds the response code received from the server.
  int response_code_;

  // Holds the response body received from the server.
  std::string response_body_;

  // The number of times this request has already been retried due to
  // authorization problems.
  int auth_retry_count_;

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
    if (value.get() && value->IsType(base::Value::TYPE_DICTIONARY)) {
      callback.Run(request, static_cast<DictionaryValue*>(value.get()));
      return;
    }
  }
  callback.Run(request, NULL);
}

// Converts a time into a string for use as a parameter in a request to the
// history server.
std::string ServerTimeString(base::Time time) {
  return base::Int64ToString((time - base::Time::UnixEpoch()).InMicroseconds());
}

// Returns a URL for querying the history server for a query specified by
// |options|.
std::string GetQueryUrl(const string16& text_query,
                        const QueryOptions& options) {
  GURL url = GURL(kHistoryQueryHistoryUrl);
  url = net::AppendQueryParameter(url, "titles", "1");

  // Take |begin_time|, |end_time|, and |max_count| from the original query
  // options, and convert them to the equivalent URL parameters.

  base::Time end_time =
      std::min(base::Time::FromInternalValue(options.EffectiveEndTime()),
               base::Time::Now());
  url = net::AppendQueryParameter(url, "max", ServerTimeString(end_time));

  if (!options.begin_time.is_null()) {
    url = net::AppendQueryParameter(
        url, "min", ServerTimeString(options.begin_time));
  }

  if (options.max_count) {
    url = net::AppendQueryParameter(
        url, "num", base::IntToString(options.max_count));
  }

  if (!text_query.empty())
    url = net::AppendQueryParameter(url, "q", UTF16ToUTF8(text_query));

  return url.spec();
}

// Creates a DictionaryValue to hold the parameters for a deletion.
// Ownership is passed to the caller.
// |url| may be empty, indicating a time-range deletion.
DictionaryValue* CreateDeletion(
    const std::string& min_time,
    const std::string& max_time,
    const GURL& url) {
  DictionaryValue* deletion = new DictionaryValue;
  deletion->SetString("type", "CHROME_HISTORY");
  if (url.is_valid())
    deletion->SetString("url", url.spec());
  deletion->SetString("min_timestamp_usec", min_time);
  deletion->SetString("max_timestamp_usec", max_time);
  return deletion;
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

  std::string url = GetQueryUrl(text_query, options);
  scoped_ptr<RequestImpl> request(
      new RequestImpl(profile_, url, completion_callback));
  request->Start();
  return request.PassAs<Request>();
}

scoped_ptr<WebHistoryService::Request> WebHistoryService::ExpireHistory(
    const std::vector<ExpireHistoryArgs>& expire_list,
    const ExpireWebHistoryCallback& callback) {
  DictionaryValue delete_request;
  scoped_ptr<ListValue> deletions(new ListValue);
  base::Time now = base::Time::Now();

  for (std::vector<ExpireHistoryArgs>::const_iterator it = expire_list.begin();
       it != expire_list.end(); ++it) {
    // Convert the times to server timestamps.
    std::string min_timestamp = ServerTimeString(it->begin_time);
    // TODO(dubroy): Use sane time (crbug.com/146090) here when it's available.
    base::Time end_time = it->end_time;
    if (end_time.is_null() || end_time > now)
      end_time = now;
    std::string max_timestamp = ServerTimeString(end_time);

    for (std::set<GURL>::const_iterator url_iterator = it->urls.begin();
         url_iterator != it->urls.end(); ++url_iterator) {
      deletions->Append(
          CreateDeletion(min_timestamp, max_timestamp, *url_iterator));
    }
    // If no URLs were specified, delete everything in the time range.
    if (it->urls.empty())
      deletions->Append(CreateDeletion(min_timestamp, max_timestamp, GURL()));
  }
  delete_request.Set("del", deletions.release());
  std::string post_data;
  base::JSONWriter::Write(&delete_request, &post_data);

  scoped_ptr<RequestImpl> request(
      new RequestImpl(profile_, kHistoryDeleteHistoryUrl, callback));
  request->set_post_data(post_data);
  request->Start();
  return request.PassAs<Request>();
}

scoped_ptr<WebHistoryService::Request> WebHistoryService::ExpireHistoryBetween(
    const std::set<GURL>& restrict_urls,
    base::Time begin_time,
    base::Time end_time,
    const ExpireWebHistoryCallback& callback) {
  std::vector<ExpireHistoryArgs> expire_list(1);
  expire_list.back().urls = restrict_urls;
  expire_list.back().begin_time = begin_time;
  expire_list.back().end_time = end_time;
  return ExpireHistory(expire_list, callback);
}

}  // namespace history
