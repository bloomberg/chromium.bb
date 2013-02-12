// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class represents the data for a single Synced Notification.
// It should map 1-1 to all the data in synced_notification_sepcifics.proto,
// and the data and render protobufs that the specifics protobuf contains.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_H_

#include <string>

#include "googleurl/src/gurl.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/synced_notification_specifics.pb.h"


namespace notifier {

class SyncedNotification {
 public:
  explicit SyncedNotification(const syncer::SyncData& sync_data);

  ~SyncedNotification();

  // Here are some helper functions to get individual data parts out of a
  // SyncedNotification.
  const std::string& title() const;
  const std::string& app_id() const;
  const std::string& coalescing_key() const;
  const GURL& origin_url() const;
  const GURL& icon_url() const;
  const GURL& image_url() const;
  const std::string& first_external_id() const;
  const std::string& notification_id() const;
  const std::string& body() const;

  bool Equals(const SyncedNotification& other) const;

  void set_has_local_changes(bool has_local_changes) {
    has_local_changes_ = has_local_changes;
  }

  bool has_local_changes() const {
    return has_local_changes_;
  }

  const syncer::SyncData* sync_data() const {
    return &sync_data_;
  }

  void NotificationHasBeenRead();

  void NotificationHasBeenDeleted();

  // This gets a pointer to the SyncedNotificationSpecifics part
  // of the sync data.  This is owned by the SyncedNotification class
  // (actually refcounted, so it is jointly owned with sync).
  // Don't free this pointer!
  const sync_pb::SyncedNotificationSpecifics* GetSyncedNotificationSpecifics();

 private:
  // Helper function to mark a notification as read or dismissed.
  bool SetReadState(
      const sync_pb::CoalescedSyncedNotification_ReadState& read_state);

  // Parsing functions to get this information out of the sync_data and into
  // our local variables.
  std::string ExtractTitle(const syncer::SyncData& sync_data) const;
  std::string ExtractAppId(const syncer::SyncData& sync_data) const;
  std::string ExtractCoalescingKey(const syncer::SyncData& sync_data) const;
  GURL ExtractOriginUrl(const syncer::SyncData& sync_data) const;
  GURL ExtractIconUrl(const syncer::SyncData& sync_data) const;
  GURL ExtractImageUrl(const syncer::SyncData& sync_data) const;
  std::string ExtractFirstExternalId(const syncer::SyncData& sync_data) const;
  std::string ExtractNotificationId(const syncer::SyncData& sync_data) const;
  std::string ExtractBody(const syncer::SyncData& sync_data) const;

  // We need to hold onto the sync data object because GetAllSyncData
  // may ask for it back.
  syncer::SyncData sync_data_;

  // Set this to true if we make any changes to the client data.
  bool has_local_changes_;

  // TODO(petewil): this list is not all inclusive, add the remaining fields.
  const std::string title_;
  const std::string app_id_;
  const std::string coalescing_key_;
  const std::string first_external_id_;
  const std::string notification_id_;
  const std::string body_;
  const GURL origin_url_;
  const GURL icon_url_;
  const GURL image_url_;

  DISALLOW_COPY_AND_ASSIGN(SyncedNotification);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_H_
