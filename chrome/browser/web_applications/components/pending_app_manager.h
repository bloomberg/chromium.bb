// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_

#include <vector>

#include "base/macros.h"
#include "url/gurl.h"

namespace web_app {

// PendingAppManager installs, uninstalls, and updates apps.
//
// Implementations of this class should perform each set of operations serially
// in the order in which they arrive. For example, if an uninstall request gets
// queued while an update request for the same app is pending, implementations
// should wait for the update request to finish before uninstalling the app.
class PendingAppManager {
 public:
  // How the app will be launched after installation.
  enum class LaunchContainer {
    kTab,
    kWindow,
  };

  struct AppInfo {
    AppInfo(GURL url, LaunchContainer launch_container);
    AppInfo(AppInfo&& other);
    ~AppInfo();

    bool operator==(const AppInfo& other) const;

    GURL url;
    LaunchContainer launch_container;

    DISALLOW_COPY_AND_ASSIGN(AppInfo);
  };

  PendingAppManager();
  virtual ~PendingAppManager();

  // Starts the installation of |apps_to_install|.
  virtual void ProcessAppOperations(std::vector<AppInfo> apps_to_install) = 0;

  DISALLOW_COPY_AND_ASSIGN(PendingAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_
