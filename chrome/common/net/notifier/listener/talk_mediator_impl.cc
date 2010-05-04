// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/notifier/listener/talk_mediator_impl.h"

#include "base/logging.h"
#include "base/singleton.h"
#include "chrome/common/net/notifier/listener/mediator_thread_impl.h"
#include "chrome/common/deprecated/event_sys-inl.h"
#include "talk/base/cryptstring.h"
#include "talk/base/ssladapter.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/xmppengine.h"

namespace notifier {

// Before any authorization event from TalkMediatorImpl, we need to initialize
// the SSL library.
class SslInitializationSingleton {
 public:
  virtual ~SslInitializationSingleton() {
    talk_base::CleanupSSL();
  };

  void RegisterClient() {}

  static SslInitializationSingleton* GetInstance() {
    return Singleton<SslInitializationSingleton>::get();
  }

 private:
  friend struct DefaultSingletonTraits<SslInitializationSingleton>;

  SslInitializationSingleton() {
    talk_base::InitializeSSL();
  };

  DISALLOW_COPY_AND_ASSIGN(SslInitializationSingleton);
};

TalkMediatorImpl::TalkMediatorImpl(bool invalidate_xmpp_auth_token)
    : mediator_thread_(new MediatorThreadImpl()),
      invalidate_xmpp_auth_token_(invalidate_xmpp_auth_token) {
  // Ensure the SSL library is initialized.
  SslInitializationSingleton::GetInstance()->RegisterClient();

  // Construct the callback channel with the shutdown event.
  TalkMediatorInitialization(false);
}

TalkMediatorImpl::TalkMediatorImpl(MediatorThread *thread)
    : mediator_thread_(thread),
      invalidate_xmpp_auth_token_(false) {
  // When testing we do not initialize the SSL library.
  TalkMediatorInitialization(true);
}

void TalkMediatorImpl::TalkMediatorInitialization(bool should_connect) {
  TalkMediatorEvent done = { TalkMediatorEvent::TALKMEDIATOR_DESTROYED };
  channel_.reset(new TalkMediatorChannel(done));
  if (should_connect) {
    mediator_thread_->SignalStateChange.connect(
        this,
        &TalkMediatorImpl::MediatorThreadMessageHandler);
    mediator_thread_->SignalNotificationReceived.connect(
        this,
        &TalkMediatorImpl::MediatorThreadNotificationHandler);
    state_.connected = 1;
  }
  mediator_thread_->Start();
  state_.started = 1;
}

TalkMediatorImpl::~TalkMediatorImpl() {
  if (state_.started) {
    Logout();
  }
}

bool TalkMediatorImpl::Login() {
  AutoLock lock(mutex_);
  // Connect to the mediator thread and start processing messages.
  if (!state_.connected) {
    mediator_thread_->SignalStateChange.connect(
        this,
        &TalkMediatorImpl::MediatorThreadMessageHandler);
    mediator_thread_->SignalNotificationReceived.connect(
        this,
        &TalkMediatorImpl::MediatorThreadNotificationHandler);
    state_.connected = 1;
  }
  if (state_.initialized && !state_.logging_in) {
    mediator_thread_->Login(xmpp_settings_);
    state_.logging_in = 1;
    return true;
  }
  return false;
}

bool TalkMediatorImpl::Logout() {
  AutoLock lock(mutex_);
  // We do not want to be called back during logout since we may be closing.
  if (state_.connected) {
    mediator_thread_->SignalStateChange.disconnect(this);
    mediator_thread_->SignalNotificationReceived.disconnect(this);
    state_.connected = 0;
  }
  if (state_.started) {
    mediator_thread_->Logout();
    state_.started = 0;
    state_.logging_in = 0;
    state_.logged_in = 0;
    state_.subscribed = 0;
    return true;
  }
  return false;
}

bool TalkMediatorImpl::SendNotification(const OutgoingNotificationData& data) {
  AutoLock lock(mutex_);
  if (state_.logged_in && state_.subscribed) {
    mediator_thread_->SendNotification(data);
    return true;
  }
  return false;
}

TalkMediatorChannel* TalkMediatorImpl::channel() const {
  return channel_.get();
}

bool TalkMediatorImpl::SetAuthToken(const std::string& email,
                                    const std::string& token,
                                    const std::string& token_service) {
  AutoLock lock(mutex_);

  // Verify that we can create a JID from the email provided.
  buzz::Jid jid = buzz::Jid(email);
  if (jid.node().empty() || !jid.IsValid()) {
    return false;
  }

  // Construct the XmppSettings object for login to buzz.
  xmpp_settings_.set_user(jid.node());
  xmpp_settings_.set_resource("chrome-sync");
  xmpp_settings_.set_host(jid.domain());
  xmpp_settings_.set_use_tls(true);
  xmpp_settings_.set_auth_cookie(invalidate_xmpp_auth_token_ ?
                                 token + "bogus" : token);
  xmpp_settings_.set_token_service(token_service);

  state_.initialized = 1;
  return true;
}

void TalkMediatorImpl::AddSubscribedServiceUrl(
    const std::string& service_url) {
  subscribed_services_list_.push_back(service_url);
  if (state_.logged_in) {
    LOG(INFO) << "Resubscribing for updates, a new service got added";
    mediator_thread_->SubscribeForUpdates(subscribed_services_list_);
  }
}


void TalkMediatorImpl::MediatorThreadMessageHandler(
    MediatorThread::MediatorMessage message) {
  LOG(INFO) << "P2P: MediatorThread has passed a message";
  switch (message) {
    case MediatorThread::MSG_LOGGED_IN:
      OnLogin();
      break;
    case MediatorThread::MSG_LOGGED_OUT:
      OnLogout();
      break;
    case MediatorThread::MSG_SUBSCRIPTION_SUCCESS:
      OnSubscriptionSuccess();
      break;
    case MediatorThread::MSG_SUBSCRIPTION_FAILURE:
      OnSubscriptionFailure();
      break;
    case MediatorThread::MSG_NOTIFICATION_SENT:
      OnNotificationSent();
      break;
    default:
      LOG(WARNING) << "P2P: Unknown message returned from mediator thread.";
      break;
  }
}

void TalkMediatorImpl::MediatorThreadNotificationHandler(
    const IncomingNotificationData& notification_data) {
  LOG(INFO) << "P2P: Updates are available on the server.";
  AutoLock lock(mutex_);
  TalkMediatorEvent event = { TalkMediatorEvent::NOTIFICATION_RECEIVED };
  event.notification_data = notification_data;
  channel_->NotifyListeners(event);
}


void TalkMediatorImpl::OnLogin() {
  LOG(INFO) << "P2P: Logged in.";
  AutoLock lock(mutex_);
  state_.logging_in = 0;
  state_.logged_in = 1;
  // ListenForUpdates enables the ListenTask.  This is done before
  // SubscribeForUpdates.
  mediator_thread_->ListenForUpdates();
  // Now subscribe for updates to all the services we are interested in
  mediator_thread_->SubscribeForUpdates(subscribed_services_list_);
  TalkMediatorEvent event = { TalkMediatorEvent::LOGIN_SUCCEEDED };
  channel_->NotifyListeners(event);
}

void TalkMediatorImpl::OnLogout() {
  LOG(INFO) << "P2P: Logged off.";
  OnSubscriptionFailure();
  AutoLock lock(mutex_);
  state_.logging_in = 0;
  state_.logged_in = 0;
  TalkMediatorEvent event = { TalkMediatorEvent::LOGOUT_SUCCEEDED };
  channel_->NotifyListeners(event);
}

void TalkMediatorImpl::OnSubscriptionSuccess() {
  LOG(INFO) << "P2P: Update subscription active.";
  {
    AutoLock lock(mutex_);
    state_.subscribed = 1;
  }
  // The above scope exists so that we can release the lock before
  // notifying listeners. In theory we should do this for all methods.
  // Notifying listeners with a lock held can cause the lock to be
  // recursively taken if the listener decides to call back into us
  // in the event handler.
  TalkMediatorEvent event = { TalkMediatorEvent::SUBSCRIPTIONS_ON };
  channel_->NotifyListeners(event);
}

void TalkMediatorImpl::OnSubscriptionFailure() {
  LOG(INFO) << "P2P: Update subscription failure.";
  AutoLock lock(mutex_);
  state_.subscribed = 0;
  TalkMediatorEvent event = { TalkMediatorEvent::SUBSCRIPTIONS_OFF };
  channel_->NotifyListeners(event);
}

void TalkMediatorImpl::OnNotificationSent() {
  LOG(INFO) <<
      "P2P: Peers were notified that updates are available on the server.";
  AutoLock lock(mutex_);
  TalkMediatorEvent event = { TalkMediatorEvent::NOTIFICATION_SENT };
  channel_->NotifyListeners(event);
}

}  // namespace notifier
