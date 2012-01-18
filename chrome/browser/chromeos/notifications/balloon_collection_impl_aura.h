// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_AURA_H_
#define CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_AURA_H_
#pragma once

#include <set>

#include "chrome/browser/chromeos/notifications/balloon_view_host.h"  // MessageCallback
#include "chrome/browser/notifications/balloon_collection_impl.h"

namespace chromeos {

// Wrapper on top of ::BalloonCollectionImpl to provide an interface for
// chromeos::SystemNotification.
class BalloonCollectionImplAura : public ::BalloonCollectionImpl {
 public:
  BalloonCollectionImplAura() {}

  // Adds a callback for WebUI message. Returns true if the callback
  // is succssfully registered, or false otherwise. It fails to add if
  // there is no notification that matches NotificationDelegate::id(),
  // or a callback for given message already exists. The callback
  // object is owned and deleted by callee.
  bool AddWebUIMessageCallback(
      const Notification& notification,
      const std::string& message,
      const BalloonViewHost::MessageCallback& callback);

  // Adds a new system notification.
  // |sticky| is ignored in the Aura implementation; desktop notifications
  // are always sticky (i.e. they need to be dismissed explicitly).
  void AddSystemNotification(const Notification& notification,
                             Profile* profile,
                             bool sticky);

  // Updates the notification's content. It uses
  // NotificationDelegate::id() to check the equality of notifications.
  // Returns true if the notification has been updated. False if
  // no corresponding notification is found. This will not change the
  // visibility of the notification.
  bool UpdateNotification(const Notification& notification);

  // On Aura this behaves the same as UpdateNotification.
  bool UpdateAndShowNotification(const Notification& notification);

 protected:
  // Creates a new balloon. Overridable by unit tests.  The caller is
  // responsible for freeing the pointer returned.
  virtual Balloon* MakeBalloon(const Notification& notification,
                               Profile* profile);

 private:
  // Set of unique ids associated with system notifications, used by
  // MakeBalloon to determine whether or not to enable Web UI.
  std::set<std::string> system_notifications_;

  DISALLOW_COPY_AND_ASSIGN(BalloonCollectionImplAura);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_AURA_H_
