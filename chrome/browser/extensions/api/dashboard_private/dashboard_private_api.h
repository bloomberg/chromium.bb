// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DASHBOARD_PRIVATE_DASHBOARD_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DASHBOARD_PRIVATE_DASHBOARD_PRIVATE_API_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/common/extensions/api/dashboard_private.h"
#include "extensions/browser/extension_function.h"
#include "third_party/skia/include/core/SkBitmap.h"

class GURL;

namespace chrome {
class BitmapFetcher;
}  // namespace chrome

namespace extensions {

class Extension;

class DashboardPrivateShowPermissionPromptForDelegatedInstallFunction
    : public UIThreadExtensionFunction,
      public ExtensionInstallPrompt::Delegate,
      public WebstoreInstallHelper::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "dashboardPrivate.showPermissionPromptForDelegatedInstall",
      DASHBOARDPRIVATE_SHOWPERMISSIONPROMPTFORDELEGATEDINSTALL)

  DashboardPrivateShowPermissionPromptForDelegatedInstallFunction();

 private:
  using Params =
     api::dashboard_private::ShowPermissionPromptForDelegatedInstall::Params;

  ~DashboardPrivateShowPermissionPromptForDelegatedInstallFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  // WebstoreInstallHelper::Delegate:
  void OnWebstoreParseSuccess(const std::string& id,
                              const SkBitmap& icon,
                              base::DictionaryValue* parsed_manifest) override;
  void OnWebstoreParseFailure(const std::string& id,
                              InstallHelperResultCode result,
                              const std::string& error_message) override;

  // ExtensionInstallPrompt::Delegate:
  void InstallUIProceed() override;
  void InstallUIAbort(bool user_initiated) override;

  ExtensionFunction::ResponseValue BuildResponse(
      api::dashboard_private::Result result,
      const std::string& error);
  scoped_ptr<base::ListValue> CreateResults(
      api::dashboard_private::Result result) const;

  const Params::Details& details() const { return params_->details; }

  scoped_ptr<Params> params_;

  // A dummy Extension object we create for the purposes of using
  // ExtensionInstallPrompt to prompt for confirmation of the install.
  scoped_refptr<Extension> dummy_extension_;

  scoped_ptr<ExtensionInstallPrompt> install_prompt_;
};

class DashboardPrivateShowPermissionPromptForDelegatedBundleInstallFunction
    : public UIThreadExtensionFunction,
      public chrome::BitmapFetcherDelegate {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "dashboardPrivate.showPermissionPromptForDelegatedBundleInstall",
      DASHBOARDPRIVATE_SHOWPERMISSIONPROMPTFORDELEGATEDBUNDLEINSTALL)

  DashboardPrivateShowPermissionPromptForDelegatedBundleInstallFunction();

 private:
  using Params = api::dashboard_private::
     ShowPermissionPromptForDelegatedBundleInstall::Params;

  ~DashboardPrivateShowPermissionPromptForDelegatedBundleInstallFunction()
      override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  // chrome::BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  void OnInstallApproval(BundleInstaller::ApprovalState state);

  const Params::Details& details() const { return params_->details; }

  ChromeExtensionFunctionDetails chrome_details_;

  scoped_ptr<Params> params_;

  scoped_ptr<extensions::BundleInstaller> bundle_;
  scoped_ptr<chrome::BitmapFetcher> icon_fetcher_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DASHBOARD_PRIVATE_DASHBOARD_PRIVATE_API_H_

