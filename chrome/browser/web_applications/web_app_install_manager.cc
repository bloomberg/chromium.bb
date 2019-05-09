// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

WebAppInstallManager::WebAppInstallManager(Profile* profile,
                                           InstallFinalizer* install_finalizer)
    : InstallManager(profile), install_finalizer_(install_finalizer) {
  data_retriever_factory_ = base::BindRepeating(
      []() { return std::make_unique<WebAppDataRetriever>(); });
}

WebAppInstallManager::~WebAppInstallManager() = default;

bool WebAppInstallManager::CanInstallWebApp(
    content::WebContents* web_contents) {
  Profile* web_contents_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  return AreWebAppsUserInstallable(web_contents_profile) &&
         IsValidWebAppUrl(web_contents->GetLastCommittedURL());
}

void WebAppInstallManager::InstallWebAppFromManifest(
    content::WebContents* contents,
    WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  DCHECK(AreWebAppsUserInstallable(profile()));

  auto task = std::make_unique<WebAppInstallTask>(
      profile(), install_finalizer_, data_retriever_factory_.Run());
  task->InstallWebAppFromManifest(
      contents, install_source, std::move(dialog_callback),
      base::BindOnce(&WebAppInstallManager::OnTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppFromManifestWithFallback(
    content::WebContents* contents,
    bool force_shortcut_app,
    WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  DCHECK(AreWebAppsUserInstallable(profile()));

  auto task = std::make_unique<WebAppInstallTask>(
      profile(), install_finalizer_, data_retriever_factory_.Run());
  task->InstallWebAppFromManifestWithFallback(
      contents, force_shortcut_app, install_source, std::move(dialog_callback),
      base::BindOnce(&WebAppInstallManager::OnTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppFromInfo(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    bool no_network_install,
    WebappInstallSource install_source,
    OnceInstallCallback callback) {
  DCHECK(AreWebAppsUserInstallable(profile()));

  auto task = std::make_unique<WebAppInstallTask>(
      profile(), install_finalizer_, data_retriever_factory_.Run());
  task->InstallWebAppFromInfo(
      std::move(web_application_info), no_network_install, install_source,
      base::BindOnce(&WebAppInstallManager::OnTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppWithOptions(
    content::WebContents* web_contents,
    const InstallOptions& install_options,
    OnceInstallCallback callback) {
  // TODO(loyso): Implement it.
  NOTIMPLEMENTED();
}

void WebAppInstallManager::InstallOrUpdateWebAppFromSync(
    const AppId& app_id,
    std::unique_ptr<WebApplicationInfo> web_application_info,
    OnceInstallCallback callback) {
  // TODO(loyso): Implement it.
  NOTIMPLEMENTED();
}

void WebAppInstallManager::InstallWebAppForTesting(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    OnceInstallCallback callback) {
  // TODO(loyso): Implement it.
  NOTIMPLEMENTED();
}

void WebAppInstallManager::SetDataRetrieverFactoryForTesting(
    DataRetrieverFactory data_retriever_factory) {
  data_retriever_factory_ = std::move(data_retriever_factory);
}

void WebAppInstallManager::OnTaskCompleted(WebAppInstallTask* task,
                                           OnceInstallCallback callback,
                                           const AppId& app_id,
                                           InstallResultCode code) {
  DCHECK(tasks_.contains(task));
  tasks_.erase(task);

  std::move(callback).Run(app_id, code);
}

}  // namespace web_app
