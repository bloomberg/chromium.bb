// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/invalidation/gcm_invalidation_bridge.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
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

}  // namespace

// Core should be very simple class that implements GCMNetwrokChannelDelegate
// and passes all calls to GCMInvalidationBridge. All calls should be serialized
// through GCMInvalidationBridge to avoid race conditions.
class GCMInvalidationBridge::Core : public syncer::GCMNetworkChannelDelegate,
                                    public base::NonThreadSafe {
 public:
  Core(base::WeakPtr<GCMInvalidationBridge> bridge,
       scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);
  virtual ~Core();

  // syncer::GCMNetworkChannelDelegate implementation.
  virtual void Initialize() OVERRIDE;
  virtual void RequestToken(RequestTokenCallback callback) OVERRIDE;
  virtual void InvalidateToken(const std::string& token) OVERRIDE;
  virtual void Register(RegisterCallback callback) OVERRIDE;

  void RequestTokenFinished(RequestTokenCallback callback,
                            const GoogleServiceAuthError& error,
                            const std::string& token);

  void RegisterFinished(RegisterCallback callback,
                        const std::string& registration_id,
                        gcm::GCMClient::Result result);

 private:
  base::WeakPtr<GCMInvalidationBridge> bridge_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;

  base::WeakPtrFactory<Core> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

GCMInvalidationBridge::Core::Core(
    base::WeakPtr<GCMInvalidationBridge> bridge,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
    : bridge_(bridge),
      ui_thread_task_runner_(ui_thread_task_runner),
      weak_factory_(this) {
  // Core is created on UI thread but all calls happen on IO thread.
  DetachFromThread();
}

GCMInvalidationBridge::Core::~Core() {}

void GCMInvalidationBridge::Core::Initialize() {
  DCHECK(CalledOnValidThread());
  // Pass core WeapPtr and TaskRunner to GCMInvalidationBridge for it to be able
  // to post back.
  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMInvalidationBridge::CoreInitializationDone,
                 bridge_,
                 weak_factory_.GetWeakPtr(),
                 base::ThreadTaskRunnerHandle::Get()));
}

void GCMInvalidationBridge::Core::RequestToken(RequestTokenCallback callback) {
  DCHECK(CalledOnValidThread());
  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMInvalidationBridge::RequestToken, bridge_, callback));
}

void GCMInvalidationBridge::Core::InvalidateToken(const std::string& token) {
  DCHECK(CalledOnValidThread());
  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMInvalidationBridge::InvalidateToken, bridge_, token));
}

void GCMInvalidationBridge::Core::Register(RegisterCallback callback) {
  DCHECK(CalledOnValidThread());
  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMInvalidationBridge::Register, bridge_, callback));
}

void GCMInvalidationBridge::Core::RequestTokenFinished(
    RequestTokenCallback callback,
    const GoogleServiceAuthError& error,
    const std::string& token) {
  DCHECK(CalledOnValidThread());
  callback.Run(error, token);
}

void GCMInvalidationBridge::Core::RegisterFinished(
    RegisterCallback callback,
    const std::string& registration_id,
    gcm::GCMClient::Result result) {
  DCHECK(CalledOnValidThread());
  callback.Run(registration_id, result);
}

GCMInvalidationBridge::GCMInvalidationBridge(Profile* profile)
    : OAuth2TokenService::Consumer("gcm_network_channel"),
      profile_(profile),
      weak_factory_(this) {}

GCMInvalidationBridge::~GCMInvalidationBridge() {}

scoped_ptr<syncer::GCMNetworkChannelDelegate>
GCMInvalidationBridge::CreateDelegate() {
  DCHECK(CalledOnValidThread());
  scoped_ptr<syncer::GCMNetworkChannelDelegate> core(new Core(
      weak_factory_.GetWeakPtr(), base::ThreadTaskRunnerHandle::Get()));
  return core.Pass();
}

void GCMInvalidationBridge::CoreInitializationDone(
    base::WeakPtr<Core> core,
    scoped_refptr<base::SingleThreadTaskRunner> core_thread_task_runner) {
  DCHECK(CalledOnValidThread());
  core_ = core;
  core_thread_task_runner_ = core_thread_task_runner;
}

void GCMInvalidationBridge::RequestToken(
    syncer::GCMNetworkChannelDelegate::RequestTokenCallback callback) {
  DCHECK(CalledOnValidThread());
  if (access_token_request_ != NULL) {
    // Report previous request as cancelled.
    GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
    std::string access_token;
    core_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&GCMInvalidationBridge::Core::RequestTokenFinished,
                   core_,
                   request_token_callback_,
                   error,
                   access_token));
  }
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  request_token_callback_ = callback;
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  std::string account_id = signin_manager->GetAuthenticatedAccountId();
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kGoogleTalkOAuth2Scope);
  access_token_request_ = token_service->StartRequest(account_id, scopes, this);
}

void GCMInvalidationBridge::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(access_token_request_, request);
  core_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMInvalidationBridge::Core::RequestTokenFinished,
                 core_,
                 request_token_callback_,
                 GoogleServiceAuthError::AuthErrorNone(),
                 access_token));
  request_token_callback_.Reset();
  access_token_request_.reset();
}

void GCMInvalidationBridge::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(access_token_request_, request);
  core_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMInvalidationBridge::Core::RequestTokenFinished,
                 core_,
                 request_token_callback_,
                 error,
                 std::string()));
  request_token_callback_.Reset();
  access_token_request_.reset();
}

void GCMInvalidationBridge::InvalidateToken(const std::string& token) {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  DCHECK(CalledOnValidThread());
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  std::string account_id = signin_manager->GetAuthenticatedAccountId();
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kGoogleTalkOAuth2Scope);
  token_service->InvalidateToken(account_id, scopes, token);
}

void GCMInvalidationBridge::Register(
    syncer::GCMNetworkChannelDelegate::RegisterCallback callback) {
  DCHECK(CalledOnValidThread());
  // No-op if GCMClient is disabled.
  gcm::GCMProfileService* gcm_profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile_);
  if (gcm_profile_service == NULL)
    return;

  std::vector<std::string> sender_ids;
  sender_ids.push_back(kInvalidationsSenderId);
  gcm_profile_service->Register(
      kInvalidationsAppId,
      sender_ids,
      base::Bind(&GCMInvalidationBridge::RegisterFinished,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void GCMInvalidationBridge::RegisterFinished(
    syncer::GCMNetworkChannelDelegate::RegisterCallback callback,
    const std::string& registration_id,
    gcm::GCMClient::Result result) {
  DCHECK(CalledOnValidThread());
  core_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMInvalidationBridge::Core::RegisterFinished,
                 core_,
                 callback,
                 registration_id,
                 result));
}

}  // namespace invalidation
