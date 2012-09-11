// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/auth_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/google_apis/operations_base.h"
#include "chrome/browser/google_apis/task_util.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "google_apis/gaia/gaia_constants.h"

using content::BrowserThread;

namespace gdata {

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
    FOR_EACH_OBSERVER(Observer, observers_, OnOAuth2RefreshTokenChanged());
}

AuthService::AuthService(const std::vector<std::string>& scopes)
    : profile_(NULL),
      scopes_(scopes),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

AuthService::~AuthService() {
}

void AuthService::StartAuthentication(OperationRegistry* registry,
                                      const AuthStatusCallback& callback) {
  scoped_refptr<base::MessageLoopProxy> relay_proxy(
      base::MessageLoopProxy::current());

  if (HasAccessToken()) {
    relay_proxy->PostTask(FROM_HERE,
         base::Bind(callback, gdata::HTTP_SUCCESS, access_token_));
  } else if (HasRefreshToken()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&AuthService::StartAuthenticationOnUIThread,
                   weak_ptr_factory_.GetWeakPtr(),
                   registry,
                   CreateRelayCallback(
                       base::Bind(&AuthService::OnAuthCompleted,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback))));
  } else {
    relay_proxy->PostTask(FROM_HERE,
        base::Bind(callback, gdata::GDATA_NOT_READY, std::string()));
  }
}

void AuthService::StartAuthenticationOnUIThread(
    OperationRegistry* registry,
    const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // We have refresh token, let's gets authenticated.
  (new AuthOperation(registry, callback, scopes_, refresh_token_))->Start();
}

void AuthService::OnAuthCompleted(const AuthStatusCallback& callback,
                                  GDataErrorCode error,
                                  const std::string& access_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == HTTP_SUCCESS)
    access_token_ = access_token;

  // TODO(zelidrag): Add retry, back-off logic when things go wrong here.
  callback.Run(error, access_token);
}

void AuthService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AuthService::RemoveObserver(Observer* observer) {
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
  FOR_EACH_OBSERVER(Observer, observers_, OnOAuth2RefreshTokenChanged());
}

}  // namespace gdata
