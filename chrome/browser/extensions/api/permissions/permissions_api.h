// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/common/extensions/permissions/permission_set.h"

class ExtensionService;

// chrome.permissions.contains
class PermissionsContainsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.contains", PERMISSIONS_CONTAINS)

 protected:
  virtual ~PermissionsContainsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// chrome.permissions.getAll
class PermissionsGetAllFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.getAll", PERMISSIONS_GETALL)

 protected:
  virtual ~PermissionsGetAllFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// chrome.permissions.remove
class PermissionsRemoveFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.remove", PERMISSIONS_REMOVE)

 protected:
  virtual ~PermissionsRemoveFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// chrome.permissions.request
class PermissionsRequestFunction : public AsyncExtensionFunction,
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
  virtual bool RunImpl() OVERRIDE;

 private:
  scoped_ptr<ExtensionInstallPrompt> install_ui_;
  scoped_refptr<extensions::PermissionSet> requested_permissions_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_H_
