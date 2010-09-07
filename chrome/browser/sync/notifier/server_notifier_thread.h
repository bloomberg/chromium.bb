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
#pragma once

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "jingle/notifier/listener/mediator_thread_impl.h"

namespace notifier {
struct NotifierOptions;
}

namespace sync_notifier {

class ServerNotifierThread
    : public notifier::MediatorThreadImpl,
      public ChromeInvalidationClient::Listener {
 public:
  explicit ServerNotifierThread(
      const notifier::NotifierOptions& notifier_options);

  virtual ~ServerNotifierThread();

  // Overridden to start listening to server notifications.
  virtual void ListenForUpdates();

  // Overridden to immediately notify the delegate that subscriptions
  // (i.e., notifications) are on.  Must be called only after a call
  // to ListenForUpdates().
  virtual void SubscribeForUpdates(
      const std::vector<std::string>& subscribed_services_list);

  // Overridden to stop listening to server notifications.
  virtual void Logout();

  // Must not be called.
  virtual void SendNotification(const OutgoingNotificationData& data);

  // ChromeInvalidationClient::Listener implementation.

  virtual void OnInvalidate(syncable::ModelType model_type);

  virtual void OnInvalidateAll();

 protected:
  // Overridden to know what state we're in.
  virtual void OnClientStateChangeMessage(
      notifier::LoginConnectionState state);

 private:
  // Posted to the worker thread by ListenForUpdates().
  void StartInvalidationListener();

  // Posted to the worker thread by SubscribeForUpdates().
  void RegisterTypesAndSignalSubscribed();

  // Signal to the delegate that we're subscribed.
  void SignalSubscribed();

  // Signal to the delegate that we have an incoming notification.
  void SignalIncomingNotification();

  // Called by StartInvalidationListener() and posted to the worker
  // thread by Stop().
  void StopInvalidationListener();

  notifier::LoginConnectionState state_;
  scoped_ptr<ChromeInvalidationClient> chrome_invalidation_client_;
};

}  // namespace sync_notifier

DISABLE_RUNNABLE_METHOD_REFCOUNT(sync_notifier::ServerNotifierThread);

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_SERVER_NOTIFIER_THREAD_H_
