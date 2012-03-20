// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_auth_service.h"

#include <string>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/chromeos/gdata/gdata_operations.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

using content::BrowserThread;

namespace gdata {

void GDataAuthService::Initialize(Profile* profile) {
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

GDataAuthService::GDataAuthService()
    : profile_(NULL),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      weak_ptr_bound_to_ui_thread_(weak_ptr_factory_.GetWeakPtr()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataAuthService::~GDataAuthService() {
}

void GDataAuthService::StartAuthentication(
    GDataOperationRegistry* registry,
    const AuthStatusCallback& callback) {
  scoped_refptr<base::MessageLoopProxy> relay_proxy(
      base::MessageLoopProxy::current());

  if (IsFullyAuthenticated()) {
    relay_proxy->PostTask(FROM_HERE,
         base::Bind(callback, gdata::HTTP_SUCCESS, oauth2_auth_token()));
  } else if (IsPartiallyAuthenticated()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataAuthService::StartAuthenticationOnUIThread,
                   weak_ptr_bound_to_ui_thread_,
                   registry,
                   relay_proxy,
                   base::Bind(&GDataAuthService::OnAuthCompleted,
                              weak_ptr_bound_to_ui_thread_,
                              relay_proxy,
                              callback)));
  } else {
    relay_proxy->PostTask(FROM_HERE,
        base::Bind(callback, gdata::HTTP_UNAUTHORIZED, std::string()));
  }
}

void GDataAuthService::StartAuthenticationOnUIThread(
    GDataOperationRegistry* registry,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // We have refresh token, let's gets authenticated.
  (new AuthOperation(registry, profile_,
                     callback, GetOAuth2RefreshToken()))->Start();
}

void GDataAuthService::OnAuthCompleted(
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const AuthStatusCallback& callback,
    GDataErrorCode error,
    const std::string& auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == HTTP_SUCCESS)
    auth_token_ = auth_token;

  // TODO(zelidrag): Add retry, back-off logic when things go wrong here.
  if (!callback.is_null())
    relay_proxy->PostTask(FROM_HERE, base::Bind(callback, error, auth_token));
}

void GDataAuthService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GDataAuthService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void GDataAuthService::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE ||
         type == chrome::NOTIFICATION_TOKEN_REQUEST_FAILED);

  TokenService::TokenAvailableDetails* token_details =
      content::Details<TokenService::TokenAvailableDetails>(details).ptr();
  if (token_details->service() != GaiaConstants::kGaiaOAuth2LoginRefreshToken)
    return;

  auth_token_.clear();
  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    TokenService* service = TokenServiceFactory::GetForProfile(profile_);
    refresh_token_ = service->GetOAuth2LoginRefreshToken();
  } else {
    refresh_token_.clear();
  }
  FOR_EACH_OBSERVER(Observer, observers_, OnOAuth2RefreshTokenChanged());
}

}  // namespace gdata
