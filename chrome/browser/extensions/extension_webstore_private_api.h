// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBSTORE_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBSTORE_PRIVATE_API_H_
#pragma once

#include "chrome/browser/browser_signin.h"
#include "chrome/browser/extensions/extension_function.h"
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

class BeginInstallFunction : public SyncExtensionFunction {
 public:
  // For use only in tests - sets a flag that can cause this function to ignore
  // the normal requirement that it is called during a user gesture.
  static void SetIgnoreUserGestureForTests(bool ignore);
 protected:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.beginInstall");
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
