// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/listener/talk_mediator_impl.h"

#include "base/logging.h"
#include "base/singleton.h"
#include "chrome/browser/sync/engine/auth_watcher.h"
#include "chrome/browser/sync/engine/syncer_thread.h"
#include "chrome/browser/sync/notifier/listener/mediator_thread_impl.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "talk/base/cryptstring.h"
#include "talk/base/ssladapter.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/xmppengine.h"

namespace browser_sync {

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

TalkMediatorImpl::TalkMediatorImpl()
    : mediator_thread_(new MediatorThreadImpl()) {
  // Ensure the SSL library is initialized.
  SslInitializationSingleton::GetInstance()->RegisterClient();

  // Construct the callback channel with the shutdown event.
  TalkMediatorInitialization(false);
}

TalkMediatorImpl::TalkMediatorImpl(MediatorThread *thread)
    : mediator_thread_(thread) {
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

void TalkMediatorImpl::AuthWatcherEventHandler(
    const AuthWatcherEvent& auth_event) {
  MutexLock lock(&mutex_);
  switch (auth_event.what_happened) {
    case AuthWatcherEvent::AUTHWATCHER_DESTROYED:
    case AuthWatcherEvent::GAIA_AUTH_FAILED:
    case AuthWatcherEvent::SERVICE_AUTH_FAILED:
    case AuthWatcherEvent::SERVICE_CONNECTION_FAILED:
      // We have failed to connect to the buzz server, and we maintain a
      // decreased polling interval and stay in a flaky connection mode.
      // Note that the failure is on the authwatcher's side and can not be
      // resolved without manual retry.
      break;
    case AuthWatcherEvent::AUTHENTICATION_ATTEMPT_START:
      // TODO(brg) : We are restarting the authentication attempt.  We need to
      // insure this code path is stable.
      break;
    case AuthWatcherEvent::AUTH_SUCCEEDED:
      if (!state_.logged_in) {
        DoLogin();
      }
      break;
    default:
      // Do nothing.
      break;
  }
}

void TalkMediatorImpl::WatchAuthWatcher(AuthWatcher* watcher) {
  auth_hookup_.reset(NewEventListenerHookup(
      watcher->channel(),
      this,
      &TalkMediatorImpl::AuthWatcherEventHandler));
}

bool TalkMediatorImpl::Login() {
  MutexLock lock(&mutex_);
  return DoLogin();
}

bool TalkMediatorImpl::DoLogin() {
  // Connect to the mediator thread and start processing messages.
  if (!state_.connected) {
    mediator_thread_->SignalStateChange.connect(
        this,
        &TalkMediatorImpl::MediatorThreadMessageHandler);
    state_.connected = 1;
  }
  if (state_.initialized && !state_.logged_in) {
    mediator_thread_->Login(xmpp_settings_);
    state_.logged_in = 1;
    return true;
  }
  return false;
}

bool TalkMediatorImpl::Logout() {
  MutexLock lock(&mutex_);
  // We do not want to be called back during logout since we may be closing.
  if (state_.connected) {
    mediator_thread_->SignalStateChange.disconnect(this);
    state_.connected = 0;
  }
  if (state_.started) {
    mediator_thread_->Logout();
    state_.started = 0;
    state_.logged_in = 0;
    state_.subscribed = 0;
    return true;
  }
  return false;
}

bool TalkMediatorImpl::SendNotification() {
  MutexLock lock(&mutex_);
  if (state_.logged_in && state_.subscribed) {
    mediator_thread_->SendNotification();
    return true;
  }
  return false;
}

TalkMediatorChannel* TalkMediatorImpl::channel() const {
  return channel_.get();
}

bool TalkMediatorImpl::SetAuthToken(const std::string& email,
                                    const std::string& token) {
  MutexLock lock(&mutex_);

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
  xmpp_settings_.set_auth_cookie(token);

  state_.initialized = 1;
  return true;
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
    case MediatorThread::MSG_NOTIFICATION_RECEIVED:
      OnNotificationReceived();
      break;
    case MediatorThread::MSG_NOTIFICATION_SENT:
      OnNotificationSent();
      break;
    default:
      LOG(WARNING) << "P2P: Unknown message returned from mediator thread.";
      break;
  }
}

void TalkMediatorImpl::OnLogin() {
  LOG(INFO) << "P2P: Logged in.";
  MutexLock lock(&mutex_);
  // ListenForUpdates enables the ListenTask.  This is done before
  // SubscribeForUpdates.
  mediator_thread_->ListenForUpdates();
  mediator_thread_->SubscribeForUpdates();
  TalkMediatorEvent event = { TalkMediatorEvent::LOGIN_SUCCEEDED };
  channel_->NotifyListeners(event);
}

void TalkMediatorImpl::OnLogout() {
  LOG(INFO) << "P2P: Logged off.";
  OnSubscriptionFailure();
  MutexLock lock(&mutex_);
  state_.logged_in = 0;
  TalkMediatorEvent event = { TalkMediatorEvent::LOGOUT_SUCCEEDED };
  channel_->NotifyListeners(event);
}

void TalkMediatorImpl::OnSubscriptionSuccess() {
  LOG(INFO) << "P2P: Update subscription active.";
  MutexLock lock(&mutex_);
  state_.subscribed = 1;
  TalkMediatorEvent event = { TalkMediatorEvent::SUBSCRIPTIONS_ON };
  channel_->NotifyListeners(event);
}

void TalkMediatorImpl::OnSubscriptionFailure() {
  LOG(INFO) << "P2P: Update subscription failure.";
  MutexLock lock(&mutex_);
  state_.subscribed = 0;
  TalkMediatorEvent event = { TalkMediatorEvent::SUBSCRIPTIONS_OFF };
  channel_->NotifyListeners(event);
}

void TalkMediatorImpl::OnNotificationReceived() {
  LOG(INFO) << "P2P: Updates are available on the server.";
  MutexLock lock(&mutex_);
  TalkMediatorEvent event = { TalkMediatorEvent::NOTIFICATION_RECEIVED };
  channel_->NotifyListeners(event);
}

void TalkMediatorImpl::OnNotificationSent() {
  LOG(INFO) <<
      "P2P: Peers were notified that updates are available on the server.";
  MutexLock lock(&mutex_);
  TalkMediatorEvent event = { TalkMediatorEvent::NOTIFICATION_SENT };
  channel_->NotifyListeners(event);
}

}  // namespace browser_sync
