// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_operation_runner.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/gdata/gdata_operations.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

GDataOperationRunner::GDataOperationRunner(Profile* profile)
    : profile_(profile),
      auth_service_(new GDataAuthService()),
      operation_registry_(new GDataOperationRegistry()),
      weak_ptr_factory_(this),
      weak_ptr_bound_to_ui_thread_(weak_ptr_factory_.GetWeakPtr()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auth_service_->AddObserver(this);
}

GDataOperationRunner::~GDataOperationRunner() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auth_service_->RemoveObserver(this);
}

void GDataOperationRunner::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auth_service_->Initialize(profile_);
}

void GDataOperationRunner::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  operation_registry_->CancelAll();
}

void GDataOperationRunner::Authenticate(const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auth_service_->StartAuthentication(operation_registry_.get(), callback);
}

void GDataOperationRunner::StartOperationWithRetry(
    GDataOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The re-authenticatation callback will run on UI thread.
  operation->SetReAuthenticateCallback(
      base::Bind(&GDataOperationRunner::RetryOperation,
                 weak_ptr_bound_to_ui_thread_));
  StartOperation(operation);
}

void GDataOperationRunner::StartOperation(GDataOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!auth_service_->IsFullyAuthenticated()) {
    // Fetch OAuth2 authentication token from the refresh token first.
    auth_service_->StartAuthentication(
        operation_registry_.get(),
        base::Bind(&GDataOperationRunner::OnOperationAuthRefresh,
                   weak_ptr_bound_to_ui_thread_,
                   operation));
    return;
  }

  operation->Start(auth_service_->oauth2_auth_token());
}

void GDataOperationRunner::OnOperationAuthRefresh(
    GDataOperationInterface* operation,
    GDataErrorCode code,
    const std::string& auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (code == HTTP_SUCCESS) {
    DCHECK(auth_service_->IsPartiallyAuthenticated());
    StartOperation(operation);
  } else {
    operation->OnAuthFailed(code);
  }
}

void GDataOperationRunner::RetryOperation(GDataOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  auth_service_->ClearOAuth2Token();
  // User authentication might have expired - rerun the request to force
  // auth token refresh.
  StartOperation(operation);
}

void GDataOperationRunner::OnOAuth2RefreshTokenChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

}  // namespace gdata
