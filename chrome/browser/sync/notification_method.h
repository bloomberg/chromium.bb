// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFICATION_METHOD_H_
#define CHROME_BROWSER_SYNC_NOTIFICATION_METHOD_H_
#pragma once

#include <string>

namespace browser_sync {

// This is the matrix for the interaction between clients with
// different notification methods (except for NOTIFICATION_SERVER):
//
//          Listen
//          L T N
//        +-------+
//      L | E E E |
// Send T | Y Y Y |
//      N | E Y Y |
//        +-------+
//
// 'Y' means a client listening with the column notification method
// will receive notifications from a client sending with the row
// notification method. 'E' means means that the notification will be
// an empty one, which may be dropped by the server in the future.
//
// As for NOTIFICATION_SERVER, server-issued notifications will also
// simulate a peer-issued notification, so that any client with
// NOTIFICATION_TRANSITIONAL or NOTIFICATION_NEW will be able to
// receive those, too.  This support will be removed once everyone is
// on NOTIFICATION_SERVER.

enum NotificationMethod {
  // Old, broken notification method.  Works only if notification
  // servers don't drop empty notifications.
  NOTIFICATION_LEGACY,
  // Compatible with new notifications.  Also compatible with legacy
  // notifications if the notification servers don't drop empty
  // notifications.
  NOTIFICATION_TRANSITIONAL,
  // New notification method.  Compatible only with transitional
  // notifications.
  //
  // NOTE: "New" is kind of a misnomer, as it refers only to
  // peer-issued notifications; the plan is to migrate everyone to
  // using NOTIFICATION_SERVER.
  NOTIFICATION_NEW,

  // Server-issued notifications.  Compatible only with transitional
  // notifications.
  NOTIFICATION_SERVER,
};

extern const NotificationMethod kDefaultNotificationMethod;

std::string NotificationMethodToString(
    NotificationMethod notification_method);

// If the given string is not one of "legacy", "transitional", "new",
// or "server", returns kDefaultNotificationMethod.
NotificationMethod StringToNotificationMethod(const std::string& str);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NOTIFICATION_METHOD_H_

