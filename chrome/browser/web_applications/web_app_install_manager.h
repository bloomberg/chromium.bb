// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;
struct WebApplicationInfo;

namespace blink {
struct Manifest;
}

namespace content {
class WebContents;
}

namespace web_app {

class WebAppDataRetriever;
class WebAppRegistrar;

class WebAppInstallManager final : public InstallManager,
                                   content::WebContentsObserver {
 public:
  WebAppInstallManager(Profile* profile, WebAppRegistrar* registrar);
  ~WebAppInstallManager() override;

  // InstallManager:
  bool CanInstallWebApp(const content::WebContents* web_contents) override;
  void InstallWebApp(content::WebContents* contents,
                     bool force_shortcut_app,
                     OnceInstallCallback callback) override;

  // WebContentsObserver:
  void WebContentsDestroyed() override;

  void SetDataRetrieverForTesting(
      std::unique_ptr<WebAppDataRetriever> data_retriever);

 private:
  void CallInstallCallback(const AppId& app_id, InstallResultCode code);
  void ReturnError(InstallResultCode code);

  // Checks typical errors like WebContents destroyed.
  bool InstallInterrupted() const;

  void OnGetWebApplicationInfo(
      bool force_shortcut_app,
      std::unique_ptr<WebApplicationInfo> web_app_info);
  void OnDidPerformInstallableCheck(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      bool force_shortcut_app,
      const blink::Manifest& manifest,
      bool is_installable);

  // Saved callback:
  OnceInstallCallback install_callback_;

  Profile* profile_;
  WebAppRegistrar* registrar_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  base::WeakPtrFactory<WebAppInstallManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_MANAGER_H_
