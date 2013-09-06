// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_WEBSTORE_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_WEBSTORE_RESULT_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "url/gurl.h"

class AppListControllerDelegate;
class Profile;

namespace extensions {
class InstallTracker;
}

namespace app_list {

class WebstoreResult : public ChromeSearchResult,
                       public extensions::InstallObserver {
 public:
  WebstoreResult(Profile* profile,
                 const std::string& app_id,
                 const std::string& localized_name,
                 const GURL& icon_url,
                 AppListControllerDelegate* controller);
  virtual ~WebstoreResult();

  // ChromeSearchResult overides:
  virtual void Open(int event_flags) OVERRIDE;
  virtual void InvokeAction(int action_index, int event_flags) OVERRIDE;
  virtual scoped_ptr<ChromeSearchResult> Duplicate() OVERRIDE;
  virtual ChromeSearchResultType GetType() OVERRIDE;

 private:
  void UpdateActions();
  void SetDefaultDetails();
  void OnIconLoaded();

  void StartInstall();
  void InstallCallback(bool success, const std::string& error);

  void StartObservingInstall();
  void StopObservingInstall();

  // extensions::InstallObserver overrides:
  virtual void OnBeginExtensionInstall(const std::string& extension_id,
                                       const std::string& extension_name,
                                       const gfx::ImageSkia& installing_icon,
                                       bool is_app,
                                       bool is_platform_app) OVERRIDE;
  virtual void OnDownloadProgress(const std::string& extension_id,
                                  int percent_downloaded) OVERRIDE;
  virtual void OnInstallFailure(const std::string& extension_id) OVERRIDE;
  virtual void OnExtensionInstalled(
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionLoaded(
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUninstalled(
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnAppsReordered() OVERRIDE;
  virtual void OnAppInstalledToAppList(
      const std::string& extension_id) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

  Profile* profile_;
  const std::string app_id_;
  const std::string localized_name_;
  const GURL icon_url_;

  gfx::ImageSkia icon_;
  base::WeakPtrFactory<WebstoreResult> weak_factory_;

  AppListControllerDelegate* controller_;
  extensions::InstallTracker* install_tracker_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(WebstoreResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_WEBSTORE_RESULT_H_
