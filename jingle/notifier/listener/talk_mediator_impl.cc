// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/talk_mediator_impl.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "jingle/notifier/base/notifier_options_util.h"

namespace notifier {

TalkMediatorImpl::TalkMediatorImpl(
    MediatorThread* mediator_thread,
    const NotifierOptions& notifier_options)
    : delegate_(NULL),
      mediator_thread_(mediator_thread),
      notifier_options_(notifier_options),
      parent_message_loop_(MessageLoop::current()) {
  mediator_thread_->Start();
  state_.started = 1;
}

TalkMediatorImpl::~TalkMediatorImpl() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  DCHECK(!state_.started);
}

bool TalkMediatorImpl::Login() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  // Connect to the mediator thread and start processing messages.
  mediator_thread_->AddObserver(this);
  if (state_.initialized && !state_.logging_in && !state_.logged_in) {
    state_.logging_in = true;
    mediator_thread_->Login(xmpp_settings_);
    return true;
  }
  return false;
}

bool TalkMediatorImpl::Logout() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  if (state_.started) {
    state_.started = 0;
    state_.logging_in = 0;
    state_.logged_in = 0;
    // We do not want to be called back during logout since we may be
    // closing.
    mediator_thread_->RemoveObserver(this);
    mediator_thread_->Logout();
    return true;
  }
  return false;
}

void TalkMediatorImpl::SendNotification(const Notification& data) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  mediator_thread_->SendNotification(data);
}

void TalkMediatorImpl::SetDelegate(TalkMediator::Delegate* delegate) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  delegate_ = delegate;
}

void TalkMediatorImpl::SetAuthToken(const std::string& email,
                                    const std::string& token,
                                    const std::string& token_service) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  xmpp_settings_ =
      MakeXmppClientSettings(notifier_options_, email, token, token_service);

  // The auth token got updated and we are already in the logging_in or
  // logged_in state. Update the token.
  if (state_.logging_in || state_.logged_in) {
    mediator_thread_->UpdateXmppSettings(xmpp_settings_);
  }

  state_.initialized = 1;
}

void TalkMediatorImpl::AddSubscription(const Subscription& subscription) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  subscriptions_.push_back(subscription);
  if (state_.logged_in) {
    VLOG(1) << "Resubscribing for updates, a new service got added";
    mediator_thread_->SubscribeForUpdates(subscriptions_);
  }
}


void TalkMediatorImpl::OnConnectionStateChange(bool logged_in) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  // If we just lost connection, then the MediatorThread implementation will
  // try to log in again. We need to set state_.logging_in to true in that case.
  state_.logging_in = !logged_in;
  state_.logged_in = logged_in;
  if (logged_in) {
    VLOG(1) << "P2P: Logged in.";
    // ListenForUpdates enables the ListenTask.  This is done before
    // SubscribeForUpdates.
    mediator_thread_->ListenForUpdates();
    // Now subscribe for updates to all the services we are interested in
    mediator_thread_->SubscribeForUpdates(subscriptions_);
  } else {
    VLOG(1) << "P2P: Logged off.";
    OnSubscriptionStateChange(false);
  }
}

void TalkMediatorImpl::OnSubscriptionStateChange(bool subscribed) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  VLOG(1) << "P2P: " << (subscribed ? "subscribed" : "unsubscribed");
  if (delegate_)
    delegate_->OnNotificationStateChange(subscribed);
}

void TalkMediatorImpl::OnIncomingNotification(
    const Notification& notification) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  VLOG(1) << "P2P: Updates are available on the server.";
  if (delegate_)
    delegate_->OnIncomingNotification(notification);
}

void TalkMediatorImpl::OnOutgoingNotification() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  VLOG(1) << "P2P: Peers were notified that updates are available on the "
             "server.";
  if (delegate_)
    delegate_->OnOutgoingNotification();
}

}  // namespace notifier
