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
#include "base/timer/timer.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;
class Profile;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {

// Implementation of web_app::PendingAppManager that manages the set of
// Bookmark Apps which are being installed, uninstalled, and updated.
//
// WebAppProvider creates an instance of this class and manages its
// lifetime. This class should only be used from the UI thread.
class PendingBookmarkAppManager final : public web_app::PendingAppManager,
                                        public content::WebContentsObserver {
 public:
  using WebContentsFactory =
      base::RepeatingCallback<std::unique_ptr<content::WebContents>(Profile*)>;
  using TaskFactory =
      base::RepeatingCallback<std::unique_ptr<BookmarkAppInstallationTask>(
          Profile*)>;

  explicit PendingBookmarkAppManager(Profile* profile);
  ~PendingBookmarkAppManager() override;

  // web_app::PendingAppManager
  void Install(AppInfo app_to_install, OnceInstallCallback callback) override;
  void InstallApps(std::vector<AppInfo> apps_to_install,
                   const RepeatingInstallCallback& callback) override;

  void SetFactoriesForTesting(WebContentsFactory web_contents_factory,
                              TaskFactory task_factory);

  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer);

 private:
  struct Installation;

  void MaybeStartNextInstallation();

  void CreateWebContentsIfNecessary();

  void OnInstalled(BookmarkAppInstallationTask::Result result);

  void OnWebContentsLoadTimedOut();

  void CurrentInstallationFinished(const std::string& app_id);

  // WebContentsObserver
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override;

  Profile* profile_;

  WebContentsFactory web_contents_factory_;
  TaskFactory task_factory_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<base::OneShotTimer> timer_;

  std::unique_ptr<Installation> current_installation_;
  std::unique_ptr<BookmarkAppInstallationTask> current_installation_task_;

  std::deque<std::unique_ptr<Installation>> installation_queue_;

  base::WeakPtrFactory<PendingBookmarkAppManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PendingBookmarkAppManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_PENDING_BOOKMARK_APP_MANAGER_H_
