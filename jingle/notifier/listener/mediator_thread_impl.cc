// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/mediator_thread_impl.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "jingle/notifier/base/task_pump.h"
#include "jingle/notifier/communicator/connection_options.h"
#include "jingle/notifier/communicator/const_communicator.h"
#include "jingle/notifier/communicator/xmpp_connection_generator.h"
#include "jingle/notifier/listener/listen_task.h"
#include "jingle/notifier/listener/send_update_task.h"
#include "jingle/notifier/listener/subscribe_task.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace notifier {

MediatorThreadImpl::MediatorThreadImpl(const NotifierOptions& notifier_options)
    : observers_(new ObserverListThreadSafe<Observer>()),
      parent_message_loop_(MessageLoop::current()),
      notifier_options_(notifier_options),
      worker_thread_("MediatorThread worker thread") {
  DCHECK(parent_message_loop_);
}

MediatorThreadImpl::~MediatorThreadImpl() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  // If the worker thread is still around, we need to call Logout() so
  // that all the variables living it get destroyed properly (i.e., on
  // the worker thread).
  if (worker_thread_.IsRunning()) {
    Logout();
  }
}

void MediatorThreadImpl::AddObserver(Observer* observer) {
  observers_->AddObserver(observer);
}

void MediatorThreadImpl::RemoveObserver(Observer* observer) {
  observers_->RemoveObserver(observer);
}

void MediatorThreadImpl::Start() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  // We create the worker thread as an IO thread in preparation for
  // making this use Chrome sockets.
  const base::Thread::Options options(MessageLoop::TYPE_IO, 0);
  // TODO(akalin): Make this function return a bool and remove this
  // CHECK().
  CHECK(worker_thread_.StartWithOptions(options));
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
  // TODO(akalin): Decomp this into a separate stop method.
  worker_thread_.Stop();
  // worker_thread_ should have cleaned this up.
  CHECK(!login_.get());
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


void MediatorThreadImpl::DoLogin(
    const buzz::XmppClientSettings& settings) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  VLOG(1) << "P2P: Thread logging into talk network.";

  base_task_.reset();

  // TODO(akalin): Use an existing HostResolver from somewhere (maybe
  // the IOThread one).
  // TODO(wtc): Sharing HostResolver and CertVerifier with the IO Thread won't
  // work because HostResolver and CertVerifier are single-threaded.
  host_resolver_.reset(
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    NULL, NULL));
  cert_verifier_.reset(new net::CertVerifier);

  notifier::ServerInformation server_list[2];
  int server_list_count = 0;

  // Override the default servers with a test notification server if one was
  // provided.
  if (!notifier_options_.xmpp_host_port.host().empty()) {
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

  login_.reset(new notifier::Login(this,
                                   settings,
                                   options,
                                   host_resolver_.get(),
                                   cert_verifier_.get(),
                                   server_list,
                                   server_list_count,
                                   notifier_options_.try_ssltcp_first));
  login_->StartConnection();
}

void MediatorThreadImpl::DoDisconnect() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  VLOG(1) << "P2P: Thread logging out of talk network.";
  login_.reset();
  cert_verifier_.reset();
  host_resolver_.reset();
  base_task_.reset();
}

void MediatorThreadImpl::DoSubscribeForUpdates(
    const std::vector<std::string>& subscribed_services_list) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  if (!base_task_.get()) {
    return;
  }
  // Owned by |base_task_|.
  SubscribeTask* subscription =
      new SubscribeTask(base_task_, subscribed_services_list);
  subscription->SignalStatusUpdate.connect(
      this,
      &MediatorThreadImpl::OnSubscriptionStateChange);
  subscription->Start();
}

void MediatorThreadImpl::DoListenForUpdates() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  if (!base_task_.get()) {
    return;
  }
  // Owned by |base_task_|.
  ListenTask* listener = new ListenTask(base_task_);
  listener->SignalUpdateAvailable.connect(
      this,
      &MediatorThreadImpl::OnIncomingNotification);
  listener->Start();
}

void MediatorThreadImpl::DoSendNotification(
    const OutgoingNotificationData& data) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  if (!base_task_.get()) {
    return;
  }
  // Owned by |base_task_|.
  SendUpdateTask* task = new SendUpdateTask(base_task_, data);
  task->SignalStatusUpdate.connect(
      this,
      &MediatorThreadImpl::OnOutgoingNotification);
  task->Start();
}

void MediatorThreadImpl::OnIncomingNotification(
    const IncomingNotificationData& notification_data) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  observers_->Notify(&Observer::OnIncomingNotification, notification_data);
}

void MediatorThreadImpl::OnOutgoingNotification(bool success) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  if (success) {
    observers_->Notify(&Observer::OnOutgoingNotification);
  }
}

void MediatorThreadImpl::OnConnect(base::WeakPtr<talk_base::Task> base_task) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  base_task_ = base_task;
  observers_->Notify(&Observer::OnConnectionStateChange, true);
}

void MediatorThreadImpl::OnDisconnect() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  base_task_.reset();
  observers_->Notify(&Observer::OnConnectionStateChange, false);
}

void MediatorThreadImpl::OnSubscriptionStateChange(bool success) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  observers_->Notify(&Observer::OnSubscriptionStateChange, success);
}

}  // namespace notifier
