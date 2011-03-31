// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/gaia_oauth_client.h"

#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/net/http_return.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_context_getter.h"

namespace {
const char kAccessTokenValue[] = "access_token";
const char kRefreshTokenValue[] = "refresh_token";
const char kExpiresInValue[] = "expires_in";
}

namespace gaia {

class GaiaOAuthClient::Core
    : public base::RefCountedThreadSafe<GaiaOAuthClient::Core>,
      public URLFetcher::Delegate {
 public:
  Core(const std::string& gaia_url,
       net::URLRequestContextGetter* request_context_getter)
           : gaia_url_(gaia_url),
             num_retries_(0),
             request_context_getter_(request_context_getter),
             delegate_(NULL) {}

  virtual ~Core() { }

  void GetTokensFromAuthCode(const OAuthClientInfo& oauth_client_info,
                             const std::string& auth_code,
                             int max_retries,
                             GaiaOAuthClient::Delegate* delegate);
  void RefreshToken(const OAuthClientInfo& oauth_client_info,
                    const std::string& refresh_token,
                    int max_retries,
                    GaiaOAuthClient::Delegate* delegate);

  // URLFetcher::Delegate implementation.
  virtual void OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data);

 private:
  void MakeGaiaRequest(std::string post_body,
                       int max_retries,
                       GaiaOAuthClient::Delegate* delegate);
  void HandleResponse(const URLFetcher* source,
                      const GURL& url,
                      const net::URLRequestStatus& status,
                      int response_code,
                      const std::string& data,
                      bool* should_retry_request);

  GURL gaia_url_;
  int num_retries_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  GaiaOAuthClient::Delegate* delegate_;
  scoped_ptr<URLFetcher> request_;
};

void GaiaOAuthClient::Core::GetTokensFromAuthCode(
    const OAuthClientInfo& oauth_client_info,
    const std::string& auth_code,
    int max_retries,
    GaiaOAuthClient::Delegate* delegate) {
  std::string post_body =
      "code=" + EscapeUrlEncodedData(auth_code) +
      "&client_id=" + EscapeUrlEncodedData(oauth_client_info.client_id) +
      "&client_secret=" +
      EscapeUrlEncodedData(oauth_client_info.client_secret) +
      "&redirect_uri=oob&grant_type=authorization_code";
  MakeGaiaRequest(post_body, max_retries, delegate);
}

void GaiaOAuthClient::Core::RefreshToken(
    const OAuthClientInfo& oauth_client_info,
    const std::string& refresh_token,
    int max_retries,
    GaiaOAuthClient::Delegate* delegate) {
  std::string post_body =
      "refresh_token=" + EscapeUrlEncodedData(refresh_token) +
      "&client_id=" + EscapeUrlEncodedData(oauth_client_info.client_id) +
      "&client_secret=" +
      EscapeUrlEncodedData(oauth_client_info.client_secret) +
      "&grant_type=refresh_token";
  MakeGaiaRequest(post_body, max_retries, delegate);
}

void GaiaOAuthClient::Core::MakeGaiaRequest(
    std::string post_body,
    int max_retries,
    GaiaOAuthClient::Delegate* delegate) {
  DCHECK(!request_.get()) << "Tried to fetch two things at once!";
  delegate_ = delegate;
  num_retries_ = 0;
  request_.reset(URLFetcher::Create(0, gaia_url_, URLFetcher::POST, this));
  request_->set_request_context(request_context_getter_);
  request_->set_upload_data("application/x-www-form-urlencoded", post_body);
  request_->set_max_retries(max_retries);
  request_->Start();
}

// URLFetcher::Delegate implementation.
void GaiaOAuthClient::Core::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  bool should_retry = false;
  HandleResponse(source, url, status, response_code, data, &should_retry);
  if (should_retry) {
    // If the response code is greater than or equal to 500, then the back-off
    // period has been increased at the network level; otherwise, explicitly
    // call ReceivedContentWasMalformed() to count the current request as a
    // failure and increase the back-off period.
    if (response_code < 500)
      request_->ReceivedContentWasMalformed();
    num_retries_++;
    request_->Start();
  } else {
    request_.reset();
  }
}

void GaiaOAuthClient::Core::HandleResponse(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const std::string& data,
    bool* should_retry_request) {
  *should_retry_request = false;
  // RC_BAD_REQUEST means the arguments are invalid. No point retrying. We are
  // done here.
  if (response_code == RC_BAD_REQUEST) {
    delegate_->OnOAuthError();
    return;
  }
  std::string access_token;
  std::string refresh_token;
  int expires_in_seconds = 0;
  if (response_code == RC_REQUEST_OK) {
    scoped_ptr<Value> message_value(
        base::JSONReader::Read(data, false));
    if (message_value.get() &&
        message_value->IsType(Value::TYPE_DICTIONARY)) {
      scoped_ptr<DictionaryValue> response_dict(
          static_cast<DictionaryValue*>(message_value.release()));
      response_dict->GetString(kAccessTokenValue, &access_token);
      response_dict->GetString(kRefreshTokenValue, &refresh_token);
      response_dict->GetInteger(kExpiresInValue, &expires_in_seconds);
    }
  }
  if (access_token.empty()) {
    // If we don't have an access token yet and the the error was not
    // RC_BAD_REQUEST, we may need to retry.
    if ((-1 != source->max_retries()) &&
        (num_retries_ > source->max_retries())) {
      // Retry limit reached. Give up.
      delegate_->OnNetworkError(response_code);
    } else {
      *should_retry_request = true;
    }
  } else if (refresh_token.empty()) {
    // If we only have an access token, then this was a refresh request.
    delegate_->OnRefreshTokenResponse(access_token, expires_in_seconds);
  } else {
    delegate_->OnGetTokensResponse(refresh_token,
                                   access_token,
                                   expires_in_seconds);
  }
}

GaiaOAuthClient::GaiaOAuthClient(const std::string& gaia_url,
                                 net::URLRequestContextGetter* context_getter) {
  core_ = new Core(gaia_url, context_getter);
}

GaiaOAuthClient::~GaiaOAuthClient() {
}


void GaiaOAuthClient::GetTokensFromAuthCode(
    const OAuthClientInfo& oauth_client_info,
    const std::string& auth_code,
    int max_retries,
    Delegate* delegate) {
  return core_->GetTokensFromAuthCode(oauth_client_info,
                                      auth_code,
                                      max_retries,
                                      delegate);
}

void GaiaOAuthClient::RefreshToken(const OAuthClientInfo& oauth_client_info,
                                   const std::string& refresh_token,
                                   int max_retries,
                                   Delegate* delegate) {
  return core_->RefreshToken(oauth_client_info,
                             refresh_token,
                             max_retries,
                             delegate);
}

}  // namespace gaia
