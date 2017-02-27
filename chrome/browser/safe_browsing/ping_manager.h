// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_PING_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_PING_MANAGER_H_

#include "chrome/browser/permissions/permission_uma_util.h"
#include "components/safe_browsing/base_ping_manager.h"
#include "content/public/browser/permission_type.h"

class SkBitmap;

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace safe_browsing {

class NotificationImageReporter;
class PermissionReporter;
class SafeBrowsingDatabaseManager;

class SafeBrowsingPingManager : public BasePingManager {
 public:
  ~SafeBrowsingPingManager() override;

  // Create an instance of the safe browsing ping manager.
  static std::unique_ptr<SafeBrowsingPingManager> Create(
      net::URLRequestContextGetter* request_context_getter,
      const SafeBrowsingProtocolConfig& config);

  // Report permission action to SafeBrowsing servers.
  void ReportPermissionAction(const PermissionReportInfo& report_info);

  // Report notification content image to SafeBrowsing CSD server if necessary.
  void ReportNotificationImage(
      Profile* profile,
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      const GURL& origin,
      const SkBitmap& image);

 private:
  friend class NotificationImageReporterTest;
  friend class PermissionReporterBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingPingManagerCertReportingTest,
                           UMAOnFailure);

  // Constructs a SafeBrowsingPingManager that issues network requests
  // using |request_context_getter|.
  SafeBrowsingPingManager(
      net::URLRequestContextGetter* request_context_getter,
      const SafeBrowsingProtocolConfig& config);

  // Sends reports of permission actions.
  std::unique_ptr<PermissionReporter> permission_reporter_;

  // Sends reports of notification content images.
  std::unique_ptr<NotificationImageReporter> notification_image_reporter_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingPingManager);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_PING_MANAGER_H_
