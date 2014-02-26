// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class represents the metadata for a service sending synced
// notifications.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_APP_INFO_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_APP_INFO_H_

#include <string>
#include <vector>

#include "url/gurl.h"

namespace notifier {

class SyncedNotificationAppInfo {
 public:
  explicit SyncedNotificationAppInfo(const std::string& settings_display_name);
  ~SyncedNotificationAppInfo();

  // Return true if the app id is present in this AppInfo protobuf.
  bool HasAppId(const std::string& app_id);

  // Add an app id to the supported set for this AppInfo protobuf.
  void AddAppId(const std::string& app_id);

  // Remove an app id from the set for this AppInfo protobuf.
  void RemoveAppId(const std::string& app_id);

  std::string settings_display_name() const { return settings_display_name_; }

  void SetSettingsIcon(const GURL& settings_icon) {
    settings_icon_url_ = settings_icon;
  }

  GURL settings_icon_url() { return settings_icon_url_; }

  // Build a vector of app_ids that this app_info contains.
  void GetAppIdList(std::vector<std::string>* app_id_list);

 private:
  // TODO(petewil): We need a unique id for a key.  We will use the settings
  // display name for now, but it would be more robust with a unique id.
  std::vector<std::string> app_ids_;
  std::string settings_display_name_;
  // TODO(petewil): We should get 1x and 2x versions of all these images.
  GURL settings_icon_url_;
  // TODO(petewil): Add monochrome icons for app badging and welcome icons.
  // TODO(petewil): Add a landing page link for settings/welcome toast.

  DISALLOW_COPY_AND_ASSIGN(SyncedNotificationAppInfo);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_APP_INFO_H_
