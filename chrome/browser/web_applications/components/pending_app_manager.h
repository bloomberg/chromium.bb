// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
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
  using OnceInstallCallback =
      base::OnceCallback<void(const GURL& app_url,
                              const base::Optional<std::string>&)>;
  using RepeatingInstallCallback =
      base::RepeatingCallback<void(const GURL& app_url,
                                   const base::Optional<std::string>&)>;
  using UninstallCallback =
      base::RepeatingCallback<void(const GURL& app_url, bool succeeded)>;

  // How the app will be launched after installation.
  enum class LaunchContainer {
    // When `kDefault` is used, the app will launch in a window if the site is
    // "installable" (also referred to as Progressive Web App) and in a tab if
    // the site is not "installable".
    kDefault,
    kTab,
    kWindow,
  };

  // Where an app was installed from. This affects what flags will be used when
  // installing the app.
  enum class InstallSource {
    // Installed by default on the system, such as "all such-and-such make and
    // model Chromebooks should have this app installed".
    kDefaultInstalled,
    // Installed by sys-admin policy, such as "all example.com employees should
    // have this app installed".
    kPolicyInstalled,
  };

  struct AppInfo {
    AppInfo(GURL url,
            LaunchContainer launch_container,
            InstallSource install_source,
            bool create_shortcuts = true);
    AppInfo(AppInfo&& other);
    ~AppInfo();

    std::unique_ptr<AppInfo> Clone() const;

    bool operator==(const AppInfo& other) const;

    const GURL url;
    const LaunchContainer launch_container;
    const InstallSource install_source;
    const bool create_shortcuts;

   private:
    DISALLOW_COPY_AND_ASSIGN(AppInfo);
  };

  PendingAppManager();
  virtual ~PendingAppManager();

  // Queues an installation operation with the highest priority. Essentially
  // installing the app immediately if there are no ongoing operations or
  // installing the app right after the current operation finishes. Runs its
  // callback with the URL in |app_to_install| and with the id of the installed
  // app or an empty string if the installation fails.
  //
  // Fails if the same operation has been queued before. Should only be used in
  // response to a user action e.g. the user clicked an install button.
  virtual void Install(AppInfo app_to_install,
                       OnceInstallCallback callback) = 0;

  // Adds |apps_to_install| to the queue of operations. Runs |callback|
  // with the URL of the corresponding AppInfo in |apps_to_install| and with the
  // id of the installed app or an empty string if the installation fails. Runs
  // |callback| for every completed installation - whether or not the
  // installation actually succeeded.
  virtual void InstallApps(std::vector<AppInfo> apps_to_install,
                           const RepeatingInstallCallback& callback) = 0;

  // Adds |apps_to_uninstall| to the queue of operations. Runs |callback|
  // with the URL of the corresponding app in |apps_to_install| and with a
  // bool indicating whether or not the uninstall succeeded. Runs |callback|
  // for every completed uninstallation - whether or not the uninstallation
  // actually succeeded.
  virtual void UninstallApps(std::vector<GURL> apps_to_uninstall,
                             const UninstallCallback& callback) = 0;

  DISALLOW_COPY_AND_ASSIGN(PendingAppManager);
};

std::ostream& operator<<(std::ostream& out,
                         const PendingAppManager::AppInfo& app_info);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_
