// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class ExtensionsService;

class ExtensionManagementFunction : public SyncExtensionFunction {
 protected:
  ExtensionsService* service();
};

class GetAllExtensionsFunction : public ExtensionManagementFunction {
  ~GetAllExtensionsFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.management.getAll");
};

class SetEnabledFunction : public ExtensionManagementFunction {
  ~SetEnabledFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.management.setEnabled");
};

class InstallFunction : public ExtensionManagementFunction {
  ~InstallFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.management.install");
};

class UninstallFunction : public ExtensionManagementFunction {
  ~UninstallFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.management.uninstall");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_API_H__
