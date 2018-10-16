// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

class Profile;
struct WebApplicationInfo;

namespace web_app {

enum class InstallResultCode;

class WebAppDataRetriever;
class WebAppRegistrar;

class WebAppInstallManager final : public InstallManager {
 public:
  WebAppInstallManager(Profile* profile, WebAppRegistrar* registrar);
  ~WebAppInstallManager() override;

  using OnceInstallCallback =
      base::OnceCallback<void(const AppId& app_id, InstallResultCode code)>;

  // InstallManager interface implementation.
  bool CanInstallWebApp(const content::WebContents* web_contents) override;
  void InstallWebApp(content::WebContents* web_contents,
                     bool force_shortcut_app) override;

  void SetDataRetrieverForTesting(
      std::unique_ptr<WebAppDataRetriever> data_retriever);

  void InstallWebAppForTesting(content::WebContents* web_contents,
                               OnceInstallCallback callback);

 private:
  void InstallFromWebContents(content::WebContents* web_contents,
                              OnceInstallCallback callback);

  void OnGetWebApplicationInfo(
      OnceInstallCallback install_callback,
      std::unique_ptr<WebApplicationInfo> web_app_info);

  Profile* profile_;
  WebAppRegistrar* registrar_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  base::WeakPtrFactory<WebAppInstallManager> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(WebAppInstallManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
