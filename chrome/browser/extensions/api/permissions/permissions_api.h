// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "extensions/common/permissions/permission_set.h"

namespace extensions {

// chrome.permissions.contains
class PermissionsContainsFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.contains", PERMISSIONS_CONTAINS)

 protected:
  virtual ~PermissionsContainsFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

// chrome.permissions.getAll
class PermissionsGetAllFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.getAll", PERMISSIONS_GETALL)

 protected:
  virtual ~PermissionsGetAllFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

// chrome.permissions.remove
class PermissionsRemoveFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.remove", PERMISSIONS_REMOVE)

 protected:
  virtual ~PermissionsRemoveFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

// chrome.permissions.request
class PermissionsRequestFunction : public ChromeAsyncExtensionFunction,
                                   public ExtensionInstallPrompt::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.request", PERMISSIONS_REQUEST)

  PermissionsRequestFunction();

  // FOR TESTS ONLY to bypass the confirmation UI.
  static void SetAutoConfirmForTests(bool should_proceed);
  static void SetIgnoreUserGestureForTests(bool ignore);

  // ExtensionInstallPrompt::Delegate:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

 protected:
  virtual ~PermissionsRequestFunction();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

 private:
  scoped_ptr<ExtensionInstallPrompt> install_ui_;
  scoped_refptr<extensions::PermissionSet> requested_permissions_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_H_
