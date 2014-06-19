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
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/sync.pb.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace sync_pb {
class SyncedNotificationSpecifics;
}

class NotificationUIManager;
class Profile;

namespace notifier {

class ChromeNotifierService;

class SyncedNotification : public chrome::BitmapFetcherDelegate {
 public:
  SyncedNotification(const syncer::SyncData& sync_data,
                     ChromeNotifierService* notifier,
                     NotificationUIManager* notification_manager);

  virtual ~SyncedNotification();

  enum ReadState {
    kUnread = 1,
    kRead = 2,
    kDismissed = 3,
  };

  static const int kUndefinedPriority = 65535;

  void Update(const syncer::SyncData& sync_data);

  // Display the notification in the notification center
  void Show(Profile* profile);

  // This gets a pointer to the SyncedNotificationSpecifics part
  // of the sync data.
  sync_pb::EntitySpecifics GetEntitySpecifics() const;

  // Display the notification if it has the specified app_id_name.
  void ShowAllForAppId(Profile* profile,
                       std::string app_id_name);

  // Remove the notification if it has the specified app_id_name.
  void HideAllForAppId(std::string app_id_name);

  // Fill up the queue of bitmaps to fetch.
  void QueueBitmapFetchJobs(ChromeNotifierService* notifier_service,
                            Profile* profile);

  // Start the bitmap fetching.  When it is complete, the callback
  // will call Show().
  void StartBitmapFetch();

  // Check against an incoming notification, see if only the read state is
  // different.
  bool EqualsIgnoringReadState(const SyncedNotification& other) const;

  // Mark the notification as Read.
  void NotificationHasBeenRead();

  // Mark a notification as Dismissed (deleted).
  void NotificationHasBeenDismissed();

  // Here are some helper functions to get individual data parts out of a
  // SyncedNotification.
  std::string GetTitle() const;
  std::string GetHeading() const;
  std::string GetDescription() const;
  std::string GetAnnotation() const;
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
  GURL GetDefaultDestinationIconUrl() const;
  GURL GetDefaultDestinationUrl() const;
  std::string GetButtonTitle(unsigned int which_button) const;
  GURL GetButtonIconUrl(unsigned int which_button) const;
  GURL GetButtonUrl(unsigned int which_button) const;
  GURL GetProfilePictureUrl(unsigned int which_url) const;
  size_t GetProfilePictureCount() const;
  size_t GetNotificationCount() const;
  size_t GetButtonCount() const;
  std::string GetContainedNotificationTitle(int index) const;
  std::string GetContainedNotificationMessage(int index) const;
  const gfx::Image& GetAppIcon() const;

  // Use this to prevent toasting a notification.
  void set_toast_state(bool toast_state);

  // Write a notification to the console log.
  void LogNotification();

 private:
  // Method inherited from BitmapFetcher delegate.
  virtual void OnFetchComplete(const GURL url, const SkBitmap* bitmap) OVERRIDE;

  // If this bitmap has a valid GURL, create a fetcher for it.
  void CreateBitmapFetcher(const GURL& gurl);

  // Check to see if we have responses for all the bitmaps we need.
  bool AreAllBitmapsFetched();

  // Helper function to mark a notification as read or dismissed.
  void SetReadState(const ReadState& read_state);

  void SetNotifierServiceForTest(ChromeNotifierService* notifier) {
    notifier_service_ = notifier;
  }

  sync_pb::SyncedNotificationSpecifics specifics_;
  NotificationUIManager* notification_manager_;
  ChromeNotifierService* notifier_service_;
  Profile* profile_;
  bool toast_state_;
  ScopedVector<chrome::BitmapFetcher> fetchers_;
  gfx::Image app_icon_bitmap_;
  gfx::Image sender_bitmap_;
  gfx::Image image_bitmap_;
  std::vector<gfx::Image> button_bitmaps_;
  bool app_icon_bitmap_fetch_pending_;
  bool sender_bitmap_fetch_pending_;
  bool image_bitmap_fetch_pending_;
  std::vector<bool> button_bitmaps_fetch_pending_;

  friend class SyncedNotificationTest;

  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationTest, CreateBitmapFetcherTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationTest, OnFetchCompleteTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationTest, QueueBitmapFetchJobsTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationTest, EmptyBitmapTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationTest, ShowIfNewlyEnabledTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationTest, HideIfNewlyRemovedTest);
  FRIEND_TEST_ALL_PREFIXES(ChromeNotifierServiceTest, SetAddedAppIdsTest);

  DISALLOW_COPY_AND_ASSIGN(SyncedNotification);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_H_
