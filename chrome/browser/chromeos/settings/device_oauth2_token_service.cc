// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace chromeos {

struct DeviceOAuth2TokenService::PendingRequest {
  PendingRequest(const base::WeakPtr<RequestImpl>& request,
                 const std::string& client_id,
                 const std::string& client_secret,
                 const ScopeSet& scopes)
      : request(request),
        client_id(client_id),
        client_secret(client_secret),
        scopes(scopes) {}

  const base::WeakPtr<RequestImpl> request;
  const std::string client_id;
  const std::string client_secret;
  const ScopeSet scopes;
};

void DeviceOAuth2TokenService::OnValidationCompleted(
    GoogleServiceAuthError::State error) {
  if (error == GoogleServiceAuthError::NONE)
    FlushPendingRequests(true, GoogleServiceAuthError::NONE);
  else
    FlushPendingRequests(false, error);
}

DeviceOAuth2TokenService::DeviceOAuth2TokenService(
    DeviceOAuth2TokenServiceDelegate* delegate)
    : OAuth2TokenService(delegate),
      delegate_(static_cast<DeviceOAuth2TokenServiceDelegate*>(delegate)) {
  delegate_->SetValidationStatusDelegate(this);
}

DeviceOAuth2TokenService::~DeviceOAuth2TokenService() {
  delegate_->SetValidationStatusDelegate(nullptr);
  FlushPendingRequests(false, GoogleServiceAuthError::REQUEST_CANCELED);
}

// static
void DeviceOAuth2TokenService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceRobotAnyApiRefreshToken,
                               std::string());
}

void DeviceOAuth2TokenService::SetAndSaveRefreshToken(
    const std::string& refresh_token,
    const StatusCallback& result_callback) {
  delegate_->SetAndSaveRefreshToken(refresh_token, result_callback);
}

std::string DeviceOAuth2TokenService::GetRobotAccountId() const {
  return delegate_->GetRobotAccountId();
}

void DeviceOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  switch (delegate_->state_) {
    case DeviceOAuth2TokenServiceDelegate::STATE_VALIDATION_PENDING:
      // If this is the first request for a token, start validation.
      delegate_->StartValidation();
      // fall through.
    case DeviceOAuth2TokenServiceDelegate::STATE_LOADING:
    case DeviceOAuth2TokenServiceDelegate::STATE_VALIDATION_STARTED:
      // Add a pending request that will be satisfied once validation completes.
      pending_requests_.push_back(new PendingRequest(
          request->AsWeakPtr(), client_id, client_secret, scopes));
      delegate_->RequestValidation();
      return;
    case DeviceOAuth2TokenServiceDelegate::STATE_NO_TOKEN:
      FailRequest(request, GoogleServiceAuthError::USER_NOT_SIGNED_UP);
      return;
    case DeviceOAuth2TokenServiceDelegate::STATE_TOKEN_INVALID:
      FailRequest(request, GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
      return;
    case DeviceOAuth2TokenServiceDelegate::STATE_TOKEN_VALID:
      // Pass through to OAuth2TokenService to satisfy the request.
      OAuth2TokenService::FetchOAuth2Token(
          request, account_id, getter, client_id, client_secret, scopes);
      return;
  }

  NOTREACHED() << "Unexpected state " << delegate_->state_;
}

void DeviceOAuth2TokenService::FlushPendingRequests(
    bool token_is_valid,
    GoogleServiceAuthError::State error) {
  std::vector<PendingRequest*> requests;
  requests.swap(pending_requests_);
  for (std::vector<PendingRequest*>::iterator request(requests.begin());
       request != requests.end();
       ++request) {
    scoped_ptr<PendingRequest> scoped_request(*request);
    if (!scoped_request->request)
      continue;

    if (token_is_valid) {
      OAuth2TokenService::FetchOAuth2Token(
          scoped_request->request.get(),
          scoped_request->request->GetAccountId(),
          delegate_->GetRequestContext(), scoped_request->client_id,
          scoped_request->client_secret, scoped_request->scopes);
    } else {
      FailRequest(scoped_request->request.get(), error);
    }
  }
}

void DeviceOAuth2TokenService::FailRequest(
    RequestImpl* request,
    GoogleServiceAuthError::State error) {
  GoogleServiceAuthError auth_error(error);
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &RequestImpl::InformConsumer,
      request->AsWeakPtr(),
      auth_error,
      std::string(),
      base::Time()));
}

}  // namespace chromeos
