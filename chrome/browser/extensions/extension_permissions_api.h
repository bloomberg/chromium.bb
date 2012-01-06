// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PERMISSIONS_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PERMISSIONS_API_H__
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/common/extensions/extension_permission_set.h"

class Extension;
class ExtensionPermissionSet;
class ExtensionService;

// chrome.permissions.contains
class ContainsPermissionsFunction : public SyncExtensionFunction {
  virtual ~ContainsPermissionsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("permissions.contains")
};

// chrome.permissions.getAll
class GetAllPermissionsFunction : public SyncExtensionFunction {
  virtual ~GetAllPermissionsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("permissions.getAll")
};

// chrome.permissions.remove
class RemovePermissionsFunction : public SyncExtensionFunction {
  virtual ~RemovePermissionsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("permissions.remove")
};

// chrome.permissions.request
class RequestPermissionsFunction : public AsyncExtensionFunction,
                                   public ExtensionInstallUI::Delegate {
 public:
  // FOR TESTS ONLY to bypass the confirmation UI.
  static void SetAutoConfirmForTests(bool should_proceed);
  static void SetIgnoreUserGestureForTests(bool ignore);

  RequestPermissionsFunction();

  // Implementing ExtensionInstallUI::Delegate interface.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

 protected:
  virtual ~RequestPermissionsFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  scoped_ptr<ExtensionInstallUI> install_ui_;
  scoped_refptr<ExtensionPermissionSet> requested_permissions_;
  DECLARE_EXTENSION_FUNCTION_NAME("permissions.request")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PERMISSIONS_API_H__
