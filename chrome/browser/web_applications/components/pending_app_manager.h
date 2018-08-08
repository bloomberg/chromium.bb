// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
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
  using InstallCallback = base::OnceCallback<void(const std::string&)>;

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

  // Queues an installation operation with the highest priority. Essentially
  // installing the app immediately if there are no ongoing operations or
  // installing the app right after the current operation finishes. Runs its
  // callback with the id of the installed app or an empty string if the
  // installation fails.
  //
  // Fails if the same operation has been queued before. Should only be used in
  // response to a user action e.g. the user clicked an install button.
  virtual void Install(AppInfo app_to_install,
                       InstallCallback callback) = 0;

  // Adds |apps_to_install| to the queue of operations.
  virtual void ProcessAppOperations(std::vector<AppInfo> apps_to_install) = 0;

  DISALLOW_COPY_AND_ASSIGN(PendingAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_
