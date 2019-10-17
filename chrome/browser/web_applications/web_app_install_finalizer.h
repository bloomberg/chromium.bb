// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"

struct WebApplicationInfo;

namespace web_app {

class WebApp;
class WebAppIconManager;
class WebAppSyncBridge;

class WebAppInstallFinalizer final : public InstallFinalizer {
 public:
  WebAppInstallFinalizer(WebAppSyncBridge* sync_bridge,
                         WebAppIconManager* icon_manager);
  ~WebAppInstallFinalizer() override;

  // InstallFinalizer:
  void FinalizeInstall(const WebApplicationInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback) override;
  void FinalizeUpdate(const WebApplicationInfo& web_app_info,
                      InstallFinalizedCallback callback) override;
  void UninstallExternalWebApp(const GURL& app_url,
                               UninstallWebAppCallback callback) override;
  void UninstallWebApp(const AppId& app_id, UninstallWebAppCallback) override;
  bool CanCreateOsShortcuts() const override;
  void CreateOsShortcuts(const AppId& app_id,
                         bool add_to_desktop,
                         CreateOsShortcutsCallback callback) override;
  bool CanRevealAppShim() const override;
  void RevealAppShim(const AppId& app_id) override;
  bool CanUserUninstallFromSync(const AppId& app_id) const override;

 private:
  void OnIconsDataWritten(InstallFinalizedCallback callback,
                          std::unique_ptr<WebApp> web_app,
                          bool success);
  void OnDatabaseCommitCompleted(InstallFinalizedCallback callback,
                                 const AppId& app_id,
                                 bool success);

  WebAppSyncBridge* const sync_bridge_;
  WebAppIconManager* const icon_manager_;

  base::WeakPtrFactory<WebAppInstallFinalizer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallFinalizer);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
