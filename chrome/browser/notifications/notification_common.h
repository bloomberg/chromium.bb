// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_COMMON_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_COMMON_H_

#include "base/feature_list.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

class GURL;
class Profile;

// Shared functionality for both in page and persistent notification
class NotificationCommon {
 public:
  // Things as user can do to a notification.
  // TODO(peter): Prefix these options with OPERATION_.
  enum Operation {
    CLICK = 0,
    CLOSE = 1,
    SETTINGS = 2,
    OPERATION_MAX = SETTINGS
  };

  // Possible kinds of notifications
  // TODO(peter): Prefix these options with TYPE_.
  enum Type {
    PERSISTENT = 0,
    NON_PERSISTENT = 1,
    EXTENSION = 2,
    DOWNLOAD = 3,
    TRANSIENT = 4,  // A generic type for any notification that does not outlive
                    // the browser instance and is controlled by a
                    // NotificationDelegate.
    TYPE_MAX = DOWNLOAD,
  };

  // A struct that contains extra data about a notification specific to one of
  // the above types.
  struct Metadata {
    virtual ~Metadata();

    Type type;
  };

  // Open the Notification settings screen when clicking the right button.
  // TODO(miguelg) have it take a Profile instead once NotificationObjectProxy
  // is updated.
  static void OpenNotificationSettings(
      content::BrowserContext* browser_context);
};

// Metadata for PERSISTENT notifications.
struct PersistentNotificationMetadata : public NotificationCommon::Metadata {
  PersistentNotificationMetadata();
  ~PersistentNotificationMetadata() override;

  static const PersistentNotificationMetadata* From(const Metadata* metadata);

  GURL service_worker_scope;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_COMMON_H_
