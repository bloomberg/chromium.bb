// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_PENDING_BOOKMARK_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_PENDING_BOOKMARK_APP_MANAGER_H_

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_uninstaller.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {
class AppRegistrar;
}  // namespace web_app

namespace extensions {

// Implementation of web_app::PendingAppManager that manages the set of
// Bookmark Apps which are being installed, uninstalled, and updated.
//
// WebAppProvider creates an instance of this class and manages its
// lifetime. This class should only be used from the UI thread.
class PendingBookmarkAppManager final : public web_app::PendingAppManager {
 public:
  using WebContentsFactory =
      base::RepeatingCallback<std::unique_ptr<content::WebContents>(Profile*)>;
  using TaskFactory = base::RepeatingCallback<
      std::unique_ptr<BookmarkAppInstallationTask>(Profile*, AppInfo)>;

  explicit PendingBookmarkAppManager(Profile* profile,
                                     web_app::AppRegistrar* registrar_);
  ~PendingBookmarkAppManager() override;

  // web_app::PendingAppManager
  void Install(AppInfo app_to_install, OnceInstallCallback callback) override;
  void InstallApps(std::vector<AppInfo> apps_to_install,
                   const RepeatingInstallCallback& callback) override;
  void UninstallApps(std::vector<GURL> apps_to_uninstall,
                     const UninstallCallback& callback) override;
  std::vector<GURL> GetInstalledAppUrls(
      web_app::InstallSource install_source) const override;
  base::Optional<std::string> LookupAppId(const GURL& url) const override;

  void SetFactoriesForTesting(WebContentsFactory web_contents_factory,
                              TaskFactory task_factory);
  void SetUninstallerForTesting(
      std::unique_ptr<BookmarkAppUninstaller> uninstaller);

 private:
  struct TaskAndCallback;

  // Returns (as the base::Optional part) whether or not there is already a
  // known extension for the given ID. The bool inside the base::Optional is,
  // when known, whether the extension is installed (true) or uninstalled
  // (false).
  base::Optional<bool> IsExtensionPresentAndInstalled(
      const std::string& extension_id);

  void MaybeStartNextInstallation();

  void StartInstallationTask(std::unique_ptr<TaskAndCallback> task);

  void CreateWebContentsIfNecessary();

  void OnUrlLoaded(web_app::WebAppUrlLoader::Result result);

  void OnInstalled(BookmarkAppInstallationTask::Result result);

  void CurrentInstallationFinished(const base::Optional<std::string>& app_id);

  Profile* profile_;
  web_app::AppRegistrar* registrar_;
  std::unique_ptr<BookmarkAppUninstaller> uninstaller_;
  web_app::ExtensionIdsMap extension_ids_map_;

  // unique_ptr so that it can be replaced in tests.
  std::unique_ptr<web_app::WebAppUrlLoader> url_loader_;

  WebContentsFactory web_contents_factory_;
  TaskFactory task_factory_;

  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<TaskAndCallback> current_task_and_callback_;

  std::deque<std::unique_ptr<TaskAndCallback>> pending_tasks_and_callbacks_;

  base::WeakPtrFactory<PendingBookmarkAppManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PendingBookmarkAppManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_PENDING_BOOKMARK_APP_MANAGER_H_
