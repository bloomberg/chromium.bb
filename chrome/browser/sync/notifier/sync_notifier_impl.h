// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Talk Mediator based implementation of the sync notifier interface.
// Initializes the talk mediator and registers itself as the delegate.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_IMPL_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_IMPL_H_

#include <string>

#include "base/observer_list.h"
#include "chrome/browser/sync/notifier/state_writer.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/talk_mediator.h"
#include "jingle/notifier/listener/talk_mediator_impl.h"

namespace sync_notifier {
class ServerNotifierThread;

class SyncNotifierImpl
    : public SyncNotifier,
      public sync_notifier::StateWriter,
      public notifier::TalkMediator::Delegate {
 public:
  explicit SyncNotifierImpl(const notifier::NotifierOptions& notifier_options);

  virtual ~SyncNotifierImpl();

  // TalkMediator::Delegate implementation.
  virtual void OnNotificationStateChange(bool notifications_enabled);

  virtual void OnIncomingNotification(
      const notifier::Notification& notification);

  virtual void OnOutgoingNotification() {}

  // sync_notifier::StateWriter implementation.
  virtual void WriteState(const std::string& state);

  // SyncNotifier implementation
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token);

  virtual void SetState(const std::string& state);

  virtual void AddObserver(SyncNotifierObserver* observer);
  virtual void RemoveObserver(SyncNotifierObserver* observer);

  virtual void UpdateEnabledTypes(const syncable::ModelTypeSet& types);
  virtual void SendNotification();
 private:
  // Login to the talk mediator with the given credentials.
  void TalkMediatorLogin(
      const std::string& email, const std::string& token);

  // Notification (xmpp) handler.
  scoped_ptr<notifier::TalkMediator> talk_mediator_;
  syncable::ModelTypeSet enabled_types_;
  std::string state_;

  notifier::NotifierOptions notifier_options_;
  ServerNotifierThread* server_notifier_thread_;
  ObserverList<SyncNotifierObserver> observer_list_;
};
}
#endif  // CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_IMPL_H_
