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
#include "url/gurl.h"

namespace web_app {

enum class InstallResultCode;

// PendingAppManager installs, uninstalls, and updates apps.
//
// Implementations of this class should perform each set of operations serially
// in the order in which they arrive. For example, if an uninstall request gets
// queued while an update request for the same app is pending, implementations
// should wait for the update request to finish before uninstalling the app.
class PendingAppManager {
 public:
  using OnceInstallCallback =
      base::OnceCallback<void(const GURL& app_url, InstallResultCode code)>;
  using RepeatingInstallCallback =
      base::RepeatingCallback<void(const GURL& app_url,
                                   InstallResultCode code)>;
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
  //
  // Internal means that the set of apps to install is defined statically, and
  // can be determined solely by 'first party' data: the Chromium binary,
  // stored user preferences (assumed to have been edited only by Chromiums
  // past and present) and the like. External means that the set of apps to
  // install is defined dynamically, depending on 'third party' data that can
  // change from session to session even if those sessions are for the same
  // user running the same binary on the same hardware.
  //
  // Third party data sources can include configuration files in well known
  // directories on the file system, entries (or the lack of) in the Windows
  // registry, or centrally configured sys-admin policy.
  //
  // The internal versus external distinction matters because, for external
  // install sources, the code that installs apps based on those external data
  // sources can also need to *un*install apps if those external data sources
  // change, either by an explicit uninstall request or an implicit uninstall
  // of a previously-listed no-longer-listed app.
  //
  // Without the distinction between e.g. kInternal and kExternalXxx, the code
  // that manages external-xxx apps might inadvertently uninstall internal apps
  // that it otherwise doesn't recognize.
  //
  // In practice, every kExternalXxx enum definition should correspond to
  // exactly one place in the code where SynchronizeInstalledApps is called.
  enum class InstallSource {
    // Do not remove or re-order the names, only append to the end. Their
    // integer values are persisted in the preferences.

    kInternal = 0,
    // Installed by default on the system, such as "all such-and-such make and
    // model Chromebooks should have this app installed".
    //
    // The corresponding SynchronizeInstalledApps call site is in
    // WebAppProvider::OnScanForExternalWebApps.
    kExternalDefault = 1,
    // Installed by sys-admin policy, such as "all example.com employees should
    // have this app installed".
    //
    // The corresponding SynchronizeInstalledApps call site is in
    // WebAppPolicyManager::RefreshPolicyInstalledApps.
    kExternalPolicy = 2,
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

  // Returns the URLs of those apps installed from |install_source|.
  virtual std::vector<GURL> GetInstalledAppUrls(
      InstallSource install_source) const = 0;

  // Installs |desired_apps| and uninstalls any apps in
  // GetInstalledAppUrls(install_source) that are not in |desired_apps|'s URLs.
  //
  // All apps in |desired_apps| should have |install_source| as their source.
  //
  // Note that this returns after queueing work (installation and
  // uninstallation) to be done. It does not wait until that work is complete.
  void SynchronizeInstalledApps(std::vector<AppInfo> desired_apps,
                                InstallSource install_source);

 private:
  DISALLOW_COPY_AND_ASSIGN(PendingAppManager);
};

std::ostream& operator<<(std::ostream& out,
                         const PendingAppManager::AppInfo& app_info);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_PENDING_APP_MANAGER_H_
