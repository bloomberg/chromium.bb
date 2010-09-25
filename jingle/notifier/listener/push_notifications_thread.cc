// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/push_notifications_thread.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "jingle/notifier/listener/notification_defines.h"
#include "talk/xmpp/xmppclient.h"

namespace notifier {

PushNotificationsThread::PushNotificationsThread(
    const NotifierOptions& notifier_options, const std::string& from)
        : MediatorThreadImpl(notifier_options), from_(from) {
}

PushNotificationsThread::~PushNotificationsThread() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
}

void PushNotificationsThread::ListenForUpdates() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &PushNotificationsThread::ListenForPushNotifications));
}

void PushNotificationsThread::SubscribeForUpdates(
    const std::vector<std::string>& subscribed_services_list) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &PushNotificationsThread::SubscribeForPushNotifications,
                        subscribed_services_list));
}

void PushNotificationsThread::SendNotification(
    const OutgoingNotificationData& data) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
}

void PushNotificationsThread::ListenForPushNotifications() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  PushNotificationsListenTask* listener =
      new PushNotificationsListenTask(base_task_, this);
  listener->Start();
}

void PushNotificationsThread::SubscribeForPushNotifications(
    const std::vector<std::string>& subscribed_services_list) {
  std::vector<PushNotificationsSubscribeTask::PushSubscriptionInfo>
      channels_list;
  for (std::vector<std::string>::const_iterator iter =
           subscribed_services_list.begin();
       iter != subscribed_services_list.end(); iter++) {
    PushNotificationsSubscribeTask::PushSubscriptionInfo subscription;
    subscription.channel = *iter;
    subscription.from = from_;
    channels_list.push_back(subscription);
  }
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  PushNotificationsSubscribeTask* subscription =
      new PushNotificationsSubscribeTask(base_task_, channels_list, this);
  subscription->Start();
}

void PushNotificationsThread::OnSubscribed() {
  OnSubscriptionStateChange(true);
}

void PushNotificationsThread::OnSubscriptionError() {
  OnSubscriptionStateChange(false);
}

void PushNotificationsThread::OnNotificationReceived(
    const IncomingNotificationData& notification) {
  OnIncomingNotification(notification);
}

}  // namespace notifier

