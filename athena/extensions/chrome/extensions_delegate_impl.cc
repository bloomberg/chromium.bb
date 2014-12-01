// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/public/extensions_delegate.h"

#include "athena/extensions/chrome/athena_chrome_app_window_client.h"
#include "athena/extensions/chrome/athena_extension_install_ui.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_urls.h"
#include "net/base/url_util.h"
#include "ui/base/window_open_disposition.h"

namespace athena {
namespace {

class ChromeExtensionsDelegate : public ExtensionsDelegate {
 public:
  explicit ChromeExtensionsDelegate(content::BrowserContext* context)
      : extension_service_(
            extensions::ExtensionSystem::Get(context)->extension_service()) {
    extensions::AppWindowClient::Set(&app_window_client_);
  }

  ~ChromeExtensionsDelegate() override {
    extensions::AppWindowClient::Set(nullptr);
  }

 private:
  // ExtensionsDelegate:
  content::BrowserContext* GetBrowserContext() const override {
    return extension_service_->GetBrowserContext();
  }
  const extensions::ExtensionSet& GetInstalledExtensions() override {
    return extensions::ExtensionRegistry::Get(GetBrowserContext())
        ->enabled_extensions();
  }
  bool LaunchApp(const std::string& app_id) override {
    // Check Running apps
    content::BrowserContext* context = GetBrowserContext();
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(context)->GetExtensionById(
            app_id, extensions::ExtensionRegistry::EVERYTHING);
    DCHECK(extension);
    if (!extension)
      return false;

    // TODO(oshima): Support installation/enabling process.
    if (!extensions::util::IsAppLaunchableWithoutEnabling(app_id, context))
      return false;

    AppLaunchParams params(Profile::FromBrowserContext(context), extension,
                           CURRENT_TAB, chrome::HOST_DESKTOP_TYPE_ASH,
                           extensions::SOURCE_APP_LAUNCHER);
    // TODO(oshima): rename HOST_DESTOP_TYPE_ASH to non native desktop.

    if (app_id == extensions::kWebStoreAppId) {
      std::string source_value =
          std::string(extension_urls::kLaunchSourceAppList);
      // Set an override URL to include the source.
      GURL extension_url =
          extensions::AppLaunchInfo::GetFullLaunchURL(extension);
      params.override_url = net::AppendQueryParameter(
          extension_url, extension_urls::kWebstoreSourceField, source_value);
    }
    params.container = extensions::LAUNCH_CONTAINER_WINDOW;

    OpenApplication(params);
    return true;
  }

  bool UnloadApp(const std::string& app_id) override {
    // TODO(skuhne): Implement using extension service.
    return false;
  }

  scoped_ptr<extensions::ExtensionInstallUI> CreateExtensionInstallUI()
      override {
    return scoped_ptr<extensions::ExtensionInstallUI>(
        new AthenaExtensionInstallUI());
  }

  // ExtensionService for the browser context this is created for.
  ExtensionService* extension_service_;

  // Installed extensions.
  extensions::ExtensionSet extensions_;

  AthenaChromeAppWindowClient app_window_client_;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsDelegate);
};

}  // namespace

// static
void ExtensionsDelegate::CreateExtensionsDelegate(
    content::BrowserContext* context) {
  new ChromeExtensionsDelegate(context);
}

}  // namespace athena
