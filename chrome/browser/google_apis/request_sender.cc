// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/request_sender.h"

#include "base/bind.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/base_requests.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace google_apis {

RequestSender::RequestSender(
    Profile* profile,
    net::URLRequestContextGetter* url_request_context_getter,
    const std::vector<std::string>& scopes,
    const std::string& custom_user_agent)
    : profile_(profile),
      auth_service_(new AuthService(url_request_context_getter, scopes)),
      request_registry_(new RequestRegistry()),
      custom_user_agent_(custom_user_agent),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

RequestSender::~RequestSender() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void RequestSender::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auth_service_->Initialize(profile_);
}

void RequestSender::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  request_registry_->CancelAll();
}

base::Closure RequestSender::StartRequestWithRetry(
    AuthenticatedRequestInterface* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kinaba): Stop relying on weak pointers. Move lifetime management
  // of the requests to request sender.
  base::Closure cancel_closure =
      base::Bind(&RequestSender::CancelRequest,
                 weak_ptr_factory_.GetWeakPtr(),
                 request->GetWeakPtr());

  if (!auth_service_->HasAccessToken()) {
    // Fetch OAuth2 access token from the refresh token first.
    auth_service_->StartAuthentication(
        base::Bind(&RequestSender::OnAccessTokenFetched,
                   weak_ptr_factory_.GetWeakPtr(),
                   request->GetWeakPtr()));
  } else {
    request->Start(auth_service_->access_token(),
                   custom_user_agent_,
                   base::Bind(&RequestSender::RetryRequest,
                              weak_ptr_factory_.GetWeakPtr()));
  }

  return cancel_closure;
}

void RequestSender::OnAccessTokenFetched(
    const base::WeakPtr<AuthenticatedRequestInterface>& request,
    GDataErrorCode code,
    const std::string& /* access_token */) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Do nothing if the request is canceled during authentication.
  if (!request.get())
    return;

  if (code == HTTP_SUCCESS) {
    DCHECK(auth_service_->HasAccessToken());
    StartRequestWithRetry(request.get());
  } else {
    request->OnAuthFailed(code);
  }
}

void RequestSender::RetryRequest(AuthenticatedRequestInterface* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  auth_service_->ClearAccessToken();
  // User authentication might have expired - rerun the request to force
  // auth token refresh.
  StartRequestWithRetry(request);
}

void RequestSender::CancelRequest(
    const base::WeakPtr<AuthenticatedRequestInterface>& request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Do nothing if the request is already finished.
  if (!request.get())
    return;
  request_registry_->CancelRequest(request->AsRequestRegistryRequest());
}

}  // namespace google_apis
