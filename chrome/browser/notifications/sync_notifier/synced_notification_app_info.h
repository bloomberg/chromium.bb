// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class represents the metadata for a service sending synced
// notifications.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_APP_INFO_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_APP_INFO_H_

#include <string>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "chrome/browser/notifications/sync_notifier/image_holder.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/notifier_settings.h"
#include "url/gurl.h"

class Profile;

namespace notifier {

class SyncedNotificationAppInfoService;

class SyncedNotificationAppInfo : public notifier::ImageHolderDelegate {
 public:
  SyncedNotificationAppInfo(
      Profile* const profile,
      const std::string& settings_display_name,
      SyncedNotificationAppInfoService* synced_notification_app_info_service);
  virtual ~SyncedNotificationAppInfo();

  // Return true if the app id is present in this AppInfo protobuf.
  bool HasAppId(const std::string& app_id);

  // Add an app id to the supported set for this AppInfo protobuf.
  void AddAppId(const std::string& app_id);

  // Remove an app id from the set for this AppInfo protobuf.
  void RemoveAppId(const std::string& app_id);

  std::string settings_display_name() const { return settings_display_name_; }

  // Sets/gets the link to navigate to when the user clicks on the body of the
  // app's welcome notification.
  void SetWelcomeLinkUrl(const GURL& settings_link_url);
  GURL welcome_link_url() const { return welcome_link_url_; }

  // Set the URL for the low and high DPI bitmaps for the settings dialog.
  void SetSettingsURLs(const GURL& settings_low_dpi,
                       const GURL& settings_high_dpi);

  // Set the URL for the low and high DPI bitmaps for indicating the sending
  // service.
  void SetMonochromeURLs(const GURL& monochrome_low_dpi,
                         const GURL& monochrome_high_dpi);

  // Set the URL for the low and high DPI bitmaps for use by the welcome dialog.
  void SetWelcomeURLs(const GURL& welcome_low_dpi,
                      const GURL& welcome_high_dpi);

  GURL settings_icon_url();

  // If an app info is updated, keep track of the newly added app ids so we can
  // later inform the chrome_notifier_service to show any newly enabled
  // notifications.
  void set_added_app_ids(std::vector<std::string> added_app_ids) {
    added_app_ids_ = added_app_ids;
  }

  std::vector<std::string> added_app_ids() { return added_app_ids_; }

  // If an app info is updated removing app ids, keep track of the removed app
  // ids so we can later remove any affected notfications.
  void set_removed_app_ids(std::vector<std::string> removed_app_ids) {
    removed_app_ids_ = removed_app_ids;
  }

  std::vector<std::string> removed_app_ids() { return removed_app_ids_; }

  // TODO(petewil): Check resolution of system and return the right icon.
  gfx::Image icon();

  // Build a vector of app_ids that this app_info contains.
  std::vector<std::string> GetAppIdList();

  // Set up for fetching all the bitmaps in this AppInfo.
  void QueueBitmapFetchJobs();

  // Start the bitmap fetching.  When it is complete, the callback
  // will notify the ChromeNotifierService of the new app info availablity.
  void StartBitmapFetch();

  // Method inherited from ImageHolderDelegate
  virtual void OnFetchComplete() OVERRIDE;

  // Check to see if we have responses for all the bitmaps we need.
  bool AreAllBitmapsFetched();

  // Construct a Message Center NotifierId from this synced notification app
  // info object.
  message_center::NotifierId GetNotifierId();

 private:
  // TODO(petewil): We need a unique id for a key.  We will use the settings
  // display name, but it would be more robust with a unique id.
  Profile* profile_;
  std::vector<std::string> app_ids_;
  std::string settings_display_name_;
  GURL welcome_link_url_;

  // The 1x and 2x versions of the icon for settings, small.
  scoped_ptr<ImageHolder> settings_holder_;
  // Monochrome icons for app badging (1x and 2x), small.
  scoped_ptr<ImageHolder> monochrome_holder_;
  // Welcome dialog icon (1x and 2x), large.
  scoped_ptr<ImageHolder> welcome_holder_;
  // A landing page link for settings/welcome toast.
  GURL welcome_landing_page_url_;
  std::vector<std::string> added_app_ids_;
  std::vector<std::string> removed_app_ids_;
  SyncedNotificationAppInfoService* synced_notification_app_info_service_;

  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoTest, AddRemoveTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoTest, GetAppIdListTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoTest,
                           CreateBitmapFetcherTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoTest, OnFetchCompleteTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoTest,
                           QueueBitmapFetchJobsTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoTest,
                           AreAllBitmapsFetchedTest);

  DISALLOW_COPY_AND_ASSIGN(SyncedNotificationAppInfo);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_APP_INFO_H_
