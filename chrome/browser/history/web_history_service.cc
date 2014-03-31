// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/web_history_service.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

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

  virtual bool is_pending() OVERRIDE { return is_pending_; }

 private:
  friend class history::WebHistoryService;

  typedef base::Callback<void(Request*, bool)> CompletionCallback;

  RequestImpl(Profile* profile,
              const GURL& url,
              const CompletionCallback& callback)
      : OAuth2TokenService::Consumer("web_history"),
        profile_(profile),
        url_(url),
        response_code_(0),
        auth_retry_count_(0),
        callback_(callback),
        is_pending_(false) {
  }

  // Tells the request to do its thang.
  void Start() {
    OAuth2TokenService::ScopeSet oauth_scopes;
    oauth_scopes.insert(kHistoryOAuthScope);

    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
    SigninManagerBase* signin_manager =
        SigninManagerFactory::GetForProfile(profile_);
    token_request_ = token_service->StartRequest(
        signin_manager->GetAuthenticatedAccountId(), oauth_scopes, this);
    is_pending_ = true;
  }

  // content::URLFetcherDelegate interface.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    DCHECK_EQ(source, url_fetcher_.get());
    response_code_ = url_fetcher_->GetResponseCode();

    UMA_HISTOGRAM_CUSTOM_ENUMERATION("WebHistory.OAuthTokenResponseCode",
        net::HttpUtil::MapStatusCodeForHistogram(response_code_),
        net::HttpUtil::GetStatusCodesForHistogram());

    // If the response code indicates that the token might not be valid,
    // invalidate the token and try again.
    if (response_code_ == net::HTTP_UNAUTHORIZED && ++auth_retry_count_ <= 1) {
      OAuth2TokenService::ScopeSet oauth_scopes;
      oauth_scopes.insert(kHistoryOAuthScope);
      ProfileOAuth2TokenService* token_service =
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
      SigninManagerBase* signin_manager =
          SigninManagerFactory::GetForProfile(profile_);
      token_service->InvalidateToken(
          signin_manager->GetAuthenticatedAccountId(),
          oauth_scopes,
          access_token_);

      access_token_.clear();
      Start();
      return;
    }
    url_fetcher_->GetResponseAsString(&response_body_);
    url_fetcher_.reset();
    is_pending_ = false;
    callback_.Run(this, true);
    // It is valid for the callback to delete |this|, so do not access any
    // members below here.
  }

  // OAuth2TokenService::Consumer interface.
  virtual void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const std::string& access_token,
      const base::Time& expiration_time) OVERRIDE {
    token_request_.reset();
    DCHECK(!access_token.empty());
    access_token_ = access_token;

    UMA_HISTOGRAM_BOOLEAN("WebHistory.OAuthTokenCompletion", true);

    // Got an access token -- start the actual API request.
    url_fetcher_.reset(CreateUrlFetcher(access_token));
    url_fetcher_->Start();
  }

  virtual void OnGetTokenFailure(
      const OAuth2TokenService::Request* request,
      const GoogleServiceAuthError& error) OVERRIDE {
    token_request_.reset();
    is_pending_ = false;

    UMA_HISTOGRAM_BOOLEAN("WebHistory.OAuthTokenCompletion", false);

    callback_.Run(this, false);
    // It is valid for the callback to delete |this|, so do not access any
    // members below here.
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

  // True if the request was started and has not yet completed, otherwise false.
  bool is_pending_;
};

// Extracts a JSON-encoded HTTP response into a DictionaryValue.
// If |request|'s HTTP response code indicates failure, or if the response
// body is not JSON, a null pointer is returned.
scoped_ptr<base::DictionaryValue> ReadResponse(RequestImpl* request) {
  scoped_ptr<base::DictionaryValue> result;
  if (request->response_code() == net::HTTP_OK) {
    base::Value* value = base::JSONReader::Read(request->response_body());
    if (value && value->IsType(base::Value::TYPE_DICTIONARY))
      result.reset(static_cast<base::DictionaryValue*>(value));
    else
      DLOG(WARNING) << "Non-JSON response received from history server.";
  }
  return result.Pass();
}

// Converts a time into a string for use as a parameter in a request to the
// history server.
std::string ServerTimeString(base::Time time) {
  if (time < base::Time::UnixEpoch()) {
    return base::Int64ToString(0);
  } else {
    return base::Int64ToString(
        (time - base::Time::UnixEpoch()).InMicroseconds());
  }
}

// Returns a URL for querying the history server for a query specified by
// |options|. |version_info|, if not empty, should be a token that was received
// from the server in response to a write operation. It is used to help ensure
// read consistency after a write.
GURL GetQueryUrl(const base::string16& text_query,
                 const QueryOptions& options,
                 const std::string& version_info) {
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
    url = net::AppendQueryParameter(url, "q", base::UTF16ToUTF8(text_query));

  if (!version_info.empty())
    url = net::AppendQueryParameter(url, "kvi", version_info);

  return url;
}

// Creates a DictionaryValue to hold the parameters for a deletion.
// Ownership is passed to the caller.
// |url| may be empty, indicating a time-range deletion.
base::DictionaryValue* CreateDeletion(
    const std::string& min_time,
    const std::string& max_time,
    const GURL& url) {
  base::DictionaryValue* deletion = new base::DictionaryValue;
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
    : profile_(profile),
      weak_ptr_factory_(this) {
}

WebHistoryService::~WebHistoryService() {
  STLDeleteElements(&pending_expire_requests_);
}

scoped_ptr<WebHistoryService::Request> WebHistoryService::QueryHistory(
    const base::string16& text_query,
    const QueryOptions& options,
    const WebHistoryService::QueryWebHistoryCallback& callback) {
  // Wrap the original callback into a generic completion callback.
  RequestImpl::CompletionCallback completion_callback = base::Bind(
      &WebHistoryService::QueryHistoryCompletionCallback, callback);

  GURL url = GetQueryUrl(text_query, options, server_version_info_);
  scoped_ptr<RequestImpl> request(
      new RequestImpl(profile_, url, completion_callback));
  request->Start();
  return request.PassAs<Request>();
}

void WebHistoryService::ExpireHistory(
    const std::vector<ExpireHistoryArgs>& expire_list,
    const ExpireWebHistoryCallback& callback) {
  base::DictionaryValue delete_request;
  scoped_ptr<base::ListValue> deletions(new base::ListValue);
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

  GURL url(kHistoryDeleteHistoryUrl);

  // Append the version info token, if it is available, to help ensure
  // consistency with any previous deletions.
  if (!server_version_info_.empty())
    url = net::AppendQueryParameter(url, "kvi", server_version_info_);

  // Wrap the original callback into a generic completion callback.
  RequestImpl::CompletionCallback completion_callback =
      base::Bind(&WebHistoryService::ExpireHistoryCompletionCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback);

  scoped_ptr<RequestImpl> request(
      new RequestImpl(profile_, url, completion_callback));
  request->set_post_data(post_data);
  request->Start();
  pending_expire_requests_.insert(request.release());
}

void WebHistoryService::ExpireHistoryBetween(
    const std::set<GURL>& restrict_urls,
    base::Time begin_time,
    base::Time end_time,
    const ExpireWebHistoryCallback& callback) {
  std::vector<ExpireHistoryArgs> expire_list(1);
  expire_list.back().urls = restrict_urls;
  expire_list.back().begin_time = begin_time;
  expire_list.back().end_time = end_time;
  ExpireHistory(expire_list, callback);
}

// static
void WebHistoryService::QueryHistoryCompletionCallback(
    const WebHistoryService::QueryWebHistoryCallback& callback,
    WebHistoryService::Request* request,
    bool success) {
  scoped_ptr<base::DictionaryValue> response_value;
  if (success)
    response_value = ReadResponse(static_cast<RequestImpl*>(request));
  callback.Run(request, response_value.get());
}

void WebHistoryService::ExpireHistoryCompletionCallback(
    const WebHistoryService::ExpireWebHistoryCallback& callback,
    WebHistoryService::Request* request,
    bool success) {
  scoped_ptr<base::DictionaryValue> response_value;
  if (success) {
    response_value = ReadResponse(static_cast<RequestImpl*>(request));
    if (response_value)
      response_value->GetString("version_info", &server_version_info_);
  }
  callback.Run(response_value.get() && success);
  // Clean up from pending requests.
  pending_expire_requests_.erase(request);
  delete request;
}

}  // namespace history
