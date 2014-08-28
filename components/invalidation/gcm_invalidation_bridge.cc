// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/invalidation/gcm_invalidation_bridge.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/identity_provider.h"

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

// Cacheinvalidation specific gcm message keys.
const char kContentKey[] = "content";
const char kEchoTokenKey[] = "echo-token";
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
  virtual void Initialize(ConnectionStateCallback callback) OVERRIDE;
  virtual void RequestToken(RequestTokenCallback callback) OVERRIDE;
  virtual void InvalidateToken(const std::string& token) OVERRIDE;
  virtual void Register(RegisterCallback callback) OVERRIDE;
  virtual void SetMessageReceiver(MessageCallback callback) OVERRIDE;

  void RequestTokenFinished(RequestTokenCallback callback,
                            const GoogleServiceAuthError& error,
                            const std::string& token);

  void RegisterFinished(RegisterCallback callback,
                        const std::string& registration_id,
                        gcm::GCMClient::Result result);

  void OnIncomingMessage(const std::string& message,
                         const std::string& echo_token);

  void OnConnectionStateChanged(bool online);

 private:
  base::WeakPtr<GCMInvalidationBridge> bridge_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;

  MessageCallback message_callback_;
  ConnectionStateCallback connection_state_callback_;

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

void GCMInvalidationBridge::Core::Initialize(ConnectionStateCallback callback) {
  DCHECK(CalledOnValidThread());
  connection_state_callback_ = callback;
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

void GCMInvalidationBridge::Core::SetMessageReceiver(MessageCallback callback) {
  message_callback_ = callback;
  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMInvalidationBridge::SubscribeForIncomingMessages,
                 bridge_));
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

void GCMInvalidationBridge::Core::OnIncomingMessage(
    const std::string& message,
    const std::string& echo_token) {
  DCHECK(!message_callback_.is_null());
  message_callback_.Run(message, echo_token);
}

void GCMInvalidationBridge::Core::OnConnectionStateChanged(bool online) {
  if (!connection_state_callback_.is_null()) {
    connection_state_callback_.Run(online);
  }
}

GCMInvalidationBridge::GCMInvalidationBridge(
    gcm::GCMDriver* gcm_driver,
    IdentityProvider* identity_provider)
    : OAuth2TokenService::Consumer("gcm_network_channel"),
      gcm_driver_(gcm_driver),
      identity_provider_(identity_provider),
      subscribed_for_incoming_messages_(false),
      weak_factory_(this) {}

GCMInvalidationBridge::~GCMInvalidationBridge() {
  if (subscribed_for_incoming_messages_) {
    gcm_driver_->RemoveAppHandler(kInvalidationsAppId);
    gcm_driver_->RemoveConnectionObserver(this);
  }
}

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
  request_token_callback_ = callback;
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  access_token_request_ = identity_provider_->GetTokenService()->StartRequest(
      identity_provider_->GetActiveAccountId(), scopes, this);
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
  DCHECK(CalledOnValidThread());
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  identity_provider_->GetTokenService()->InvalidateToken(
      identity_provider_->GetActiveAccountId(), scopes, token);
}

void GCMInvalidationBridge::Register(
    syncer::GCMNetworkChannelDelegate::RegisterCallback callback) {
  DCHECK(CalledOnValidThread());
  // No-op if GCMClient is disabled.
  if (gcm_driver_ == NULL)
    return;

  std::vector<std::string> sender_ids;
  sender_ids.push_back(kInvalidationsSenderId);
  gcm_driver_->Register(kInvalidationsAppId,
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

void GCMInvalidationBridge::SubscribeForIncomingMessages() {
  // No-op if GCMClient is disabled.
  if (gcm_driver_ == NULL)
    return;

  DCHECK(!subscribed_for_incoming_messages_);
  gcm_driver_->AddAppHandler(kInvalidationsAppId, this);
  gcm_driver_->AddConnectionObserver(this);
  core_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMInvalidationBridge::Core::OnConnectionStateChanged,
                 core_,
                 gcm_driver_->IsConnected()));

  subscribed_for_incoming_messages_ = true;
}

void GCMInvalidationBridge::ShutdownHandler() {
  // Nothing to do.
}

void GCMInvalidationBridge::OnMessage(
    const std::string& app_id,
    const gcm::GCMClient::IncomingMessage& message) {
  gcm::GCMClient::MessageData::const_iterator it;
  std::string content;
  std::string echo_token;
  it = message.data.find(kContentKey);
  if (it != message.data.end())
    content = it->second;
  it = message.data.find(kEchoTokenKey);
  if (it != message.data.end())
    echo_token = it->second;

  core_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMInvalidationBridge::Core::OnIncomingMessage,
                 core_,
                 content,
                 echo_token));
}

void GCMInvalidationBridge::OnMessagesDeleted(const std::string& app_id) {
  // Cacheinvalidation doesn't use long lived non-collapsable messages with GCM.
  // Android implementation of cacheinvalidation doesn't handle MessagesDeleted
  // callback so this should be no-op in desktop version as well.
}

void GCMInvalidationBridge::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {
  // cacheinvalidation doesn't send messages over GCM.
  NOTREACHED();
}

void GCMInvalidationBridge::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  // cacheinvalidation doesn't send messages over GCM.
  NOTREACHED();
}

void GCMInvalidationBridge::OnConnected(const net::IPEndPoint& ip_endpoint) {
  core_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &GCMInvalidationBridge::Core::OnConnectionStateChanged, core_, true));
}

void GCMInvalidationBridge::OnDisconnected() {
  core_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GCMInvalidationBridge::Core::OnConnectionStateChanged,
                 core_,
                 false));
}


}  // namespace invalidation
