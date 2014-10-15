// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_DRIVE_DRIVE_APP_UNINSTALL_SYNC_SERVICE_H_
#define CHROME_BROWSER_APPS_DRIVE_DRIVE_APP_UNINSTALL_SYNC_SERVICE_H_

class DriveAppUninstallSyncService {
 public:
  // Invoked when user uninstalls a default Drive app.
  virtual void TrackUninstalledDriveApp(
      const std::string& drive_app_id) = 0;

  // Invoked when user installs a default Drive app.
  virtual void UntrackUninstalledDriveApp(
      const std::string& drive_app_id) = 0;

 protected:
  virtual ~DriveAppUninstallSyncService() {}
};

#endif  // CHROME_BROWSER_APPS_DRIVE_DRIVE_APP_UNINSTALL_SYNC_SERVICE_H_
