// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_ios_promotion/sms_service.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

const char kDesktopIOSPromotionOAuthScope[] =
    "https://www.googleapis.com/auth/mobile_user_preferences";

const char kDesktopIOSPromotionQueryPhoneNumber[] =
    "https://growth-pa.googleapis.com/v1/get_verified_phone_numbers";

const char kDesktopIOSPromotionSendSMS[] =
    "https://growth-pa.googleapis.com/v1/send_sms";

const char kPostDataMimeType[] = "application/json";

const char kSendSMSPromoFormat[] = "{promo_id:%s}";

// The maximum number of retries for the URLFetcher requests.
const size_t kMaxRetries = 1;

class RequestImpl : public SMSService::Request,
                    private OAuth2TokenService::Consumer,
                    private net::URLFetcherDelegate {
 public:
  ~RequestImpl() override {}

  // Returns the response code received from the server, which will only be
  // valid if the request succeeded.
  int GetResponseCode() override { return response_code_; }

  // Returns the contents of the response body received from the server.
  const std::string& GetResponseBody() override { return response_body_; }

  bool IsPending() override { return is_pending_; }

 private:
  friend class ::SMSService;

  RequestImpl(
      OAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      const GURL& url,
      const SMSService::CompletionCallback& callback)
      : OAuth2TokenService::Consumer("desktop_ios_promotion"),
        token_service_(token_service),
        signin_manager_(signin_manager),
        request_context_(request_context),
        url_(url),
        post_data_mime_type_(kPostDataMimeType),
        response_code_(0),
        auth_retry_count_(0),
        callback_(callback),
        is_pending_(false) {
    DCHECK(token_service_);
    DCHECK(signin_manager_);
    DCHECK(request_context_);
  }

  void Start() override {
    OAuth2TokenService::ScopeSet oauth_scopes;
    oauth_scopes.insert(kDesktopIOSPromotionOAuthScope);
    token_request_ = token_service_->StartRequest(
        signin_manager_->GetAuthenticatedAccountId(), oauth_scopes, this);
    is_pending_ = true;
  }

  // content::URLFetcherDelegate interface.
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    DCHECK_EQ(source, url_fetcher_.get());
    response_code_ = url_fetcher_->GetResponseCode();

    UMA_HISTOGRAM_CUSTOM_ENUMERATION(
        "DesktopIOSPromotion.OAuthTokenResponseCode",
        net::HttpUtil::MapStatusCodeForHistogram(response_code_),
        net::HttpUtil::GetStatusCodesForHistogram());

    // If the response code indicates that the token might not be valid,
    // invalidate the token and try again.
    if (response_code_ == net::HTTP_UNAUTHORIZED && ++auth_retry_count_ <= 1) {
      OAuth2TokenService::ScopeSet oauth_scopes;
      oauth_scopes.insert(kDesktopIOSPromotionOAuthScope);
      token_service_->InvalidateAccessToken(
          signin_manager_->GetAuthenticatedAccountId(), oauth_scopes,
          access_token_);

      access_token_.clear();
      Start();
      return;
    }
    url_fetcher_->GetResponseAsString(&response_body_);
    url_fetcher_.reset();
    is_pending_ = false;
    callback_.Run(this, response_code_ == net::HTTP_OK);
    // It is valid for the callback to delete |this|, so do not access any
    // members below here.
  }

  // OAuth2TokenService::Consumer interface.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override {
    token_request_.reset();
    DCHECK(!access_token.empty());
    access_token_ = access_token;

    UMA_HISTOGRAM_BOOLEAN("DesktopIOSPromotion.OAuthTokenCompletion", true);

    // Got an access token -- start the actual API request.
    url_fetcher_ = CreateUrlFetcher(access_token);
    url_fetcher_->Start();
  }

  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override {
    token_request_.reset();
    is_pending_ = false;

    UMA_HISTOGRAM_BOOLEAN("DesktopIOSPromotion.OAuthTokenCompletion", false);

    callback_.Run(this, false);
    // It is valid for the callback to delete |this|, so do not access any
    // members below here.
  }

  // Helper for creating a new URLFetcher for the API request.
  std::unique_ptr<net::URLFetcher> CreateUrlFetcher(
      const std::string& access_token) {
    net::URLFetcher::RequestType request_type =
        post_data_ ? net::URLFetcher::POST : net::URLFetcher::GET;
    std::unique_ptr<net::URLFetcher> fetcher =
        net::URLFetcher::Create(url_, request_type, this);
    fetcher->SetRequestContext(request_context_.get());
    fetcher->SetMaxRetriesOn5xx(kMaxRetries);
    fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                          net::LOAD_DO_NOT_SAVE_COOKIES);
    fetcher->AddExtraRequestHeader("Authorization: Bearer " + access_token);
    fetcher->AddExtraRequestHeader(
        "X-Developer-Key: " +
        GaiaUrls::GetInstance()->oauth2_chrome_client_id());

    if (post_data_)
      fetcher->SetUploadData(post_data_mime_type_, post_data_.value());
    return fetcher;
  }

  void SetPostData(const std::string& post_data) override {
    SetPostDataAndType(post_data, kPostDataMimeType);
  }

  void SetPostDataAndType(const std::string& post_data,
                          const std::string& mime_type) override {
    post_data_ = post_data;
    post_data_mime_type_ = mime_type;
  }

  OAuth2TokenService* token_service_;
  SigninManagerBase* signin_manager_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // The URL of the API endpoint.
  GURL url_;

  // POST data to be sent with the request (may be empty).
  base::Optional<std::string> post_data_;

  // MIME type of the post requests. Defaults to text/plain.
  std::string post_data_mime_type_;

  // The OAuth2 access token request.
  std::unique_ptr<OAuth2TokenService::Request> token_request_;

  // The current OAuth2 access token.
  std::string access_token_;

  // Handles the actual API requests after the OAuth token is acquired.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // Holds the response code received from the server.
  int response_code_;

  // Holds the response body received from the server.
  std::string response_body_;

  // The number of times this request has already been retried due to
  // authorization problems.
  int auth_retry_count_;

  // The callback to execute when the query is complete.
  SMSService::CompletionCallback callback_;

  // True if the request was started and has not yet completed, otherwise false.
  bool is_pending_;
};

}  // namespace

SMSService::Request::Request() {}

SMSService::Request::~Request() {}

SMSService::SMSService(
    OAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : token_service_(token_service),
      signin_manager_(signin_manager),
      request_context_(request_context),
      weak_ptr_factory_(this) {}

SMSService::~SMSService() {}

SMSService::Request* SMSService::CreateRequest(
    const GURL& url,
    const CompletionCallback& callback) {
  return new RequestImpl(token_service_, signin_manager_, request_context_, url,
                         callback);
}

void SMSService::QueryPhoneNumber(const PhoneNumberCallback& callback) {
  CompletionCallback completion_callback =
      base::Bind(&SMSService::QueryPhoneNumberCompletionCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);

  GURL url(kDesktopIOSPromotionQueryPhoneNumber);
  Request* request = CreateRequest(url, completion_callback);
  // This placeholder is required by the API.
  request->SetPostData("{}");
  pending_requests_[request] = base::WrapUnique(request);
  request->Start();
}

void SMSService::SendSMS(const std::string& promo_id,
                         const SMSService::PhoneNumberCallback& callback) {
  CompletionCallback completion_callback = base::Bind(
      &SMSService::SendSMSCallback, weak_ptr_factory_.GetWeakPtr(), callback);
  GURL url(kDesktopIOSPromotionSendSMS);
  Request* request = CreateRequest(url, completion_callback);
  request->SetPostData(
      base::StringPrintf(kSendSMSPromoFormat, promo_id.c_str()));
  pending_requests_[request] = base::WrapUnique(request);
  request->Start();
}

void SMSService::QueryPhoneNumberCompletionCallback(
    const SMSService::PhoneNumberCallback& callback,
    SMSService::Request* request,
    bool success) {
  std::unique_ptr<Request> request_ptr = std::move(pending_requests_[request]);
  pending_requests_.erase(request);

  std::string phone_number;
  bool has_number = false;
  std::unique_ptr<base::Value> value =
      base::JSONReader::Read(request->GetResponseBody());
  if (value.get() && value.get()->IsType(base::Value::Type::DICTIONARY)) {
    const base::DictionaryValue* dictionary;
    if (value->GetAsDictionary(&dictionary)) {
      const base::ListValue* number_list;
      if (dictionary->GetList("phoneNumber", &number_list)) {
        const base::DictionaryValue* sub_dictionary;
        // For now only handle the first number.
        if (number_list->GetSize() > 0 &&
            number_list->GetDictionary(0, &sub_dictionary)) {
          if (sub_dictionary->GetString("phoneNumber", &phone_number))
            has_number = true;
        }
      }
    }
  }
  callback.Run(request, success && has_number, phone_number);
}

void SMSService::SendSMSCallback(
    const SMSService::PhoneNumberCallback& callback,
    SMSService::Request* request,
    bool success) {
  std::unique_ptr<Request> request_ptr = std::move(pending_requests_[request]);
  pending_requests_.erase(request);

  std::string phone_number;
  bool has_number = false;
  std::unique_ptr<base::Value> value =
      base::JSONReader::Read(request->GetResponseBody());
  if (value.get() && value.get()->IsType(base::Value::Type::DICTIONARY)) {
    const base::DictionaryValue* dictionary;
    if (value->GetAsDictionary(&dictionary)) {
      if (dictionary->GetString("phoneNumber", &phone_number))
        has_number = true;
    }
  }
  callback.Run(request, success && has_number, phone_number);
}
