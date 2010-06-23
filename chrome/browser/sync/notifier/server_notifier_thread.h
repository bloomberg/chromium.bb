// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class is the (hackish) way to use the XMPP parts of
// MediatorThread for server-issued notifications.
//
// TODO(akalin): Decomp MediatorThread into an XMPP service part and a
// notifications-specific part and use the XMPP service part for
// server-issued notifications.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_SERVER_NOTIFIER_THREAD_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_SERVER_NOTIFIER_THREAD_H_

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/common/net/notifier/listener/mediator_thread_impl.h"
#include "google/cacheinvalidation/invalidation-client.h"

namespace chrome_common_net {
class NetworkChangeNotifierThread;
}  // namespace chrome_common_net

namespace sync_notifier {

class ChromeInvalidationClient;

class ServerNotifierThread
    : public notifier::MediatorThreadImpl,
      public invalidation::InvalidationListener {
 public:
  explicit ServerNotifierThread(
      chrome_common_net::NetworkChangeNotifierThread*
          network_change_notifier_thread);

  virtual ~ServerNotifierThread();

  // Overridden to start listening to server notifications.
  virtual void ListenForUpdates();

  // Overridden to immediately notify the delegate that subscriptions
  // (i.e., notifications) are on.  Must be called only after a call
  // to ListenForUpdates().
  virtual void SubscribeForUpdates(
      const std::vector<std::string>& subscribed_services_list);

  // Overridden to also stop listening to server notifications.
  virtual void Logout();

  // Must not be called.
  virtual void SendNotification(const OutgoingNotificationData& data);

  // invalidation::InvalidationListener implementation.

  virtual void Invalidate(const invalidation::Invalidation& invalidation,
                          invalidation::Closure* callback);

  virtual void InvalidateAll(invalidation::Closure* callback);

  virtual void AllRegistrationsLost(invalidation::Closure* callback);

  virtual void RegistrationLost(const invalidation::ObjectId& object_id,
                                invalidation::Closure* callback);

 private:
  // Posted to the worker thread by ListenForUpdates().
  void StartInvalidationListener();

  // Posted to the worker thread by SubscribeForUpdates().
  void RegisterTypesAndSignalSubscribed();

  // Register the sync types that we're interested in getting
  // notifications for.
  void RegisterTypes();

  // Called when we get a registration response back.
  void RegisterCallback(
      const invalidation::RegistrationUpdateResult& result);

  // Signal to the delegate that we're subscribed.
  void SignalSubscribed();

  // Signal to the delegate that we have an incoming notification.
  void SignalIncomingNotification();

  // Called by StartInvalidationListener() and posted to the worker
  // thread by Stop().
  void StopInvalidationListener();

  scoped_ptr<ChromeInvalidationClient> chrome_invalidation_client_;
};

}  // namespace sync_notifier

DISABLE_RUNNABLE_METHOD_REFCOUNT(sync_notifier::ServerNotifierThread);

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_SERVER_NOTIFIER_THREAD_H_
