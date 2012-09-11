// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/operation_runner.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/gdata/auth_service.h"
#include "chrome/browser/google_apis/operations_base.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

OperationRunner::OperationRunner(Profile* profile,
                                 const std::vector<std::string>& scopes)
    : profile_(profile),
      auth_service_(new AuthService(scopes)),
      operation_registry_(new OperationRegistry()),
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

void OperationRunner::Authenticate(const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auth_service_->StartAuthentication(operation_registry_.get(), callback);
}

void OperationRunner::StartOperationWithRetry(
    AuthenticatedOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The re-authenticatation callback will run on UI thread.
  operation->SetReAuthenticateCallback(
      base::Bind(&OperationRunner::RetryOperation,
                 weak_ptr_factory_.GetWeakPtr()));
  StartOperation(operation);
}

void OperationRunner::StartOperation(
    AuthenticatedOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!auth_service_->HasAccessToken()) {
    // Fetch OAuth2 authentication token from the refresh token first.
    auth_service_->StartAuthentication(
        operation_registry_.get(),
        base::Bind(&OperationRunner::OnOperationAuthRefresh,
                   weak_ptr_factory_.GetWeakPtr(),
                   operation));
    return;
  }

  operation->Start(auth_service_->access_token());
}

void OperationRunner::OnOperationAuthRefresh(
    AuthenticatedOperationInterface* operation,
    GDataErrorCode code,
    const std::string& auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (code == HTTP_SUCCESS) {
    DCHECK(auth_service_->HasRefreshToken());
    StartOperation(operation);
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
  StartOperation(operation);
}

}  // namespace gdata
