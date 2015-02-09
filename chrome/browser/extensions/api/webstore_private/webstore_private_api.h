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

class WebstorePrivateBeginInstallWithManifest3Function
    : public UIThreadExtensionFunction,
      public ExtensionInstallPrompt::Delegate,
      public WebstoreInstallHelper::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.beginInstallWithManifest3",
                             WEBSTOREPRIVATE_BEGININSTALLWITHMANIFEST3)

  // Result codes for the return value. If you change this, make sure to
  // update the description for the beginInstallWithManifest3 callback in
  // the extension API JSON.
  enum ResultCode {
    ERROR_NONE = 0,

    // An unspecified error occurred.
    UNKNOWN_ERROR,

    // The user cancelled the confirmation dialog instead of accepting it.
    USER_CANCELLED,

    // The manifest failed to parse correctly.
    MANIFEST_ERROR,

    // There was a problem parsing the base64 encoded icon data.
    ICON_ERROR,

    // The extension id was invalid.
    INVALID_ID,

    // The page does not have permission to call this function.
    PERMISSION_DENIED,

    // Invalid icon url.
    INVALID_ICON_URL,

    // An extension with the same extension id has already been installed.
    ALREADY_INSTALLED,
  };

  WebstorePrivateBeginInstallWithManifest3Function();

 private:
  ~WebstorePrivateBeginInstallWithManifest3Function() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  // WebstoreInstallHelper::Delegate:
  void OnWebstoreParseSuccess(const std::string& id,
                              const SkBitmap& icon,
                              base::DictionaryValue* parsed_manifest) override;
  void OnWebstoreParseFailure(const std::string& id,
                              InstallHelperResultCode result_code,
                              const std::string& error_message) override;

  // ExtensionInstallPrompt::Delegate:
  void InstallUIProceed() override;
  void InstallUIAbort(bool user_initiated) override;

  // Response helpers.
  const char* ResultCodeToString(ResultCode code) const;
  ExtensionFunction::ResponseValue BuildResponseForSuccess();
  ExtensionFunction::ResponseValue BuildResponseForError(
      ResultCode code, const std::string& error);

  ChromeExtensionFunctionDetails chrome_details_;

  // This stores the input parameters to the function.
  scoped_ptr<api::webstore_private::BeginInstallWithManifest3::Params> params_;

  // The results of parsing manifest_ and icon_data_ go into these two.
  scoped_ptr<base::DictionaryValue> parsed_manifest_;
  SkBitmap icon_;

  // A dummy Extension object we create for the purposes of using
  // ExtensionInstallPrompt to prompt for confirmation of the install.
  scoped_refptr<Extension> dummy_extension_;

  // The class that displays the install prompt.
  scoped_ptr<ExtensionInstallPrompt> install_prompt_;

  scoped_ptr<ScopedActiveInstall> scoped_active_install_;

  // The authuser query parameter value which should be used with CRX download
  // requests. This is empty if authuser should not be set on download requests.
  std::string authuser_;
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
      api::webstore_private::LaunchEphemeralApp::Results::Result result,
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
