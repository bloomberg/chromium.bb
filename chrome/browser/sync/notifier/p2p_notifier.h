// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A notifier that uses p2p notifications based on XMPP push
// notifications.  Used only for sync integration tests.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_P2P_NOTIFIER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_P2P_NOTIFIER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "jingle/notifier/listener/talk_mediator.h"

namespace base {
class MessageLoopProxy;
}


namespace sync_notifier {

// The channel to use for sync notifications.
extern const char* kSyncP2PNotificationChannel;

// The intended recipient(s) of a P2P notification.
enum P2PNotificationTarget {
  NOTIFY_SELF,
  FIRST_NOTIFICATION_TARGET = NOTIFY_SELF,
  NOTIFY_OTHERS,
  NOTIFY_ALL,
  LAST_NOTIFICATION_TARGET = NOTIFY_ALL
};

std::string P2PNotificationTargetToString(
    P2PNotificationTarget target);

// If |target_str| can't be parsed, assumes NOTIFY_SELF.
P2PNotificationTarget P2PNotificationTargetFromString(
    const std::string& target_str);

// Helper notification data class that can be serialized to and
// deserialized from a string.
class P2PNotificationData {
 public:
  // Initializes with an empty sender ID, target set to NOTIFY_SELF,
  // and empty changed types.
  P2PNotificationData();
  P2PNotificationData(const std::string& sender_id,
                      P2PNotificationTarget target,
                      const syncable::ModelTypeSet& changed_types);

  ~P2PNotificationData();

  // Returns true if the given ID is targeted by this notification.
  bool IsTargeted(const std::string& id) const;

  const syncable::ModelTypeSet& GetChangedTypes() const;

  bool Equals(const P2PNotificationData& other) const;

  std::string ToString() const;

  // Returns whether parsing |str| was successful.  If parsing was
  // unsuccessful, the state of the notification is undefined.
  bool ResetFromString(const std::string& str);

 private:
  // The unique ID of the client that sent the notification.
  std::string sender_id_;
  // The intendent recipient(s) of the notification.
  P2PNotificationTarget target_;
  // The types the notification is for.
  syncable::ModelTypeSet changed_types_;
};

class P2PNotifier
    : public SyncNotifier,
      public notifier::TalkMediator::Delegate {
 public:
  // Takes ownership of |talk_mediator|, but it is guaranteed that
  // |talk_mediator| is destroyed only when this object is destroyed.
  //
  // The |send_notification_target| parameter was added to allow us to send
  // self-notifications in some cases, but not others.  The value should be
  // either NOTIFY_ALL to send notifications to all clients, or NOTIFY_OTHERS
  // to send notificaitons to all clients except for the one that triggered the
  // notification.  See crbug.com/97780.
  P2PNotifier(notifier::TalkMediator* talk_mediator,
              P2PNotificationTarget send_notification_target);

  virtual ~P2PNotifier();

  // SyncNotifier implementation
  virtual void AddObserver(SyncNotifierObserver* observer) OVERRIDE;
  virtual void RemoveObserver(SyncNotifierObserver* observer) OVERRIDE;
  virtual void SetUniqueId(const std::string& unique_id) OVERRIDE;
  virtual void SetState(const std::string& state) OVERRIDE;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void UpdateEnabledTypes(
      const syncable::ModelTypeSet& enabled_types) OVERRIDE;
  virtual void SendNotification(
      const syncable::ModelTypeSet& changed_types) OVERRIDE;

  // TalkMediator::Delegate implementation.
  virtual void OnNotificationStateChange(bool notifications_enabled) OVERRIDE;
  virtual void OnIncomingNotification(
      const notifier::Notification& notification) OVERRIDE;
  virtual void OnOutgoingNotification() OVERRIDE;

  // For testing.
  void SendNotificationDataForTest(
      const P2PNotificationData& notification_data);

 private:
  void SendNotificationData(const P2PNotificationData& notification_data);

  ObserverList<SyncNotifierObserver> observer_list_;

  // The actual notification listener.
  scoped_ptr<notifier::TalkMediator> talk_mediator_;
  // Our unique ID.
  std::string unique_id_;
  // Whether we called Login() on |talk_mediator_| yet.
  bool logged_in_;
  // Whether |talk_mediator_| has notified us that notifications are
  // enabled.
  bool notifications_enabled_;
  // Which set of clients should be sent notifications.
  P2PNotificationTarget send_notification_target_;

  syncable::ModelTypeSet enabled_types_;
  scoped_refptr<base::MessageLoopProxy> parent_message_loop_proxy_;
};

}  // namespace sync_notifier
#endif  // CHROME_BROWSER_SYNC_NOTIFIER_P2P_NOTIFIER_H_
