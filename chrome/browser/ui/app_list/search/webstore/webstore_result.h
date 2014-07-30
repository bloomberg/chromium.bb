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
#include "chrome/common/extensions/webstore_install_result.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/manifest.h"
#include "url/gurl.h"

class AppListControllerDelegate;
class Profile;

namespace extensions {
class ExtensionRegistry;
class InstallTracker;
}

namespace app_list {

class WebstoreResult : public ChromeSearchResult,
                       public extensions::InstallObserver,
                       public extensions::ExtensionRegistryObserver {
 public:
  WebstoreResult(Profile* profile,
                 const std::string& app_id,
                 const std::string& localized_name,
                 const GURL& icon_url,
                 extensions::Manifest::Type item_type,
                 AppListControllerDelegate* controller);
  virtual ~WebstoreResult();

  const std::string& app_id() const { return app_id_; }
  const GURL& icon_url() const { return icon_url_; }
  extensions::Manifest::Type item_type() const { return item_type_; }

  // ChromeSearchResult overides:
  virtual void Open(int event_flags) OVERRIDE;
  virtual void InvokeAction(int action_index, int event_flags) OVERRIDE;
  virtual scoped_ptr<ChromeSearchResult> Duplicate() OVERRIDE;
  virtual ChromeSearchResultType GetType() OVERRIDE;

 private:
  // Set the initial state and start observing both InstallObserver and
  // ExtensionRegistryObserver.
  void InitAndStartObserving();

  void UpdateActions();
  void SetDefaultDetails();
  void OnIconLoaded();

  void StartInstall(bool launch_ephemeral_app);
  void InstallCallback(bool success,
                       const std::string& error,
                       extensions::webstore_install::Result result);
  void LaunchCallback(extensions::webstore_install::Result result,
                      const std::string& error);

  void StopObservingInstall();
  void StopObservingRegistry();

  // extensions::InstallObserver overrides:
  virtual void OnDownloadProgress(const std::string& extension_id,
                                  int percent_downloaded) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

  // extensions::ExtensionRegistryObserver overides:
  virtual void OnExtensionInstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      bool is_update) OVERRIDE;
  virtual void OnShutdown(extensions::ExtensionRegistry* registry) OVERRIDE;

  Profile* profile_;
  const std::string app_id_;
  const std::string localized_name_;
  const GURL icon_url_;
  extensions::Manifest::Type item_type_;

  gfx::ImageSkia icon_;

  AppListControllerDelegate* controller_;
  extensions::InstallTracker* install_tracker_;  // Not owned.
  extensions::ExtensionRegistry* extension_registry_;  // Not owned.

  base::WeakPtrFactory<WebstoreResult> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_WEBSTORE_RESULT_H_
