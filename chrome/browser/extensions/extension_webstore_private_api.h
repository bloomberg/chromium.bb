// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBSTORE_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBSTORE_PRIVATE_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"

class ProfileSyncService;

class InstallFunction : public SyncExtensionFunction {
 public:
  static void SetTestingInstallBaseUrl(const char* testing_install_base_url);

 protected:
  ~InstallFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.install");
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
                                   public ProfileSyncServiceObserver {
 public:
  // Allows you to set the ProfileSyncService the function will use for
  // testing purposes.
  static void SetTestingProfileSyncService(ProfileSyncService* service);

  // Implements ProfileSyncServiceObserver interface.
  virtual void OnStateChanged();

 protected:
  virtual ~PromptBrowserLoginFunction();
  virtual bool RunImpl();

  // Returns either the actual ProfileSyncService or the test service if one
  // was set.
  ProfileSyncService* profile_sync_service();

  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.promptBrowserLogin");

 private:
  // This indicates whether we're currently registered as an observer of the
  // ProfileSyncService, and need to unregister ourselves at destruction.
  bool observing_sync_state_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBSTORE_PRIVATE_API_H_
