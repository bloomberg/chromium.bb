// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/mediator_thread_impl.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "jingle/notifier/base/task_pump.h"
#include "jingle/notifier/communicator/connection_options.h"
#include "jingle/notifier/communicator/const_communicator.h"
#include "jingle/notifier/communicator/xmpp_connection_generator.h"
#include "jingle/notifier/listener/listen_task.h"
#include "jingle/notifier/listener/send_update_task.h"
#include "jingle/notifier/listener/subscribe_task.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/thread.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmppclientsettings.h"

// We manage the lifetime of notifier::MediatorThreadImpl ourselves.
DISABLE_RUNNABLE_METHOD_REFCOUNT(notifier::MediatorThreadImpl);

namespace notifier {

MediatorThreadImpl::MediatorThreadImpl(const NotifierOptions& notifier_options)
    : delegate_(NULL),
      parent_message_loop_(MessageLoop::current()),
      notifier_options_(notifier_options),
      worker_thread_("MediatorThread worker thread") {
  DCHECK(parent_message_loop_);
}

MediatorThreadImpl::~MediatorThreadImpl() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
}

void MediatorThreadImpl::SetDelegate(Delegate* delegate) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  delegate_ = delegate;
}

void MediatorThreadImpl::Start() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  // We create the worker thread as an IO thread in preparation for
  // making this use Chrome sockets.
  const base::Thread::Options options(MessageLoop::TYPE_IO, 0);
  // TODO(akalin): Make this function return a bool and remove this
  // CHECK().
  CHECK(worker_thread_.StartWithOptions(options));
  if (!notifier_options_.use_chrome_async_socket) {
    worker_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &MediatorThreadImpl::StartLibjingleThread));
  }
}

void MediatorThreadImpl::StartLibjingleThread() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  DCHECK(!notifier_options_.use_chrome_async_socket);
  socket_server_.reset(new talk_base::PhysicalSocketServer());
  libjingle_thread_.reset(new talk_base::Thread());
  talk_base::ThreadManager::SetCurrent(libjingle_thread_.get());
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &MediatorThreadImpl::PumpLibjingleLoop));
}

void MediatorThreadImpl::StopLibjingleThread() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  DCHECK(!notifier_options_.use_chrome_async_socket);
  talk_base::ThreadManager::SetCurrent(NULL);
  libjingle_thread_.reset();
  socket_server_.reset();
}

void MediatorThreadImpl::PumpLibjingleLoop() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  DCHECK(!notifier_options_.use_chrome_async_socket);
  // Pump the libjingle message loop 100ms at a time.
  if (!libjingle_thread_.get()) {
    // StopLibjingleThread() was called.
    return;
  }
  libjingle_thread_->ProcessMessages(100);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &MediatorThreadImpl::PumpLibjingleLoop));
}

void MediatorThreadImpl::Login(const buzz::XmppClientSettings& settings) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &MediatorThreadImpl::DoLogin, settings));
}

void MediatorThreadImpl::Logout() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &MediatorThreadImpl::DoDisconnect));
  if (!notifier_options_.use_chrome_async_socket) {
    worker_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &MediatorThreadImpl::StopLibjingleThread));
  }
  // TODO(akalin): Decomp this into a separate stop method.
  worker_thread_.Stop();
  // Process any messages the worker thread may be posted on our
  // thread.
  bool old_state = parent_message_loop_->NestableTasksAllowed();
  parent_message_loop_->SetNestableTasksAllowed(true);
  parent_message_loop_->RunAllPending();
  parent_message_loop_->SetNestableTasksAllowed(old_state);
  // worker_thread_ should have cleaned all this up.
  CHECK(!login_.get());
  CHECK(!pump_.get());
}

void MediatorThreadImpl::ListenForUpdates() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &MediatorThreadImpl::DoListenForUpdates));
}

void MediatorThreadImpl::SubscribeForUpdates(
    const std::vector<std::string>& subscribed_services_list) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &MediatorThreadImpl::DoSubscribeForUpdates,
          subscribed_services_list));
}

void MediatorThreadImpl::SendNotification(
    const OutgoingNotificationData& data) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &MediatorThreadImpl::DoSendNotification,
                        data));
}

MessageLoop* MediatorThreadImpl::worker_message_loop() {
  MessageLoop* current_message_loop = MessageLoop::current();
  DCHECK(current_message_loop);
  MessageLoop* worker_message_loop = worker_thread_.message_loop();
  DCHECK(worker_message_loop);
  DCHECK(current_message_loop == parent_message_loop_ ||
         current_message_loop == worker_message_loop);
  return worker_message_loop;
}

buzz::XmppClient* MediatorThreadImpl::xmpp_client() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  DCHECK(login_.get());
  buzz::XmppClient* xmpp_client = login_->xmpp_client();
  DCHECK(xmpp_client);
  return xmpp_client;
}

void MediatorThreadImpl::DoLogin(
    const buzz::XmppClientSettings& settings) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  LOG(INFO) << "P2P: Thread logging into talk network.";

  // TODO(akalin): Use an existing HostResolver from somewhere (maybe
  // the IOThread one).
  host_resolver_ =
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    NULL);

  // Start a new pump for the login.
  login_.reset();
  pump_.reset(new notifier::TaskPump());

  notifier::ServerInformation server_list[2];
  int server_list_count = 0;

  // Override the default servers with a test notification server if one was
  // provided.
  if(!notifier_options_.xmpp_host_port.host().empty()) {
    server_list[0].server = notifier_options_.xmpp_host_port;
    server_list[0].special_port_magic = false;
    server_list_count = 1;
  } else {
    // The default servers know how to serve over port 443 (that's the magic).
    server_list[0].server = net::HostPortPair("talk.google.com",
                                              notifier::kDefaultXmppPort);
    server_list[0].special_port_magic = true;
    server_list[1].server = net::HostPortPair("talkx.l.google.com",
                                              notifier::kDefaultXmppPort);
    server_list[1].special_port_magic = true;
    server_list_count = 2;
  }

  // Autodetect proxy is on by default.
  notifier::ConnectionOptions options;

  // Language is not used in the stanza so we default to |en|.
  std::string lang = "en";
  login_.reset(new notifier::Login(pump_.get(),
                                   notifier_options_.use_chrome_async_socket,
                                   settings,
                                   options,
                                   lang,
                                   host_resolver_.get(),
                                   server_list,
                                   server_list_count,
                                   // talk_base::FirewallManager* is NULL.
                                   NULL,
                                   notifier_options_.try_ssltcp_first,
                                   // Both the proxy and a non-proxy route
                                   // will be attempted.
                                   false));

  login_->SignalClientStateChange.connect(
      this, &MediatorThreadImpl::OnClientStateChangeMessage);
  login_->SignalLoginFailure.connect(
      this, &MediatorThreadImpl::OnLoginFailureMessage);
  login_->StartConnection();
}

void MediatorThreadImpl::DoDisconnect() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  LOG(INFO) << "P2P: Thread logging out of talk network.";
  login_.reset();
  // Delete the old pump while on the thread to ensure that everything is
  // cleaned-up in a predicatable manner.
  pump_.reset();

  host_resolver_ = NULL;
}

void MediatorThreadImpl::DoSubscribeForUpdates(
    const std::vector<std::string>& subscribed_services_list) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  SubscribeTask* subscription =
      new SubscribeTask(xmpp_client(), subscribed_services_list);
  subscription->SignalStatusUpdate.connect(
      this,
      &MediatorThreadImpl::OnSubscriptionStateChange);
  subscription->Start();
}

void MediatorThreadImpl::DoListenForUpdates() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  ListenTask* listener = new ListenTask(xmpp_client());
  listener->SignalUpdateAvailable.connect(
      this,
      &MediatorThreadImpl::OnIncomingNotification);
  listener->Start();
}

void MediatorThreadImpl::DoSendNotification(
    const OutgoingNotificationData& data) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  SendUpdateTask* task = new SendUpdateTask(xmpp_client(), data);
  task->SignalStatusUpdate.connect(
      this,
      &MediatorThreadImpl::OnOutgoingNotification);
  task->Start();
}

void MediatorThreadImpl::OnIncomingNotification(
    const IncomingNotificationData& notification_data) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  parent_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &MediatorThreadImpl::OnIncomingNotificationOnParentThread,
          notification_data));
}

void MediatorThreadImpl::OnIncomingNotificationOnParentThread(
    const IncomingNotificationData& notification_data) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  if (delegate_) {
    delegate_->OnIncomingNotification(notification_data);
  }
}

void MediatorThreadImpl::OnOutgoingNotification(bool success) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  parent_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &MediatorThreadImpl::OnOutgoingNotificationOnParentThread,
          success));
}

void MediatorThreadImpl::OnOutgoingNotificationOnParentThread(
    bool success) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  if (delegate_ && success) {
    delegate_->OnOutgoingNotification();
  }
}

void MediatorThreadImpl::OnLoginFailureMessage(
    const notifier::LoginFailure& failure) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  parent_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &MediatorThreadImpl::OnLoginFailureMessageOnParentThread,
          failure));
}

void MediatorThreadImpl::OnLoginFailureMessageOnParentThread(
    const notifier::LoginFailure& failure) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  if (delegate_) {
    delegate_->OnConnectionStateChange(false);
  }
}

void MediatorThreadImpl::OnClientStateChangeMessage(
    LoginConnectionState state) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  parent_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &MediatorThreadImpl::OnClientStateChangeMessageOnParentThread,
          state));
}

void MediatorThreadImpl::OnClientStateChangeMessageOnParentThread(
    LoginConnectionState state) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  switch (state) {
    case STATE_DISCONNECTED:
      LOG(INFO) << "P2P: Thread trying to connect.";
      // Maybe first time logon, maybe intermediate network disruption. Assume
      // the server went down, and lost our subscription for updates.
      if (delegate_) {
        delegate_->OnConnectionStateChange(false);
        delegate_->OnSubscriptionStateChange(false);
      }
      break;
    case STATE_CONNECTED:
      if (delegate_) {
        delegate_->OnConnectionStateChange(true);
      }
      break;
    default:
      LOG(WARNING) << "P2P: Unknown client state change.";
      break;
  }
}

void MediatorThreadImpl::OnSubscriptionStateChange(bool success) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  parent_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &MediatorThreadImpl::OnSubscriptionStateChangeOnParentThread,
          success));
}

void MediatorThreadImpl::OnSubscriptionStateChangeOnParentThread(
    bool success) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  if (delegate_) {
    delegate_->OnSubscriptionStateChange(success);
  }
}

}  // namespace notifier
