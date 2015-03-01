// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_

#include <string>

#include "chrome/browser/extensions/active_install_data.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/common/extensions/api/webstore_private.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "extensions/browser/extension_function.h"
#include "third_party/skia/include/core/SkBitmap.h"

class GPUFeatureChecker;

namespace extensions {

class WebstorePrivateApi {
 public:
  // Allows you to override the WebstoreInstaller delegate for testing.
  static void SetWebstoreInstallerDelegateForTesting(
      WebstoreInstaller::Delegate* delegate);

  // Gets the pending approval for the |extension_id| in |profile|. Pending
  // approvals are held between the calls to beginInstallWithManifest and
  // completeInstall. This should only be used for testing.
  static scoped_ptr<WebstoreInstaller::Approval> PopApprovalForTesting(
      Profile* profile, const std::string& extension_id);
};

// Base class for webstorePrivate functions that show a permission prompt.
template<typename Params>
class WebstorePrivateFunctionWithPermissionPrompt
    : public UIThreadExtensionFunction,
      public ExtensionInstallPrompt::Delegate,
      public WebstoreInstallHelper::Delegate {
 public:
  WebstorePrivateFunctionWithPermissionPrompt();

 protected:
  ~WebstorePrivateFunctionWithPermissionPrompt() override;

  // May be implemented by subclasses to add their own code to the
  // ExtensionFunction::Run implementation. Return a non-null ResponseValue to
  // trigger an immediate response.
  virtual ExtensionFunction::ResponseValue RunExtraForResponse();

  // Must be implemented by subclasses to call one of the Confirm* methods on
  // |install_prompt|.
  virtual void ShowPrompt(ExtensionInstallPrompt* install_prompt) = 0;

  // May be implemented by subclasses to add their own code to the
  // ExtensionInstallPrompt::Delegate::InstallUIProceed and InstallUIAbort
  // implementations.
  virtual void InstallUIProceedHook() {}
  virtual void InstallUIAbortHook(bool user_initiated) {}

  // Must be implemented by subclasses to forward to their own Results::Create
  // function.
  virtual scoped_ptr<base::ListValue> CreateResults(
      api::webstore_private::Result result) const = 0;

  ExtensionFunction::ResponseValue BuildResponse(
      api::webstore_private::Result result,
      const std::string& error);

  const Params& params() const { return *params_; }
  const SkBitmap& icon() const { return icon_; }
  const scoped_refptr<Extension>& dummy_extension() const {
    return dummy_extension_;
  }

  scoped_ptr<base::DictionaryValue> PassParsedManifest();

 private:
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

  // This stores the input parameters to the function.
  scoped_ptr<Params> params_;

  // The results of parsing manifest_ and icon_data_.
  scoped_ptr<base::DictionaryValue> parsed_manifest_;
  SkBitmap icon_;

  // A dummy Extension object we create for the purposes of using
  // ExtensionInstallPrompt to prompt for confirmation of the install.
  scoped_refptr<Extension> dummy_extension_;

  // The class that displays the install prompt.
  scoped_ptr<ExtensionInstallPrompt> install_prompt_;
};

class WebstorePrivateBeginInstallWithManifest3Function
    : public WebstorePrivateFunctionWithPermissionPrompt
          <api::webstore_private::BeginInstallWithManifest3::Params> {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.beginInstallWithManifest3",
                             WEBSTOREPRIVATE_BEGININSTALLWITHMANIFEST3)

  WebstorePrivateBeginInstallWithManifest3Function();

 private:
  ~WebstorePrivateBeginInstallWithManifest3Function() override;

  ExtensionFunction::ResponseValue RunExtraForResponse() override;
  void ShowPrompt(ExtensionInstallPrompt* install_prompt) override;
  void InstallUIProceedHook() override;
  void InstallUIAbortHook(bool user_initiated) override;
  scoped_ptr<base::ListValue> CreateResults(
      api::webstore_private::Result result) const override;

  ChromeExtensionFunctionDetails chrome_details_;

  scoped_ptr<ScopedActiveInstall> scoped_active_install_;
};

class WebstorePrivateCompleteInstallFunction
    : public UIThreadExtensionFunction,
      public WebstoreInstaller::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.completeInstall",
                             WEBSTOREPRIVATE_COMPLETEINSTALL)

  WebstorePrivateCompleteInstallFunction();

 private:
  ~WebstorePrivateCompleteInstallFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  // WebstoreInstaller::Delegate:
  void OnExtensionInstallSuccess(const std::string& id) override;
  void OnExtensionInstallFailure(
      const std::string& id,
      const std::string& error,
      WebstoreInstaller::FailureReason reason) override;

  void OnInstallSuccess(const std::string& id);

  ChromeExtensionFunctionDetails chrome_details_;

  scoped_ptr<WebstoreInstaller::Approval> approval_;
  scoped_ptr<ScopedActiveInstall> scoped_active_install_;
};

class WebstorePrivateShowPermissionPromptForDelegatedInstallFunction
    : public WebstorePrivateFunctionWithPermissionPrompt
          <api::webstore_private::ShowPermissionPromptForDelegatedInstall::
               Params> {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "webstorePrivate.showPermissionPromptForDelegatedInstall",
      WEBSTOREPRIVATE_SHOWPERMISSIONPROMPTFORDELEGATEDINSTALL)

  WebstorePrivateShowPermissionPromptForDelegatedInstallFunction();

 private:
  ~WebstorePrivateShowPermissionPromptForDelegatedInstallFunction() override;

  void ShowPrompt(ExtensionInstallPrompt* install_prompt) override;
  scoped_ptr<base::ListValue> CreateResults(
      api::webstore_private::Result result) const override;
};

class WebstorePrivateEnableAppLauncherFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.enableAppLauncher",
                             WEBSTOREPRIVATE_ENABLEAPPLAUNCHER)

  WebstorePrivateEnableAppLauncherFunction();

 private:
  ~WebstorePrivateEnableAppLauncherFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  ChromeExtensionFunctionDetails chrome_details_;
};

class WebstorePrivateGetBrowserLoginFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getBrowserLogin",
                             WEBSTOREPRIVATE_GETBROWSERLOGIN)

  WebstorePrivateGetBrowserLoginFunction();

 private:
  ~WebstorePrivateGetBrowserLoginFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  ChromeExtensionFunctionDetails chrome_details_;
};

class WebstorePrivateGetStoreLoginFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getStoreLogin",
                             WEBSTOREPRIVATE_GETSTORELOGIN)

  WebstorePrivateGetStoreLoginFunction();

 private:
  ~WebstorePrivateGetStoreLoginFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  ChromeExtensionFunctionDetails chrome_details_;
};

class WebstorePrivateSetStoreLoginFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.setStoreLogin",
                             WEBSTOREPRIVATE_SETSTORELOGIN)

  WebstorePrivateSetStoreLoginFunction();

 private:
  ~WebstorePrivateSetStoreLoginFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  ChromeExtensionFunctionDetails chrome_details_;
};

class WebstorePrivateGetWebGLStatusFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getWebGLStatus",
                             WEBSTOREPRIVATE_GETWEBGLSTATUS)

  WebstorePrivateGetWebGLStatusFunction();

 private:
  ~WebstorePrivateGetWebGLStatusFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  void OnFeatureCheck(bool feature_allowed);

  scoped_refptr<GPUFeatureChecker> feature_checker_;
};

class WebstorePrivateGetIsLauncherEnabledFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getIsLauncherEnabled",
                             WEBSTOREPRIVATE_GETISLAUNCHERENABLED)

  WebstorePrivateGetIsLauncherEnabledFunction();

 private:
  ~WebstorePrivateGetIsLauncherEnabledFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  void OnIsLauncherCheckCompleted(bool is_enabled);
};

class WebstorePrivateIsInIncognitoModeFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.isInIncognitoMode",
                             WEBSTOREPRIVATE_ISININCOGNITOMODEFUNCTION)

  WebstorePrivateIsInIncognitoModeFunction();

 private:
  ~WebstorePrivateIsInIncognitoModeFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  ChromeExtensionFunctionDetails chrome_details_;
};

class WebstorePrivateLaunchEphemeralAppFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.launchEphemeralApp",
                             WEBSTOREPRIVATE_LAUNCHEPHEMERALAPP)

  WebstorePrivateLaunchEphemeralAppFunction();

 private:
  ~WebstorePrivateLaunchEphemeralAppFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  void OnLaunchComplete(webstore_install::Result result,
                        const std::string& error);

  ExtensionFunction::ResponseValue BuildResponse(
      api::webstore_private::Result result,
      const std::string& error);

  ChromeExtensionFunctionDetails chrome_details_;
};

class WebstorePrivateGetEphemeralAppsEnabledFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getEphemeralAppsEnabled",
                             WEBSTOREPRIVATE_GETEPHEMERALAPPSENABLED)

  WebstorePrivateGetEphemeralAppsEnabledFunction();

 private:
  ~WebstorePrivateGetEphemeralAppsEnabledFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_
