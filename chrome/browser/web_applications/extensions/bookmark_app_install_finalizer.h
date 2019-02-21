// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALL_FINALIZER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"

class Profile;

struct WebApplicationInfo;

namespace content {
class WebContents;
}

namespace extensions {

class CrxInstaller;
class CrxInstallError;

// Class used to actually install the Bookmark App in the system.
class BookmarkAppInstallFinalizer : public web_app::InstallFinalizer {
 public:
  // Constructs a BookmarkAppInstallFinalizer that will install the Bookmark App
  // in |profile|.
  explicit BookmarkAppInstallFinalizer(Profile* profile);
  ~BookmarkAppInstallFinalizer() override;

  // InstallFinalizer:
  void FinalizeInstall(const WebApplicationInfo& web_app_info,
                       InstallFinalizedCallback callback) override;
  void CreateOsShortcuts(const web_app::AppId& app_id) override;
  void ReparentTab(const WebApplicationInfo& web_app_info,
                   const web_app::AppId& app_id,
                   content::WebContents* web_contents) override;

  void SetCrxInstallerForTesting(scoped_refptr<CrxInstaller> crx_installer);

 private:
  void OnExtensionInstalled(std::unique_ptr<WebApplicationInfo> web_app_info,
                            InstallFinalizedCallback callback,
                            const base::Optional<CrxInstallError>& error);

  scoped_refptr<CrxInstaller> crx_installer_;
  Profile* profile_;

  // We need a WeakPtr because CrxInstaller is refcounted and it can run its
  // callback after this class has been destroyed.
  base::WeakPtrFactory<BookmarkAppInstallFinalizer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallFinalizer);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALL_FINALIZER_H_
