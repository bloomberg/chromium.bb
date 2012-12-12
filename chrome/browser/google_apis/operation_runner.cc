// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/operation_runner.h"

#include "base/bind.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/base_operations.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace google_apis {

OperationRunner::OperationRunner(
    Profile* profile,
    net::URLRequestContextGetter* url_request_context_getter,
    const std::vector<std::string>& scopes,
    const std::string& custom_user_agent)
    : profile_(profile),
      auth_service_(new AuthService(url_request_context_getter, scopes)),
      operation_registry_(new OperationRegistry()),
      custom_user_agent_(custom_user_agent),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

OperationRunner::~OperationRunner() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void OperationRunner::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auth_service_->Initialize(profile_);
}

void OperationRunner::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  operation_registry_->CancelAll();
}

void OperationRunner::StartOperationWithRetry(
    AuthenticatedOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!auth_service_->HasAccessToken()) {
    // Fetch OAuth2 access token from the refresh token first.
    auth_service_->StartAuthentication(
        operation_registry_.get(),
        base::Bind(&OperationRunner::OnAccessTokenFetched,
                   weak_ptr_factory_.GetWeakPtr(),
                   operation->GetWeakPtr()));
    return;
  }

  operation->Start(auth_service_->access_token(),
                   custom_user_agent_,
                   base::Bind(&OperationRunner::RetryOperation,
                              weak_ptr_factory_.GetWeakPtr()));
}

void OperationRunner::OnAccessTokenFetched(
    const base::WeakPtr<AuthenticatedOperationInterface>& operation,
    GDataErrorCode code,
    const std::string& /* access_token */) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Do nothing if the operation is canceled during authentication.
  if (!operation)
    return;

  if (code == HTTP_SUCCESS) {
    DCHECK(auth_service_->HasAccessToken());
    StartOperationWithRetry(operation);
  } else {
    operation->OnAuthFailed(code);
  }
}

void OperationRunner::RetryOperation(
    AuthenticatedOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  auth_service_->ClearAccessToken();
  // User authentication might have expired - rerun the request to force
  // auth token refresh.
  StartOperationWithRetry(operation);
}

}  // namespace google_apis
