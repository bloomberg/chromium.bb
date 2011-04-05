// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBSTORE_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBSTORE_PRIVATE_API_H_
#pragma once

#include <string>

#include "chrome/browser/browser_signin.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class ProfileSyncService;

class WebstorePrivateApi {
 public:
  // Allows you to set the ProfileSyncService the function will use for
  // testing purposes.
  static void SetTestingProfileSyncService(ProfileSyncService* service);

  // Allows you to set the BrowserSignin the function will use for
  // testing purposes.
  static void SetTestingBrowserSignin(BrowserSignin* signin);
};

// TODO(asargent): this is being deprecated in favor of
// BeginInstallWithManifestFunction. See crbug.com/75821 for details.
class BeginInstallFunction : public SyncExtensionFunction {
 public:
  // For use only in tests - sets a flag that can cause this function to ignore
  // the normal requirement that it is called during a user gesture.
  static void SetIgnoreUserGestureForTests(bool ignore);
 protected:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.beginInstall");
};

class BeginInstallWithManifestFunction : public AsyncExtensionFunction,
                                         public ExtensionInstallUI::Delegate {
 public:
  BeginInstallWithManifestFunction();

  // Result codes for the return value. If you change this, make sure to
  // update the description for the beginInstallWithManifest callback in
  // extension_api.json.
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

    // The function was not called during a user gesture.
    NO_GESTURE,
  };

  // For use only in tests - sets a flag that can cause this function to ignore
  // the normal requirement that it is called during a user gesture.
  static void SetIgnoreUserGestureForTests(bool ignore);

  // Called when we've successfully parsed the manifest and icon data in the
  // utility process. Ownership of parsed_manifest is transferred.
  void OnParseSuccess(const SkBitmap& icon, DictionaryValue* parsed_manifest);

  // Called to indicate a parse failure. The |result_code| parameter should
  // indicate whether the problem was with the manifest or icon data.
  void OnParseFailure(ResultCode result_code, const std::string& error_message);

  // Implementing ExtensionInstallUI::Delegate interface.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort() OVERRIDE;

 protected:
  virtual ~BeginInstallWithManifestFunction();
  virtual bool RunImpl();

  // Sets the result_ as a string based on |code|.
  void SetResult(ResultCode code);

 private:
  // These store the input parameters to the function.
  std::string id_;
  std::string manifest_;
  std::string icon_data_;

  // The results of parsing manifest_ and icon_data_ go into these two.
  scoped_ptr<DictionaryValue> parsed_manifest_;
  SkBitmap icon_;

  // A dummy Extension object we create for the purposes of using
  // ExtensionInstallUI to prompt for confirmation of the install.
  scoped_refptr<Extension> dummy_extension_;
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.beginInstallWithManifest");
};

class CompleteInstallFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.completeInstall");
};

class GetBrowserLoginFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.getBrowserLogin");
};

class GetStoreLoginFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.getStoreLogin");
};

class SetStoreLoginFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.setStoreLogin");
};

class PromptBrowserLoginFunction : public AsyncExtensionFunction,
                                   public NotificationObserver,
                                   public BrowserSignin::SigninDelegate {
 public:
  PromptBrowserLoginFunction();
  // Implements BrowserSignin::SigninDelegate interface.
  virtual void OnLoginSuccess();
  virtual void OnLoginFailure(const GoogleServiceAuthError& error);

  // Implements the NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  virtual ~PromptBrowserLoginFunction();
  virtual bool RunImpl();

 private:
  // Creates the message for signing in.
  virtual string16 GetLoginMessage();

  // Are we waiting for a token available notification?
  bool waiting_for_token_;

  // Used for listening for TokenService notifications.
  NotificationRegistrar registrar_;

  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.promptBrowserLogin");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBSTORE_PRIVATE_API_H_
