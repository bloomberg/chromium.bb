// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/mediator_thread_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/observer_list_threadsafe.h"
#include "base/threading/thread.h"
#include "jingle/notifier/base/const_communicator.h"
#include "jingle/notifier/base/notifier_options_util.h"
#include "jingle/notifier/base/task_pump.h"
#include "jingle/notifier/communicator/login.h"
#include "jingle/notifier/listener/push_notifications_listen_task.h"
#include "jingle/notifier/listener/push_notifications_send_update_task.h"
#include "jingle/notifier/listener/push_notifications_subscribe_task.h"
#include "net/base/host_port_pair.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace notifier {

class MediatorThreadImpl::Core
    : public base::RefCountedThreadSafe<MediatorThreadImpl::Core>,
      public LoginDelegate,
      public PushNotificationsListenTaskDelegate,
      public PushNotificationsSubscribeTaskDelegate {
 public:
  // Invoked on the caller thread.
  explicit Core(const NotifierOptions& notifier_options);
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Login::Delegate implementation. Called on I/O thread.
  virtual void OnConnect(
      base::WeakPtr<buzz::XmppTaskParentInterface> base_task);
  virtual void OnDisconnect();

  // PushNotificationsListenTaskDelegate implementation. Called on I/O thread.
  virtual void OnNotificationReceived(
      const Notification& notification);
  // PushNotificationsSubscribeTaskDelegate implementation. Called on I/O
  // thread.
  virtual void OnSubscribed();
  virtual void OnSubscriptionError();

  // Helpers invoked on I/O thread.
  void Login(const buzz::XmppClientSettings& settings);
  void Disconnect();
  void ListenForPushNotifications();
  void SubscribeForPushNotifications(
      const SubscriptionList& subscriptions);
  void SendNotification(const Notification& data);
  void UpdateXmppSettings(const buzz::XmppClientSettings& settings);

 private:
  friend class base::RefCountedThreadSafe<MediatorThreadImpl::Core>;
  // Invoked on either the caller thread or the I/O thread.
  virtual ~Core();
  scoped_refptr<ObserverListThreadSafe<Observer> > observers_;
  base::WeakPtr<buzz::XmppTaskParentInterface> base_task_;

  const NotifierOptions notifier_options_;

  scoped_ptr<notifier::Login> login_;

  std::vector<Notification> pending_notifications_to_send_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

MediatorThreadImpl::Core::Core(
    const NotifierOptions& notifier_options)
    : observers_(new ObserverListThreadSafe<Observer>()),
      notifier_options_(notifier_options) {
  DCHECK(notifier_options_.request_context_getter);
}

MediatorThreadImpl::Core::~Core() {
}

void MediatorThreadImpl::Core::AddObserver(Observer* observer) {
  observers_->AddObserver(observer);
}

void MediatorThreadImpl::Core::RemoveObserver(Observer* observer) {
  observers_->RemoveObserver(observer);
}

void MediatorThreadImpl::Core::Login(const buzz::XmppClientSettings& settings) {
  DCHECK(notifier_options_.request_context_getter->GetIOMessageLoopProxy()->
      BelongsToCurrentThread());
  VLOG(1) << "P2P: Thread logging into talk network.";

  base_task_.reset();
  login_.reset(new notifier::Login(this,
                                   settings,
                                   notifier_options_.request_context_getter,
                                   GetServerList(notifier_options_),
                                   notifier_options_.try_ssltcp_first,
                                   notifier_options_.auth_mechanism));
  login_->StartConnection();
}

void MediatorThreadImpl::Core::Disconnect() {
  DCHECK(notifier_options_.request_context_getter->GetIOMessageLoopProxy()->
      BelongsToCurrentThread());
  VLOG(1) << "P2P: Thread logging out of talk network.";
  login_.reset();
  base_task_.reset();
}

void MediatorThreadImpl::Core::ListenForPushNotifications() {
  DCHECK(notifier_options_.request_context_getter->GetIOMessageLoopProxy()->
      BelongsToCurrentThread());
  if (!base_task_.get())
    return;
  PushNotificationsListenTask* listener =
      new PushNotificationsListenTask(base_task_, this);
  listener->Start();
}

void MediatorThreadImpl::Core::SubscribeForPushNotifications(
    const SubscriptionList& subscriptions) {
  DCHECK(notifier_options_.request_context_getter->GetIOMessageLoopProxy()->
      BelongsToCurrentThread());
  if (!base_task_.get())
    return;
  PushNotificationsSubscribeTask* subscribe_task =
      new PushNotificationsSubscribeTask(base_task_, subscriptions, this);
  subscribe_task->Start();
}

void MediatorThreadImpl::Core::OnSubscribed() {
  DCHECK(notifier_options_.request_context_getter->GetIOMessageLoopProxy()->
      BelongsToCurrentThread());
  observers_->Notify(&Observer::OnSubscriptionStateChange, true);
}

void MediatorThreadImpl::Core::OnSubscriptionError() {
  DCHECK(notifier_options_.request_context_getter->GetIOMessageLoopProxy()->
      BelongsToCurrentThread());
  observers_->Notify(&Observer::OnSubscriptionStateChange, false);
}

void MediatorThreadImpl::Core::OnNotificationReceived(
    const Notification& notification) {
  DCHECK(notifier_options_.request_context_getter->GetIOMessageLoopProxy()->
      BelongsToCurrentThread());
  observers_->Notify(&Observer::OnIncomingNotification, notification);
}

void MediatorThreadImpl::Core::SendNotification(const Notification& data) {
  DCHECK(notifier_options_.request_context_getter->GetIOMessageLoopProxy()->
      BelongsToCurrentThread());
  if (!base_task_.get()) {
    VLOG(1) << "P2P: Cannot send notification " << data.ToString()
            << "; sending later";
    pending_notifications_to_send_.push_back(data);
    return;
  }
  // Owned by |base_task_|.
  PushNotificationsSendUpdateTask* task =
      new PushNotificationsSendUpdateTask(base_task_, data);
  task->Start();
  observers_->Notify(&Observer::OnOutgoingNotification);
}

void MediatorThreadImpl::Core::UpdateXmppSettings(
    const buzz::XmppClientSettings& settings) {
  DCHECK(notifier_options_.request_context_getter->GetIOMessageLoopProxy()->
      BelongsToCurrentThread());
  VLOG(1) << "P2P: Thread Updating login settings.";
  // The caller should only call UpdateXmppSettings after a Login call.
  if (login_.get())
    login_->UpdateXmppSettings(settings);
  else
    NOTREACHED() <<
        "P2P: Thread UpdateXmppSettings called when login_ was NULL";
}

void MediatorThreadImpl::Core::OnConnect(
    base::WeakPtr<buzz::XmppTaskParentInterface> base_task) {
  DCHECK(notifier_options_.request_context_getter->GetIOMessageLoopProxy()->
      BelongsToCurrentThread());
  base_task_ = base_task;
  observers_->Notify(&Observer::OnConnectionStateChange, true);
  std::vector<Notification> notifications_to_send;
  notifications_to_send.swap(pending_notifications_to_send_);
  for (std::vector<Notification>::const_iterator it =
           notifications_to_send.begin();
       it != notifications_to_send.end(); ++it) {
    VLOG(1) << "P2P: Sending pending notification " << it->ToString();
    SendNotification(*it);
  }
}

void MediatorThreadImpl::Core::OnDisconnect() {
  DCHECK(notifier_options_.request_context_getter->GetIOMessageLoopProxy()->
      BelongsToCurrentThread());
  base_task_.reset();
  observers_->Notify(&Observer::OnConnectionStateChange, false);
}


MediatorThreadImpl::MediatorThreadImpl(const NotifierOptions& notifier_options)
    : core_(new Core(notifier_options)),
      parent_message_loop_proxy_(
          base::MessageLoopProxy::current()),
      io_message_loop_proxy_(
          notifier_options.request_context_getter->GetIOMessageLoopProxy()) {
}

MediatorThreadImpl::~MediatorThreadImpl() {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  LogoutImpl();
}

void MediatorThreadImpl::AddObserver(Observer* observer) {
    DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  core_->AddObserver(observer);
}

void MediatorThreadImpl::RemoveObserver(Observer* observer) {
    DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  core_->RemoveObserver(observer);
}

void MediatorThreadImpl::Start() {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
}

void MediatorThreadImpl::Login(const buzz::XmppClientSettings& settings) {
    DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&MediatorThreadImpl::Core::Login, core_.get(), settings));
}

void MediatorThreadImpl::Logout() {
    DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  LogoutImpl();
}

void MediatorThreadImpl::ListenForUpdates() {
    DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&MediatorThreadImpl::Core::ListenForPushNotifications,
                 core_.get()));
}

void MediatorThreadImpl::SubscribeForUpdates(
    const SubscriptionList& subscriptions) {
    DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&MediatorThreadImpl::Core::SubscribeForPushNotifications,
                 core_.get(), subscriptions));
}

void MediatorThreadImpl::SendNotification(
    const Notification& data) {
    DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&MediatorThreadImpl::Core::SendNotification, core_.get(),
                 data));
}

void MediatorThreadImpl::UpdateXmppSettings(
    const buzz::XmppClientSettings& settings) {
    DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&MediatorThreadImpl::Core::UpdateXmppSettings, core_.get(),
                 settings));
}

void MediatorThreadImpl::TriggerOnConnectForTest(
    base::WeakPtr<buzz::XmppTaskParentInterface> base_task) {
    DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&MediatorThreadImpl::Core::OnConnect, core_.get(), base_task));
}

void MediatorThreadImpl::LogoutImpl() {
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&MediatorThreadImpl::Core::Disconnect, core_.get()));
}

}  // namespace notifier
