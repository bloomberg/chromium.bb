// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_LISTENER_PUSH_CLIENT_H_
#define JINGLE_NOTIFIER_LISTENER_PUSH_CLIENT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/notification_defines.h"

namespace base {
class MessageLoopProxy;
} // namespace base

namespace buzz {
class XmppTaskParentInterface;
}  // namespace buzz

namespace notifier {

// This class implements a client for the XMPP google:push protocol.
//
// This class must be used on a single thread.
class PushClient {
 public:
  // An Observer is sent messages whenever a notification is received
  // or when the state of the push client changes.
  class Observer {
   public:
    // Called when the state of the push client changes.  If
    // |notifications_enabled| is true, that means notifications can
    // be sent and received freely.  If it is false, that means no
    // notifications can be sent or received.
    virtual void OnNotificationStateChange(bool notifications_enabled) = 0;

    // Called when a notification is received.  The details of the
    // notification are in |notification|.
    virtual void OnIncomingNotification(const Notification& notification) = 0;

   protected:
    virtual ~Observer();
  };

  explicit PushClient(const NotifierOptions& notifier_options);
  ~PushClient();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Takes effect only on the next (re-)connection.  Therefore, you
  // probably want to call this before UpdateCredentials().
  void UpdateSubscriptions(const SubscriptionList& subscriptions);

  // If not connected, connects with the given credentials.  If
  // already connected, the next connection attempt will use the given
  // credentials.
  void UpdateCredentials(const std::string& email, const std::string& token);

  // Sends a notification.  Can be called when notifications are
  // disabled; the notification will be sent when notifications become
  // enabled.
  void SendNotification(const Notification& notification);

  void SimulateOnNotificationReceivedForTest(
      const Notification& notification);

  void SimulateConnectAndSubscribeForTest(
      base::WeakPtr<buzz::XmppTaskParentInterface> base_task);

  void SimulateDisconnectForTest();

  void SimulateSubscriptionErrorForTest();

  // Any notifications sent after this is called will be reflected,
  // i.e. will be treated as an incoming notification also.
  void ReflectSentNotificationsForTest();

 private:
  class Core;

  // The real guts of PushClient, which allows this class to not be
  // refcounted.
  const scoped_refptr<Core> core_;
  const scoped_refptr<base::MessageLoopProxy> parent_message_loop_proxy_;
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(PushClient);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_PUSH_CLIENT_H_
