// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_

#include <string>

#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/gpu_feature_checker.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/google_service_auth_error.h"

class ProfileSyncService;

namespace content {
class GpuDataManager;
}

namespace extensions {

class WebstorePrivateApi {
 public:
  // Allows you to set the ProfileSyncService the function will use for
  // testing purposes.
  static void SetTestingProfileSyncService(ProfileSyncService* service);

  // Allows you to override the WebstoreInstaller delegate for testing.
  static void SetWebstoreInstallerDelegateForTesting(
      WebstoreInstaller::Delegate* delegate);

  // Gets the pending approval for the |extension_id| in |profile|. Pending
  // approvals are held between the calls to beginInstallWithManifest and
  // completeInstall. This should only be used for testing.
  static scoped_ptr<WebstoreInstaller::Approval> PopApprovalForTesting(
      Profile* profile, const std::string& extension_id);
};

class InstallBundleFunction : public AsyncExtensionFunction,
                              public extensions::BundleInstaller::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.installBundle");

  InstallBundleFunction();

  // BundleInstaller::Delegate:
  virtual void OnBundleInstallApproved() OVERRIDE;
  virtual void OnBundleInstallCanceled(bool user_initiated) OVERRIDE;
  virtual void OnBundleInstallCompleted() OVERRIDE;

 protected:
  virtual ~InstallBundleFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // Reads the extension |details| into |items|.
  bool ReadBundleInfo(base::ListValue* details,
                      extensions::BundleInstaller::ItemList* items);

 private:
  scoped_refptr<extensions::BundleInstaller> bundle_;
};

class BeginInstallWithManifestFunction
    : public AsyncExtensionFunction,
      public ExtensionInstallPrompt::Delegate,
      public WebstoreInstallHelper::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.beginInstallWithManifest3");

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
    INVALID_ICON_URL
  };

  BeginInstallWithManifestFunction();

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
  virtual ~BeginInstallWithManifestFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // Sets the result_ as a string based on |code|.
  void SetResultCode(ResultCode code);

 private:
  // These store the input parameters to the function.
  std::string id_;
  std::string manifest_;
  std::string icon_data_;
  std::string localized_name_;
  bool use_app_installed_bubble_;

  // The results of parsing manifest_ and icon_data_ go into these two.
  scoped_ptr<base::DictionaryValue> parsed_manifest_;
  SkBitmap icon_;

  // A dummy Extension object we create for the purposes of using
  // ExtensionInstallPrompt to prompt for confirmation of the install.
  scoped_refptr<extensions::Extension> dummy_extension_;

  // The class that displays the install prompt.
  scoped_ptr<ExtensionInstallPrompt> install_prompt_;
};

class CompleteInstallFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.completeInstall");

 protected:
  virtual ~CompleteInstallFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class GetBrowserLoginFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.getBrowserLogin");

 protected:
  virtual ~GetBrowserLoginFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class GetStoreLoginFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.getStoreLogin");

 protected:
  virtual ~GetStoreLoginFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class SetStoreLoginFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.setStoreLogin");

 protected:
  virtual ~SetStoreLoginFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class GetWebGLStatusFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.getWebGLStatus");

  GetWebGLStatusFunction();

 protected:
  virtual ~GetWebGLStatusFunction();

  void OnFeatureCheck(bool feature_allowed);

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void CreateResult(bool webgl_allowed);

  scoped_refptr<GPUFeatureChecker> feature_checker_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_
