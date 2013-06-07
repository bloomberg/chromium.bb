// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class represents the data for a single Synced Notification.
// It should map 1-1 to all the data in synced_notification_sepcifics.proto,
// and the data and render protobufs that the specifics protobuf contains.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/notifications/sync_notifier/notification_bitmap_fetcher.h"
#include "googleurl/src/gurl.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/sync.pb.h"
#include "ui/gfx/image/image.h"

namespace sync_pb {
class SyncedNotificationSpecifics;
}

class NotificationUIManager;
class Profile;

namespace notifier {

class ChromeNotifierService;

class SyncedNotification : public NotificationBitmapFetcherDelegate {
 public:
  explicit SyncedNotification(const syncer::SyncData& sync_data);

  virtual ~SyncedNotification();

  enum ReadState {
    kUnread = 1,
    kRead = 2,
    kDismissed = 3,
  };

  static const int kUndefinedPriority = 65535;

  void Update(const syncer::SyncData& sync_data);

  // Here are some helper functions to get individual data parts out of a
  // SyncedNotification.
  std::string GetTitle() const;
  std::string GetHeading() const;
  std::string GetDescription() const;
  std::string GetAppId() const;
  std::string GetKey() const;
  GURL GetOriginUrl() const;
  GURL GetAppIconUrl() const;
  GURL GetImageUrl() const;
  std::string GetText() const;
  ReadState GetReadState() const;
  uint64 GetCreationTime() const;
  int GetPriority() const;
  std::string GetDefaultDestinationTitle() const;
  std::string GetDefaultDestinationIconUrl() const;
  std::string GetDefaultDestinationUrl() const;
  std::string GetButtonOneTitle() const;
  std::string GetButtonOneIconUrl() const;
  std::string GetButtonOneUrl() const;
  std::string GetButtonTwoTitle() const;
  std::string GetButtonTwoIconUrl() const;
  std::string GetButtonTwoUrl() const;
  int GetNotificationCount() const;
  int GetButtonCount() const;
  std::string GetContainedNotificationTitle(int index) const;
  std::string GetContainedNotificationMessage(int index) const;


  bool EqualsIgnoringReadState(const SyncedNotification& other) const;

  void NotificationHasBeenDismissed();

  // Fill up the queue of bitmaps to fetch.
  void QueueBitmapFetchJobs(NotificationUIManager* notification_manager,
                            ChromeNotifierService* notifier_service,
                            Profile* profile);

  // Start the bitmap fetching.  When it is complete, the callback
  // will call Show().
  void StartBitmapFetch();

  // Display the notification in the notification center
  void Show(NotificationUIManager* notification_manager,
            ChromeNotifierService* notifier_service,
            Profile* profile);

  // This gets a pointer to the SyncedNotificationSpecifics part
  // of the sync data.
  sync_pb::EntitySpecifics GetEntitySpecifics() const;

 private:
  // Helper function to mark a notification as read or dismissed.
  void SetReadState(const ReadState& read_state);

  // Method inherited from NotificationBitmapFetcher delegate.
  virtual void OnFetchComplete(const GURL url, const SkBitmap* bitmap) OVERRIDE;

  // If this bitmap has a valid GURL, create a fetcher for it.
  void AddBitmapToFetchQueue(const GURL& gurl);

  sync_pb::SyncedNotificationSpecifics specifics_;
  NotificationUIManager* notification_manager_;
  ChromeNotifierService* notifier_service_;
  Profile* profile_;
  ScopedVector<NotificationBitmapFetcher> fetchers_;
  int active_fetcher_count_;
  gfx::Image app_icon_bitmap_;
  gfx::Image image_bitmap_;
  gfx::Image button_one_bitmap_;
  gfx::Image button_two_bitmap_;

  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationTest, AddBitmapToFetchQueueTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationTest, OnFetchCompleteTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationTest, QueueBitmapFetchJobsTest);

  DISALLOW_COPY_AND_ASSIGN(SyncedNotification);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_H_
