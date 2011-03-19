// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A notifier that communicates to sync servers for server-issued
// notifications.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_INVALIDATION_NOTIFIER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_INVALIDATION_NOTIFIER_H_

#include <string>

#include "base/observer_list.h"
#include "chrome/browser/sync/notifier/server_notifier_thread.h"
#include "chrome/browser/sync/notifier/state_writer.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "jingle/notifier/base/notifier_options.h"

namespace sync_notifier {

class InvalidationNotifier
    : public SyncNotifier,
      public StateWriter,
      public ServerNotifierThread::Observer {
 public:
  InvalidationNotifier(const notifier::NotifierOptions& notifier_options,
                       const std::string& client_info);

  virtual ~InvalidationNotifier();

  // SyncNotifier implementation.
  virtual void AddObserver(SyncNotifierObserver* observer);
  virtual void RemoveObserver(SyncNotifierObserver* observer);
  virtual void SetState(const std::string& state);
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token);
  virtual void UpdateEnabledTypes(const syncable::ModelTypeSet& types);
  virtual void SendNotification();

  // StateWriter implementation.
  virtual void WriteState(const std::string& state);

  // ServerNotifierThread::Observer implementation.
  virtual void OnConnectionStateChange(bool logged_in);
  virtual void OnSubscriptionStateChange(bool subscribed);
  virtual void OnIncomingNotification(
      const notifier::Notification& notification);
  virtual void OnOutgoingNotification();

 private:
  const notifier::NotifierOptions notifier_options_;
  // The actual notification listener.
  ServerNotifierThread server_notifier_thread_;
  // Whether we called Login() on |server_notifier_thread_| yet.
  bool logged_in_;

  ObserverList<SyncNotifierObserver> observer_list_;
  syncable::ModelTypeSet enabled_types_;
};

}
#endif  // CHROME_BROWSER_SYNC_NOTIFIER_INVALIDATION_NOTIFIER_H_
