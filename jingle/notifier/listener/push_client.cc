// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/push_client.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "jingle/notifier/base/notifier_options_util.h"
#include "jingle/notifier/communicator/login.h"
#include "jingle/notifier/listener/push_notifications_listen_task.h"
#include "jingle/notifier/listener/push_notifications_send_update_task.h"
#include "jingle/notifier/listener/push_notifications_subscribe_task.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace notifier {

PushClient::Observer::~Observer() {}

// All member functions except for the constructor, destructor, and
// {Add,Remove}Observer() must be called on the IO thread (as taken from
// |notifier_options|).
class PushClient::Core
    : public base::RefCountedThreadSafe<PushClient::Core>,
      public LoginDelegate,
      public PushNotificationsListenTaskDelegate,
      public PushNotificationsSubscribeTaskDelegate {
 public:
  // Called on the parent thread.
  explicit Core(const NotifierOptions& notifier_options);

  // Must be called before being destroyed.
  void DestroyOnIOThread();

  // Login::Delegate implementation.
  virtual void OnConnect(
      base::WeakPtr<buzz::XmppTaskParentInterface> base_task) OVERRIDE;
  virtual void OnDisconnect();

  // PushNotificationsListenTaskDelegate implementation.
  virtual void OnNotificationReceived(
      const Notification& notification) OVERRIDE;

  // PushNotificationsSubscribeTaskDelegate implementation.
  virtual void OnSubscribed() OVERRIDE;
  virtual void OnSubscriptionError() OVERRIDE;

  // Called on the parent thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void UpdateSubscriptions(const SubscriptionList& subscriptions);
  void UpdateCredentials(const std::string& email, const std::string& token);
  void SendNotification(const Notification& data);

  // Any notifications sent after this is called will be reflected,
  // i.e. will be treated as an incoming notification also.
  void ReflectSentNotificationsForTest();

 private:
  friend class base::RefCountedThreadSafe<PushClient::Core>;

  // Called on either the parent thread or the I/O thread.
  virtual ~Core();

  const NotifierOptions notifier_options_;
  const scoped_refptr<base::MessageLoopProxy> parent_message_loop_proxy_;
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  const scoped_refptr<ObserverListThreadSafe<Observer> > observers_;

  // XMPP connection settings.
  SubscriptionList subscriptions_;
  buzz::XmppClientSettings xmpp_settings_;

  // Must be created/used/destroyed only on the IO thread.
  scoped_ptr<notifier::Login> login_;

  // The XMPP connection.
  base::WeakPtr<buzz::XmppTaskParentInterface> base_task_;

  std::vector<Notification> pending_notifications_to_send_;

  bool reflect_sent_notifications_for_test_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

PushClient::Core::Core(const NotifierOptions& notifier_options)
    : notifier_options_(notifier_options),
      parent_message_loop_proxy_(base::MessageLoopProxy::current()),
      io_message_loop_proxy_(
          notifier_options_.request_context_getter->GetIOMessageLoopProxy()),
      observers_(new ObserverListThreadSafe<Observer>()),
      reflect_sent_notifications_for_test_(false) {}

PushClient::Core::~Core() {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread() ||
         io_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!login_.get());
  DCHECK(!base_task_.get());
  observers_->AssertEmpty();
}

void PushClient::Core::DestroyOnIOThread() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  login_.reset();
  base_task_.reset();
}

void PushClient::Core::OnConnect(
    base::WeakPtr<buzz::XmppTaskParentInterface> base_task) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  base_task_ = base_task;

  if (!base_task_.get()) {
    NOTREACHED();
    return;
  }

  // Listen for notifications.
  {
    // Owned by |base_task_|.
    PushNotificationsListenTask* listener =
        new PushNotificationsListenTask(base_task_, this);
    listener->Start();
  }

  // Send subscriptions.
  {
    // Owned by |base_task_|.
    PushNotificationsSubscribeTask* subscribe_task =
        new PushNotificationsSubscribeTask(base_task_, subscriptions_, this);
    subscribe_task->Start();
  }

  std::vector<Notification> notifications_to_send;
  notifications_to_send.swap(pending_notifications_to_send_);
  for (std::vector<Notification>::const_iterator it =
           notifications_to_send.begin();
       it != notifications_to_send.end(); ++it) {
    DVLOG(1) << "Push: Sending pending notification " << it->ToString();
    SendNotification(*it);
  }
}

void PushClient::Core::OnDisconnect() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  base_task_.reset();
  observers_->Notify(&Observer::OnNotificationStateChange, false);
}

void PushClient::Core::OnNotificationReceived(
    const Notification& notification) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  observers_->Notify(&Observer::OnIncomingNotification, notification);
}

void PushClient::Core::OnSubscribed() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  observers_->Notify(&Observer::OnNotificationStateChange, true);
}

void PushClient::Core::OnSubscriptionError() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  observers_->Notify(&Observer::OnNotificationStateChange, false);
}

void PushClient::Core::AddObserver(Observer* observer) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  observers_->AddObserver(observer);
}

void PushClient::Core::RemoveObserver(Observer* observer) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  observers_->RemoveObserver(observer);
}

void PushClient::Core::UpdateSubscriptions(
    const SubscriptionList& subscriptions) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  subscriptions_ = subscriptions;
}

void PushClient::Core::UpdateCredentials(
      const std::string& email, const std::string& token) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  DVLOG(1) << "Push: Updating credentials for " << email;
  xmpp_settings_ = MakeXmppClientSettings(notifier_options_, email, token);
  if (login_.get()) {
    login_->UpdateXmppSettings(xmpp_settings_);
  } else {
    DVLOG(1) << "Push: Starting XMPP connection";
    base_task_.reset();
    login_.reset(new notifier::Login(this,
                                     xmpp_settings_,
                                     notifier_options_.request_context_getter,
                                     GetServerList(notifier_options_),
                                     notifier_options_.try_ssltcp_first,
                                     notifier_options_.auth_mechanism));
    login_->StartConnection();
  }
}

void PushClient::Core::SendNotification(const Notification& notification) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  if (!base_task_.get()) {
    DVLOG(1) << "Push: Cannot send notification "
             << notification.ToString() << "; sending later";
    pending_notifications_to_send_.push_back(notification);
    return;
  }
  // Owned by |base_task_|.
  PushNotificationsSendUpdateTask* task =
      new PushNotificationsSendUpdateTask(base_task_, notification);
  task->Start();

  if (reflect_sent_notifications_for_test_) {
    OnNotificationReceived(notification);
  }
}

void PushClient::Core::ReflectSentNotificationsForTest() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  reflect_sent_notifications_for_test_ = true;
}

PushClient::PushClient(const NotifierOptions& notifier_options)
    : core_(new Core(notifier_options)),
      parent_message_loop_proxy_(base::MessageLoopProxy::current()),
      io_message_loop_proxy_(
          notifier_options.request_context_getter->GetIOMessageLoopProxy()) {
}

PushClient::~PushClient() {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PushClient::Core::DestroyOnIOThread, core_.get()));
}

void PushClient::AddObserver(Observer* observer) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  core_->AddObserver(observer);
}

void PushClient::RemoveObserver(Observer* observer) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  core_->RemoveObserver(observer);
}

void PushClient::UpdateSubscriptions(const SubscriptionList& subscriptions) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PushClient::Core::UpdateSubscriptions,
                 core_.get(), subscriptions));
}

void PushClient::UpdateCredentials(
    const std::string& email, const std::string& token) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PushClient::Core::UpdateCredentials,
                 core_.get(), email, token));
}

void PushClient::SendNotification(const Notification& notification) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PushClient::Core::SendNotification, core_.get(),
                 notification));
}

void PushClient::SimulateOnNotificationReceivedForTest(
    const Notification& notification) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PushClient::Core::OnNotificationReceived,
                 core_.get(), notification));
}

void PushClient::SimulateConnectAndSubscribeForTest(
    base::WeakPtr<buzz::XmppTaskParentInterface> base_task) {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PushClient::Core::OnConnect, core_.get(), base_task));
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PushClient::Core::OnSubscribed, core_.get()));
}

void PushClient::SimulateDisconnectForTest() {
  DCHECK(parent_message_loop_proxy_->BelongsToCurrentThread());
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PushClient::Core::OnDisconnect, core_.get()));
}

void PushClient::SimulateSubscriptionErrorForTest() {
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PushClient::Core::OnSubscriptionError, core_.get()));
}

void PushClient::ReflectSentNotificationsForTest() {
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PushClient::Core::ReflectSentNotificationsForTest,
                 core_.get()));
}

}  // namespace notifier
