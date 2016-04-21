// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/profile_sync_auth_provider.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"

// This is thin proxy class for forwarding calls between sync and UI threads.
// The main purpose is to hide the fact that ProfileSyncAuthProvider and
// SyncThreadProxy have independent lifetimes. If ProfileSyncAuthProvider is
// destroyed first calls to RequestAccessToken will never complete. This is fine
// since sync thread is not blocked and is in the process of shutdown anyway.
class ProfileSyncAuthProvider::SyncThreadProxy
    : public syncer::SyncAuthProvider,
      public base::NonThreadSafe {
 public:
  SyncThreadProxy(
      base::WeakPtr<ProfileSyncAuthProvider> provider_impl,
      scoped_refptr<base::SingleThreadTaskRunner> provider_task_runner);

  // syncer::SyncAuthProvider implementation.
  void RequestAccessToken(const RequestTokenCallback& callback) override;
  void InvalidateAccessToken(const std::string& token) override;

 private:
  base::WeakPtr<ProfileSyncAuthProvider> provider_impl_;
  scoped_refptr<base::SingleThreadTaskRunner> provider_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SyncThreadProxy);
};

ProfileSyncAuthProvider::SyncThreadProxy::SyncThreadProxy(
    base::WeakPtr<ProfileSyncAuthProvider> provider_impl,
    scoped_refptr<base::SingleThreadTaskRunner> provider_task_runner)
    : provider_impl_(provider_impl),
      provider_task_runner_(provider_task_runner) {
  // SyncThreadProxy is created on UI thread but used on sync thread.
  // Detach NonThreadSafe from UI thread so that it can reattach to sync thread
  // on first invocation.
  DetachFromThread();
}

void ProfileSyncAuthProvider::SyncThreadProxy::RequestAccessToken(
    const RequestTokenCallback& callback) {
  DCHECK(CalledOnValidThread());
  provider_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ProfileSyncAuthProvider::RequestAccessToken,
                 provider_impl_,
                 callback,
                 base::ThreadTaskRunnerHandle::Get()));
}

void ProfileSyncAuthProvider::SyncThreadProxy::InvalidateAccessToken(
    const std::string& token) {
  DCHECK(CalledOnValidThread());
  provider_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ProfileSyncAuthProvider::InvalidateAccessToken,
                 provider_impl_,
                 token));
}

ProfileSyncAuthProvider::ProfileSyncAuthProvider(
    ProfileOAuth2TokenService* token_service,
    const std::string& account_id,
    const std::string& scope)
    : OAuth2TokenService::Consumer("sync_auth_provider"),
      token_service_(token_service),
      account_id_(account_id),
      weak_factory_(this) {
  oauth2_scope_.insert(scope);
}

ProfileSyncAuthProvider::~ProfileSyncAuthProvider() {
  DCHECK(CalledOnValidThread());
}

void ProfileSyncAuthProvider::RequestAccessToken(
    const syncer::SyncAuthProvider::RequestTokenCallback& callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(CalledOnValidThread());
  if (access_token_request_ != NULL) {
    // If there is already pending request report it as cancelled.
    GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
    RespondToTokenRequest(error, std::string());
  }
  request_token_callback_ = callback;
  callback_task_runner_ = task_runner;
  access_token_request_ =
      token_service_->StartRequest(account_id_, oauth2_scope_, this);
}

void ProfileSyncAuthProvider::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(access_token_request_.get(), request);
  RespondToTokenRequest(GoogleServiceAuthError::AuthErrorNone(), access_token);
}

void ProfileSyncAuthProvider::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(access_token_request_.get(), request);
  RespondToTokenRequest(error, std::string());
}

void ProfileSyncAuthProvider::RespondToTokenRequest(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  DCHECK(CalledOnValidThread());
  callback_task_runner_->PostTask(
      FROM_HERE, base::Bind(request_token_callback_, error, token));
  access_token_request_.reset();
  request_token_callback_.Reset();
  callback_task_runner_ = NULL;
}

void ProfileSyncAuthProvider::InvalidateAccessToken(const std::string& token) {
  DCHECK(CalledOnValidThread());
  token_service_->InvalidateAccessToken(account_id_, oauth2_scope_, token);
}

std::unique_ptr<syncer::SyncAuthProvider>
ProfileSyncAuthProvider::CreateProviderForSyncThread() {
  DCHECK(CalledOnValidThread());
  std::unique_ptr<syncer::SyncAuthProvider> auth_provider(new SyncThreadProxy(
      weak_factory_.GetWeakPtr(), base::ThreadTaskRunnerHandle::Get()));
  return auth_provider;
}
