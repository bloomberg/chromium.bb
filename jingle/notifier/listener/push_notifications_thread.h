// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementaion of MediatorThreadImpl to subscribe and listen for
// notifications using the new Google Push notifications service.

#ifndef JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_THREAD_H_
#define JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_THREAD_H_

#include <string>
#include <vector>

#include "jingle/notifier/listener/mediator_thread_impl.h"
#include "jingle/notifier/listener/push_notifications_listen_task.h"
#include "jingle/notifier/listener/push_notifications_subscribe_task.h"

namespace notifier {

class PushNotificationsThread
    : public MediatorThreadImpl,
      public PushNotificationsListenTaskDelegate,
      public PushNotificationsSubscribeTaskDelegate {
 public:
  explicit PushNotificationsThread(
    const NotifierOptions& notifier_options, const std::string& from);
  virtual ~PushNotificationsThread();

  // MediatorThreadImpl overrides.
  virtual void ListenForUpdates();
  // TODO(sanjeevr): Because we are using the MediatorThread interface which
  // was designed for the old notifications scheme, this class is limited to
  // subscribing to multiple channels from the same source (the |from| c'tor
  // argument). We need to extend the interface to handle the new push
  // notifications service.
  virtual void SubscribeForUpdates(
      const std::vector<std::string>& subscribed_services_list);
  virtual void SendNotification(const OutgoingNotificationData& data);

  // PushNotificationsListenTaskDelegate implementation.
  virtual void OnNotificationReceived(
      const IncomingNotificationData& notification);
  // PushNotificationsSubscribeTaskDelegate implementation.
  virtual void OnSubscribed();
  virtual void OnSubscriptionError();

 private:
  // Helpers invoked on worker thread.
  void ListenForPushNotifications();
  void SubscribeForPushNotifications(
      const std::vector<std::string>& subscribed_services_list);

  std::string from_;

  DISALLOW_COPY_AND_ASSIGN(PushNotificationsThread);
};

}  // namespace notifier

DISABLE_RUNNABLE_METHOD_REFCOUNT(notifier::PushNotificationsThread);

#endif  // JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_THREAD_H_

