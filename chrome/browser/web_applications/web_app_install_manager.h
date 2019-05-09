// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

class Profile;

namespace web_app {

enum class InstallResultCode;
class InstallFinalizer;
class WebAppDataRetriever;
class WebAppInstallTask;

class WebAppInstallManager final : public InstallManager {
 public:
  WebAppInstallManager(Profile* profile, InstallFinalizer* install_finalizer);
  ~WebAppInstallManager() override;

  // InstallManager:
  bool CanInstallWebApp(content::WebContents* web_contents) override;
  void InstallWebAppFromManifest(content::WebContents* contents,
                                 WebappInstallSource install_source,
                                 WebAppInstallDialogCallback dialog_callback,
                                 OnceInstallCallback callback) override;
  void InstallWebAppFromManifestWithFallback(
      content::WebContents* contents,
      bool force_shortcut_app,
      WebappInstallSource install_source,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback) override;
  void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      bool no_network_install,
      WebappInstallSource install_source,
      OnceInstallCallback callback) override;
  void InstallWebAppWithOptions(content::WebContents* web_contents,
                                const InstallOptions& install_options,
                                OnceInstallCallback callback) override;
  void InstallOrUpdateWebAppFromSync(
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) override;
  void InstallWebAppForTesting(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      OnceInstallCallback callback) override;

  using DataRetrieverFactory =
      base::RepeatingCallback<std::unique_ptr<WebAppDataRetriever>()>;
  void SetDataRetrieverFactoryForTesting(
      DataRetrieverFactory data_retriever_factory);

 private:
  void OnTaskCompleted(WebAppInstallTask* task,
                       OnceInstallCallback callback,
                       const AppId& app_id,
                       InstallResultCode code);

  DataRetrieverFactory data_retriever_factory_;

  using Tasks = base::flat_set<std::unique_ptr<WebAppInstallTask>,
                               base::UniquePtrComparator>;
  Tasks tasks_;

  InstallFinalizer* install_finalizer_;

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
