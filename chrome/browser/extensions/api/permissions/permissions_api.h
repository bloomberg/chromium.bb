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
class ContainsPermissionsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("permissions.contains")

 protected:
  virtual ~ContainsPermissionsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// chrome.permissions.getAll
class GetAllPermissionsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("permissions.getAll")

 protected:
  virtual ~GetAllPermissionsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// chrome.permissions.remove
class RemovePermissionsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("permissions.remove")

 protected:
  virtual ~RemovePermissionsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// chrome.permissions.request
class RequestPermissionsFunction : public AsyncExtensionFunction,
                                   public ExtensionInstallPrompt::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("permissions.request")

  RequestPermissionsFunction();

  // FOR TESTS ONLY to bypass the confirmation UI.
  static void SetAutoConfirmForTests(bool should_proceed);
  static void SetIgnoreUserGestureForTests(bool ignore);

  // ExtensionInstallPrompt::Delegate:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

 protected:
  virtual ~RequestPermissionsFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  scoped_ptr<ExtensionInstallPrompt> install_ui_;
  scoped_refptr<extensions::PermissionSet> requested_permissions_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_H_
