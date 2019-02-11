// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"

struct WebApplicationInfo;

namespace content {
class WebContents;
}

namespace web_app {

enum class InstallResultCode;

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
                             WebAppInstallDialogCallback dialog_callback,
                             OnceInstallCallback callback) = 0;

  virtual ~InstallManager() = default;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_
