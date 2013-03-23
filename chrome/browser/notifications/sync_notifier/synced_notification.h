// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class represents the data for a single Synced Notification.
// It should map 1-1 to all the data in synced_notification_sepcifics.proto,
// and the data and render protobufs that the specifics protobuf contains.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/sync.pb.h"

namespace sync_pb {
class SyncedNotificationSpecifics;
}

namespace notifier {

class SyncedNotification {
 public:
  explicit SyncedNotification(const syncer::SyncData& sync_data);

  ~SyncedNotification();

  enum ReadState {
    kUnread = 1,
    kRead = 2,
    kDismissed = 3,
  };

  void Update(const syncer::SyncData& sync_data);

  // Here are some helper functions to get individual data parts out of a
  // SyncedNotification.
  // TODO(petewil): Add more types as we expand support for the protobuf.
  std::string title() const;
  std::string heading() const;
  std::string description() const;
  std::string app_id() const;
  std::string key() const;
  GURL origin_url() const;
  GURL app_icon_url() const;
  GURL image_url() const;
  std::string first_external_id() const;
  std::string notification_id() const;
  std::string text() const;
  ReadState read_state() const;

  bool EqualsIgnoringReadState(const SyncedNotification& other) const;
  bool IdMatches(const SyncedNotification& other) const;

  void NotificationHasBeenRead();
  void NotificationHasBeenDismissed();

  // This gets a pointer to the SyncedNotificationSpecifics part
  // of the sync data.
  sync_pb::EntitySpecifics GetEntitySpecifics() const;

 private:
  // Helper function to mark a notification as read or dismissed.
  void SetReadState(const ReadState& read_state);

  // Parsing functions to get this information out of the sync_data and into
  // our local variables.
  std::string ExtractTitle() const;
  std::string ExtractHeading() const;
  std::string ExtractDescription() const;
  std::string ExtractAppId() const;
  std::string ExtractKey() const;
  GURL ExtractOriginUrl() const;
  GURL ExtractAppIconUrl() const;
  GURL ExtractImageUrl() const;
  std::string ExtractFirstExternalId() const;
  std::string ExtractNotificationId() const;
  std::string ExtractText() const;
  ReadState ExtractReadState() const;

  sync_pb::SyncedNotificationSpecifics specifics_;

  DISALLOW_COPY_AND_ASSIGN(SyncedNotification);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_H_
