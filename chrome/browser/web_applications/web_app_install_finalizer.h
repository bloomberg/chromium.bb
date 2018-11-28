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
class WebAppRegistrar;

class WebAppInstallFinalizer final : public InstallFinalizer {
 public:
  WebAppInstallFinalizer(WebAppRegistrar* registrar,
                         WebAppIconManager* icon_manager);
  ~WebAppInstallFinalizer() override;

  // InstallFinalizer:
  void FinalizeInstall(std::unique_ptr<WebApplicationInfo> web_app_info,
                       InstallFinalizedCallback callback) override;

 private:
  void OnDataWritten(InstallFinalizedCallback callback,
                     std::unique_ptr<WebApp> web_app,
                     bool success);

  WebAppRegistrar* registrar_;
  WebAppIconManager* icon_manager_;

  base::WeakPtrFactory<WebAppInstallFinalizer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallFinalizer);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
