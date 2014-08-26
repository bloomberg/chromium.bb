// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_

#include <string>

#include "chrome/browser/extensions/active_install_data.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/common/extensions/api/webstore_private.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/skia/include/core/SkBitmap.h"

class ProfileSyncService;

namespace content {
class GpuDataManager;
}

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

class WebstorePrivateInstallBundleFunction
    : public ChromeAsyncExtensionFunction,
      public extensions::BundleInstaller::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.installBundle",
                             WEBSTOREPRIVATE_INSTALLBUNDLE)

  WebstorePrivateInstallBundleFunction();

  // BundleInstaller::Delegate:
  virtual void OnBundleInstallApproved() OVERRIDE;
  virtual void OnBundleInstallCanceled(bool user_initiated) OVERRIDE;
  virtual void OnBundleInstallCompleted() OVERRIDE;

 protected:
  virtual ~WebstorePrivateInstallBundleFunction();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

  // Reads the extension |details| into |items|.
  bool ReadBundleInfo(
      const api::webstore_private::InstallBundle::Params& details,
          extensions::BundleInstaller::ItemList* items);

 private:
  scoped_refptr<extensions::BundleInstaller> bundle_;
};

class WebstorePrivateBeginInstallWithManifest3Function
    : public ChromeAsyncExtensionFunction,
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

  // WebstoreInstallHelper::Delegate:
  virtual void OnWebstoreParseSuccess(
      const std::string& id,
      const SkBitmap& icon,
      base::DictionaryValue* parsed_manifest) OVERRIDE;
  virtual void OnWebstoreParseFailure(
      const std::string& id,
      InstallHelperResultCode result_code,
      const std::string& error_message) OVERRIDE;

  // ExtensionInstallPrompt::Delegate:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

 protected:
  virtual ~WebstorePrivateBeginInstallWithManifest3Function();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

  // Sets the result_ as a string based on |code|.
  void SetResultCode(ResultCode code);

 private:
  const char* ResultCodeToString(ResultCode code);

  // These store the input parameters to the function.
  scoped_ptr<api::webstore_private::BeginInstallWithManifest3::Params> params_;

  // The results of parsing manifest_ and icon_data_ go into these two.
  scoped_ptr<base::DictionaryValue> parsed_manifest_;
  SkBitmap icon_;

  // A dummy Extension object we create for the purposes of using
  // ExtensionInstallPrompt to prompt for confirmation of the install.
  scoped_refptr<extensions::Extension> dummy_extension_;

  // The class that displays the install prompt.
  scoped_ptr<ExtensionInstallPrompt> install_prompt_;

  scoped_ptr<ScopedActiveInstall> scoped_active_install_;

  // The authuser query parameter value which should be used with CRX download
  // requests. This is empty if authuser should not be set on download requests.
  std::string authuser_;
};

class WebstorePrivateCompleteInstallFunction
    : public ChromeAsyncExtensionFunction,
      public WebstoreInstaller::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.completeInstall",
                             WEBSTOREPRIVATE_COMPLETEINSTALL)

  WebstorePrivateCompleteInstallFunction();

  // WebstoreInstaller::Delegate:
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(
      const std::string& id,
      const std::string& error,
      WebstoreInstaller::FailureReason reason) OVERRIDE;

 protected:
  virtual ~WebstorePrivateCompleteInstallFunction();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

 private:
  scoped_ptr<WebstoreInstaller::Approval> approval_;
  scoped_ptr<ScopedActiveInstall> scoped_active_install_;

  void OnInstallSuccess(const std::string& id);
};

class WebstorePrivateEnableAppLauncherFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.enableAppLauncher",
                             WEBSTOREPRIVATE_ENABLEAPPLAUNCHER)

  WebstorePrivateEnableAppLauncherFunction();

 protected:
  virtual ~WebstorePrivateEnableAppLauncherFunction();

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class WebstorePrivateGetBrowserLoginFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getBrowserLogin",
                             WEBSTOREPRIVATE_GETBROWSERLOGIN)

 protected:
  virtual ~WebstorePrivateGetBrowserLoginFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class WebstorePrivateGetStoreLoginFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getStoreLogin",
                             WEBSTOREPRIVATE_GETSTORELOGIN)

 protected:
  virtual ~WebstorePrivateGetStoreLoginFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class WebstorePrivateSetStoreLoginFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.setStoreLogin",
                             WEBSTOREPRIVATE_SETSTORELOGIN)

 protected:
  virtual ~WebstorePrivateSetStoreLoginFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class WebstorePrivateGetWebGLStatusFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getWebGLStatus",
                             WEBSTOREPRIVATE_GETWEBGLSTATUS)

  WebstorePrivateGetWebGLStatusFunction();

 protected:
  virtual ~WebstorePrivateGetWebGLStatusFunction();

  void OnFeatureCheck(bool feature_allowed);

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

 private:
  void CreateResult(bool webgl_allowed);

  scoped_refptr<GPUFeatureChecker> feature_checker_;
};

class WebstorePrivateGetIsLauncherEnabledFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getIsLauncherEnabled",
                             WEBSTOREPRIVATE_GETISLAUNCHERENABLED)

  WebstorePrivateGetIsLauncherEnabledFunction() {}

 protected:
  virtual ~WebstorePrivateGetIsLauncherEnabledFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;

 private:
  void OnIsLauncherCheckCompleted(bool is_enabled);
};

class WebstorePrivateIsInIncognitoModeFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.isInIncognitoMode",
                             WEBSTOREPRIVATE_ISININCOGNITOMODEFUNCTION)

  WebstorePrivateIsInIncognitoModeFunction() {}

 protected:
  virtual ~WebstorePrivateIsInIncognitoModeFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class WebstorePrivateLaunchEphemeralAppFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.launchEphemeralApp",
                             WEBSTOREPRIVATE_LAUNCHEPHEMERALAPP)

  WebstorePrivateLaunchEphemeralAppFunction();

 protected:
  virtual ~WebstorePrivateLaunchEphemeralAppFunction();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

 private:
  void OnLaunchComplete(webstore_install::Result result,
                        const std::string& error);
  void SetResult(
      api::webstore_private::LaunchEphemeralApp::Results::Result result,
      const std::string& error);
};

class WebstorePrivateGetEphemeralAppsEnabledFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getEphemeralAppsEnabled",
                             WEBSTOREPRIVATE_GETEPHEMERALAPPSENABLED)

  WebstorePrivateGetEphemeralAppsEnabledFunction();

 protected:
  virtual ~WebstorePrivateGetEphemeralAppsEnabledFunction();

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_
