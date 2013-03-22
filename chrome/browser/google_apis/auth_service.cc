// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/auth_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/google_apis/auth_service_observer.h"
#include "chrome/browser/google_apis/operation_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif  // OS_CHROMEOS

using content::BrowserThread;

namespace google_apis {

namespace {

// Used for success ratio histograms. 0 for failure, 1 for success,
// 2 for no connection (likely offline).
const int kSuccessRatioHistogramFailure = 0;
const int kSuccessRatioHistogramSuccess = 1;
const int kSuccessRatioHistogramNoConnection = 2;
const int kSuccessRatioHistogramTemporaryFailure = 3;
const int kSuccessRatioHistogramMaxValue = 4;  // The max value is exclusive.

}  // namespace

// OAuth2 authorization token retrieval operation.
class AuthOperation : public OperationRegistry::Operation,
                      public OAuth2AccessTokenConsumer {
 public:
  AuthOperation(OperationRegistry* registry,
                net::URLRequestContextGetter* url_request_context_getter,
                const AuthStatusCallback& callback,
                const std::vector<std::string>& scopes,
                const std::string& refresh_token);
  virtual ~AuthOperation();
  void Start();

  // Overridden from OAuth2AccessTokenConsumer:
  virtual void OnGetTokenSuccess(const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // Overridden from OperationRegistry::Operation
  virtual void DoCancel() OVERRIDE;

 private:
  net::URLRequestContextGetter* url_request_context_getter_;
  std::string refresh_token_;
  AuthStatusCallback callback_;
  std::vector<std::string> scopes_;
  scoped_ptr<OAuth2AccessTokenFetcher> oauth2_access_token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AuthOperation);
};

AuthOperation::AuthOperation(
    OperationRegistry* registry,
    net::URLRequestContextGetter* url_request_context_getter,
    const AuthStatusCallback& callback,
    const std::vector<std::string>& scopes,
    const std::string& refresh_token)
    : OperationRegistry::Operation(registry),
      url_request_context_getter_(url_request_context_getter),
      refresh_token_(refresh_token),
      callback_(callback),
      scopes_(scopes) {
  DCHECK(!callback_.is_null());
}

AuthOperation::~AuthOperation() {}

void AuthOperation::Start() {
  DCHECK(!refresh_token_.empty());
  oauth2_access_token_fetcher_.reset(new OAuth2AccessTokenFetcher(
      this, url_request_context_getter_));
  NotifyStart();
  oauth2_access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      refresh_token_,
      scopes_);
}

void AuthOperation::DoCancel() {
  oauth2_access_token_fetcher_->CancelRequest();
  callback_.Run(GDATA_CANCELLED, std::string());
}

// Callback for OAuth2AccessTokenFetcher on success. |access_token| is the token
// used to start fetching user data.
void AuthOperation::OnGetTokenSuccess(const std::string& access_token,
                                      const base::Time& expiration_time) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UMA_HISTOGRAM_ENUMERATION("GData.AuthSuccess",
                            kSuccessRatioHistogramSuccess,
                            kSuccessRatioHistogramMaxValue);

  callback_.Run(HTTP_SUCCESS, access_token);
  NotifyFinish(OPERATION_COMPLETED);
}

// Callback for OAuth2AccessTokenFetcher on failure.
void AuthOperation::OnGetTokenFailure(const GoogleServiceAuthError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  LOG(WARNING) << "AuthOperation: token request using refresh token failed: "
               << error.ToString();

  // There are many ways to fail, but if the failure is due to connection,
  // it's likely that the device is off-line. We treat the error differently
  // so that the file manager works while off-line.
  if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED) {
    UMA_HISTOGRAM_ENUMERATION("GData.AuthSuccess",
                              kSuccessRatioHistogramNoConnection,
                              kSuccessRatioHistogramMaxValue);
    callback_.Run(GDATA_NO_CONNECTION, std::string());
  } else if (error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE) {
    // Temporary auth error.
    UMA_HISTOGRAM_ENUMERATION("GData.AuthSuccess",
                              kSuccessRatioHistogramTemporaryFailure,
                              kSuccessRatioHistogramMaxValue);
    callback_.Run(HTTP_FORBIDDEN, std::string());
  } else {
    // Permanent auth error.
    UMA_HISTOGRAM_ENUMERATION("GData.AuthSuccess",
                              kSuccessRatioHistogramFailure,
                              kSuccessRatioHistogramMaxValue);
    callback_.Run(HTTP_UNAUTHORIZED, std::string());
  }
  NotifyFinish(OPERATION_FAILED);
}

void AuthService::Initialize(Profile* profile) {
  profile_ = profile;
  // Get OAuth2 refresh token (if we have any) and register for its updates.
  TokenService* service = TokenServiceFactory::GetForProfile(profile_);
  refresh_token_ = service->GetOAuth2LoginRefreshToken();
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 content::Source<TokenService>(service));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                 content::Source<TokenService>(service));

  if (!refresh_token_.empty())
    FOR_EACH_OBSERVER(AuthServiceObserver,
                      observers_,
                      OnOAuth2RefreshTokenChanged());
}

AuthService::AuthService(
    net::URLRequestContextGetter* url_request_context_getter,
    const std::vector<std::string>& scopes)
    : profile_(NULL),
      url_request_context_getter_(url_request_context_getter),
      scopes_(scopes),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

AuthService::~AuthService() {
}

void AuthService::StartAuthentication(OperationRegistry* registry,
                                      const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_refptr<base::MessageLoopProxy> relay_proxy(
      base::MessageLoopProxy::current());

  if (HasAccessToken()) {
    // We already have access token. Give it back to the caller asynchronously.
    relay_proxy->PostTask(FROM_HERE,
         base::Bind(callback, HTTP_SUCCESS, access_token_));
  } else if (HasRefreshToken()) {
    // We have refresh token, let's get an access token.
    (new AuthOperation(registry,
                       url_request_context_getter_,
                       base::Bind(&AuthService::OnAuthCompleted,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback),
                       scopes_,
                       refresh_token_))->Start();
  } else {
    relay_proxy->PostTask(FROM_HERE,
        base::Bind(callback, GDATA_NOT_READY, std::string()));
  }
}

bool AuthService::HasAccessToken() const {
  return !access_token_.empty();
}

bool AuthService::HasRefreshToken() const {
  return !refresh_token_.empty();
}

const std::string& AuthService::access_token() const {
  return access_token_;
}

void AuthService::ClearAccessToken() {
  access_token_.clear();
}

void AuthService::ClearRefreshToken() {
  refresh_token_.clear();

  FOR_EACH_OBSERVER(AuthServiceObserver,
                    observers_,
                    OnOAuth2RefreshTokenChanged());
}

void AuthService::OnAuthCompleted(const AuthStatusCallback& callback,
                                  GDataErrorCode error,
                                  const std::string& access_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == HTTP_SUCCESS) {
    access_token_ = access_token;
  } else if (error == HTTP_UNAUTHORIZED) {
    // Refreshing access token using the refresh token is failed with 401 error
    // (HTTP_UNAUTHORIZED). This means the current refresh token is invalid for
    // Drive, hence we clear the refresh token here to make HasRefreshToken()
    // false, thus the invalidness is clearly observable.
    // This is not for triggering refetch of the refresh token. UI should
    // show some message to encourage user to log-off and log-in again in order
    // to fetch new valid refresh token.
    ClearRefreshToken();
  }

  // TODO(zelidrag): Add retry, back-off logic when things go wrong here.
  callback.Run(error, access_token);
}

void AuthService::AddObserver(AuthServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void AuthService::RemoveObserver(AuthServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AuthService::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE ||
         type == chrome::NOTIFICATION_TOKEN_REQUEST_FAILED);

  TokenService::TokenAvailableDetails* token_details =
      content::Details<TokenService::TokenAvailableDetails>(details).ptr();
  if (token_details->service() != GaiaConstants::kGaiaOAuth2LoginRefreshToken)
    return;

  access_token_.clear();
  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    TokenService* service = TokenServiceFactory::GetForProfile(profile_);
    refresh_token_ = service->GetOAuth2LoginRefreshToken();
  } else {
    refresh_token_.clear();
  }
  FOR_EACH_OBSERVER(AuthServiceObserver,
                    observers_,
                    OnOAuth2RefreshTokenChanged());
}

// static
bool AuthService::CanAuthenticate(Profile* profile) {
#if defined(OS_CHROMEOS)
  if (!chromeos::UserManager::Get()->IsUserLoggedIn() ||
      chromeos::UserManager::Get()->IsLoggedInAsGuest() ||
      chromeos::UserManager::Get()->IsLoggedInAsDemoUser())
    return false;
#endif  // OS_CHROMEOS

  // Authentication cannot be done with the incognito mode profile.
  if (profile->IsOffTheRecord())
    return false;

  return true;
}

}  // namespace google_apis
