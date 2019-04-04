// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"

enum class WebappInstallSource;
struct WebApplicationInfo;

namespace content {
class WebContents;
}

namespace web_app {

enum class InstallResultCode;
class InstallManagerObserver;
struct InstallOptions;

// TODO(loyso): Rework this interface once BookmarkAppHelper erased. Unify the
// API and merge similar InstallWebAppZZZZ functions. crbug.com/915043.
class InstallManager {
 public:
  using OnceInstallCallback =
      base::OnceCallback<void(const AppId& app_id, InstallResultCode code)>;

  // Callback used to indicate whether a user has accepted the installation of a
  // web app.
  using WebAppInstallationAcceptanceCallback =
      base::OnceCallback<void(bool user_accepted,
                              std::unique_ptr<WebApplicationInfo>)>;

  // Callback to show the WebApp installation confirmation bubble in UI.
  // |web_app_info| is the WebApplicationInfo to be installed.
  using WebAppInstallDialogCallback = base::OnceCallback<void(
      content::WebContents* initiator_web_contents,
      std::unique_ptr<WebApplicationInfo> web_app_info,
      ForInstallableSite for_installable_site,
      WebAppInstallationAcceptanceCallback acceptance_callback)>;

  // Returns true if a web app can be installed for a given |web_contents|.
  virtual bool CanInstallWebApp(content::WebContents* web_contents) = 0;

  // Starts a web app installation process for a given |web_contents|.
  // |force_shortcut_app| forces the creation of a shortcut app instead of a PWA
  // even if installation is available.
  virtual void InstallWebApp(content::WebContents* web_contents,
                             bool force_shortcut_app,
                             WebappInstallSource install_source,
                             WebAppInstallDialogCallback dialog_callback,
                             OnceInstallCallback callback) = 0;

  // Starts a web app installation process for a given |web_contents|, initiated
  // by WebApp script. Bypasses the GetWebApplicationInfo from renderer step.
  virtual void InstallWebAppFromBanner(
      content::WebContents* web_contents,
      WebappInstallSource install_source,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback) = 0;

  // Starts a web app installation process using prefilled
  // |web_application_info|. If |no_network_install| is true, then
  // |web_application_info| holds all the data needed for installation and
  // InstallManager should not try to fetch a manifest.
  virtual void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      bool no_network_install,
      WebappInstallSource install_source,
      OnceInstallCallback callback) = 0;

  // Starts a background web app installation process for a given
  // |web_contents|.
  virtual void InstallWebAppWithOptions(content::WebContents* web_contents,
                                        const InstallOptions& install_options,
                                        OnceInstallCallback callback) = 0;

  // Starts background installation or an update of a web app from the sync
  // system. |web_application_info| contains received sync data. Icons will be
  // downloaded from the icon URLs provided in |web_application_info|.
  virtual void InstallOrUpdateWebAppFromSync(
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) = 0;

  // Starts background installation of a web app from the given
  // |web_application_info|.
  virtual void InstallWebAppForTesting(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) = 0;

  InstallManager();
  virtual ~InstallManager();

  // Called before the web app system gets destroyed.
  void Reset();

  void AddObserver(InstallManagerObserver* observer);
  void RemoveObserver(InstallManagerObserver* observer);

 private:
  base::ObserverList<InstallManagerObserver, true /*check_empty*/> observers_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_
