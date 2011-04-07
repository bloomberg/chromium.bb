// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A notifier that uses p2p notifications based on XMPP push
// notifications.  Used only for sync integration tests.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_P2P_NOTIFIER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_P2P_NOTIFIER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "jingle/notifier/listener/talk_mediator.h"

namespace base {
class MessageLoopProxy;
}


namespace notifier {
struct NotifierOptions;
}  // namespace

namespace sync_notifier {

class P2PNotifier
    : public SyncNotifier,
      public notifier::TalkMediator::Delegate {
 public:
  explicit P2PNotifier(const notifier::NotifierOptions& notifier_options);

  virtual ~P2PNotifier();

  // SyncNotifier implementation
  virtual void AddObserver(SyncNotifierObserver* observer);
  virtual void RemoveObserver(SyncNotifierObserver* observer);
  virtual void SetState(const std::string& state);
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token);
  virtual void UpdateEnabledTypes(const syncable::ModelTypeSet& types);
  virtual void SendNotification();

  // TalkMediator::Delegate implementation.
  virtual void OnNotificationStateChange(bool notifications_enabled);
  virtual void OnIncomingNotification(
      const notifier::Notification& notification);
  virtual void OnOutgoingNotification();

 private:
  // Call OnIncomingNotification() on observers if we have a non-empty
  // set of enabled types.
  void MaybeEmitNotification();
  void CheckOrSetValidThread();

  ObserverList<SyncNotifierObserver> observer_list_;

  // The actual notification listener.
  scoped_ptr<notifier::TalkMediator> talk_mediator_;
  // Whether we called Login() on |talk_mediator_| yet.
  bool logged_in_;
  // Whether |talk_mediator_| has notified us that notifications are
  // enabled.
  bool notifications_enabled_;

  syncable::ModelTypeSet enabled_types_;
  scoped_refptr<base::MessageLoopProxy> construction_message_loop_proxy_;
  scoped_refptr<base::MessageLoopProxy> method_message_loop_proxy_;
};

}  // namespace sync_notifier
#endif  // CHROME_BROWSER_SYNC_NOTIFIER_P2P_NOTIFIER_H_
