// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/gaia_oauth_client.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context_getter.h"

namespace {
const char kAccessTokenValue[] = "access_token";
const char kRefreshTokenValue[] = "refresh_token";
const char kExpiresInValue[] = "expires_in";
}

namespace gaia {

class GaiaOAuthClient::Core
    : public base::RefCountedThreadSafe<GaiaOAuthClient::Core>,
      public content::URLFetcherDelegate {
 public:
  Core(const std::string& gaia_url,
       net::URLRequestContextGetter* request_context_getter)
           : gaia_url_(gaia_url),
             num_retries_(0),
             request_context_getter_(request_context_getter),
             delegate_(NULL) {}

  void GetTokensFromAuthCode(const OAuthClientInfo& oauth_client_info,
                             const std::string& auth_code,
                             int max_retries,
                             GaiaOAuthClient::Delegate* delegate);
  void RefreshToken(const OAuthClientInfo& oauth_client_info,
                    const std::string& refresh_token,
                    int max_retries,
                    GaiaOAuthClient::Delegate* delegate);

  // content::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source);

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core() {}

  void MakeGaiaRequest(std::string post_body,
                       int max_retries,
                       GaiaOAuthClient::Delegate* delegate);
  void HandleResponse(const net::URLFetcher* source,
                      bool* should_retry_request);

  GURL gaia_url_;
  int num_retries_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  GaiaOAuthClient::Delegate* delegate_;
  scoped_ptr<content::URLFetcher> request_;
};

void GaiaOAuthClient::Core::GetTokensFromAuthCode(
    const OAuthClientInfo& oauth_client_info,
    const std::string& auth_code,
    int max_retries,
    GaiaOAuthClient::Delegate* delegate) {
  std::string post_body =
      "code=" + net::EscapeUrlEncodedData(auth_code, true) +
      "&client_id=" + net::EscapeUrlEncodedData(oauth_client_info.client_id,
                                                true) +
      "&client_secret=" +
      net::EscapeUrlEncodedData(oauth_client_info.client_secret, true) +
      "&redirect_uri=oob&grant_type=authorization_code";
  MakeGaiaRequest(post_body, max_retries, delegate);
}

void GaiaOAuthClient::Core::RefreshToken(
    const OAuthClientInfo& oauth_client_info,
    const std::string& refresh_token,
    int max_retries,
    GaiaOAuthClient::Delegate* delegate) {
  std::string post_body =
      "refresh_token=" + net::EscapeUrlEncodedData(refresh_token, true) +
      "&client_id=" + net::EscapeUrlEncodedData(oauth_client_info.client_id,
                                                true) +
      "&client_secret=" +
      net::EscapeUrlEncodedData(oauth_client_info.client_secret, true) +
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
  request_.reset(content::URLFetcher::Create(
      0, gaia_url_, content::URLFetcher::POST, this));
  request_->SetRequestContext(request_context_getter_);
  request_->SetUploadData("application/x-www-form-urlencoded", post_body);
  request_->SetMaxRetries(max_retries);
  request_->Start();
}

// URLFetcher::Delegate implementation.
void GaiaOAuthClient::Core::OnURLFetchComplete(
    const net::URLFetcher* source) {
  bool should_retry = false;
  HandleResponse(source, &should_retry);
  if (should_retry) {
    // Explicitly call ReceivedContentWasMalformed() to ensure the current
    // request gets counted as a failure for calculation of the back-off
    // period.  If it was already a failure by status code, this call will
    // be ignored.
    request_->ReceivedContentWasMalformed();
    num_retries_++;
    // We must set our request_context_getter_ again because
    // URLFetcher::Core::RetryOrCompleteUrlFetch resets it to NULL...
    request_->SetRequestContext(request_context_getter_);
    request_->Start();
  } else {
    request_.reset();
  }
}

void GaiaOAuthClient::Core::HandleResponse(
    const net::URLFetcher* source,
    bool* should_retry_request) {
  *should_retry_request = false;
  // RC_BAD_REQUEST means the arguments are invalid. No point retrying. We are
  // done here.
  if (source->GetResponseCode() == net::HTTP_BAD_REQUEST) {
    delegate_->OnOAuthError();
    return;
  }
  std::string access_token;
  std::string refresh_token;
  int expires_in_seconds = 0;
  if (source->GetResponseCode() == net::HTTP_OK) {
    std::string data;
    source->GetResponseAsString(&data);
    scoped_ptr<Value> message_value(base::JSONReader::Read(data));
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
    if ((-1 != source->GetMaxRetries()) &&
        (num_retries_ > source->GetMaxRetries())) {
      // Retry limit reached. Give up.
      delegate_->OnNetworkError(source->GetResponseCode());
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
