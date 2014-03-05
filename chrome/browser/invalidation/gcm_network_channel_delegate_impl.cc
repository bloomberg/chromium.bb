// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/invalidation/gcm_network_channel_delegate_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_request.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"

namespace invalidation {

namespace {

// For 3rd party developers SenderId should come from application dashboard when
// server side application is registered with Google. Android invalidations use
// legacy format where gmail account can be specificed. Below value is copied
// from Android.
const char kInvalidationsSenderId[] = "ipc.invalidation@gmail.com";
// In Android world AppId is provided by operating system and should
// match package name and hash of application. In desktop world these values
// are arbitrary and not verified/enforced by registration service (yet).
const char kInvalidationsAppId[] = "com.google.chrome.invalidations";

// In each call to Register object of RegisterCall will be created.
// Its purpose is to pass context (profile and callback) around between threads
// and async call to GCMProfilkeService::Register.
class RegisterCall : public base::RefCountedThreadSafe<RegisterCall> {
 public:
  RegisterCall(Profile* profile,
               syncer::GCMNetworkChannelDelegate::RegisterCallback callback);

  void RegisterOnUIThread();

  void RegisterFinishedOnUIThread(const std::string& registration_id,
                                  gcm::GCMClient::Result result);
  void RegisterFinished(const std::string& registration_id,
                        gcm::GCMClient::Result result);

 private:
  friend class base::RefCountedThreadSafe<RegisterCall>;
  virtual ~RegisterCall();

  Profile* profile_;
  syncer::GCMNetworkChannelDelegate::RegisterCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(RegisterCall);
};

RegisterCall::RegisterCall(
    Profile* profile,
    syncer::GCMNetworkChannelDelegate::RegisterCallback callback)
    : profile_(profile),
      callback_(callback),
      origin_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

RegisterCall::~RegisterCall() {}

void RegisterCall::RegisterOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // No-op if Profile was destroyed or GCMClient is disabled.
  if (!g_browser_process->profile_manager()->IsValidProfile(profile_))
    return;

  gcm::GCMProfileService* gcm_profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile_);
  if (gcm_profile_service == NULL)
    return;

  std::vector<std::string> sender_ids;
  sender_ids.push_back(kInvalidationsSenderId);
  gcm_profile_service->Register(
      kInvalidationsAppId,
      sender_ids,
      base::Bind(&RegisterCall::RegisterFinishedOnUIThread, this));
}

void RegisterCall::RegisterFinishedOnUIThread(
    const std::string& registration_id,
    gcm::GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  origin_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &RegisterCall::RegisterFinished, this, registration_id, result));
}

void RegisterCall::RegisterFinished(const std::string& registration_id,
                                    gcm::GCMClient::Result result) {
  DCHECK(origin_thread_task_runner_->BelongsToCurrentThread());
  callback_.Run(registration_id, result);
}

}  // namespace

GCMNetworkChannelDelegateImpl::GCMNetworkChannelDelegateImpl(Profile* profile)
    : OAuth2TokenService::Consumer("gcm_network_channel"),
      profile_(profile),
      ui_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  // Stash account_id. It is needed in RequestToken call on IO thread.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  account_id_ = signin_manager->GetAuthenticatedAccountId();
}

GCMNetworkChannelDelegateImpl::~GCMNetworkChannelDelegateImpl() {}

void GCMNetworkChannelDelegateImpl::Register(RegisterCallback callback) {
  scoped_refptr<RegisterCall> call(new RegisterCall(profile_, callback));
  ui_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RegisterCall::RegisterOnUIThread, call));
}

void GCMNetworkChannelDelegateImpl::RequestToken(
    RequestTokenCallback callback) {
  if (access_token_request_ != NULL) {
    // Report previous request as cancelled.
    GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
    std::string access_token;
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(request_token_callback_, error, access_token));
  }
  request_token_callback_ = callback;
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  access_token_request_.reset(ProfileOAuth2TokenServiceRequest::CreateAndStart(
      profile_, account_id_, scopes, this));
}

void GCMNetworkChannelDelegateImpl::InvalidateToken(const std::string& token) {
  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMNetworkChannelDelegateImpl::InvalidateTokenOnUIThread,
                 profile_,
                 token));
}

void GCMNetworkChannelDelegateImpl::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(access_token_request_, request);
  request_token_callback_.Run(GoogleServiceAuthError::AuthErrorNone(),
                              access_token);
  request_token_callback_.Reset();
  access_token_request_.reset();
}

void GCMNetworkChannelDelegateImpl::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(access_token_request_, request);
  request_token_callback_.Run(error, std::string());
  request_token_callback_.Reset();
  access_token_request_.reset();
}

void GCMNetworkChannelDelegateImpl::InvalidateTokenOnUIThread(
    Profile* profile,
    const std::string& token) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  ProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  std::string account_id = signin_manager->GetAuthenticatedAccountId();
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kGoogleTalkOAuth2Scope);
  oauth2_token_service->InvalidateToken(account_id, scopes, token);
}

}  // namespace invalidation
