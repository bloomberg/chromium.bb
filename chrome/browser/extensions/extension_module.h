// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MODULE_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MODULE_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class ExtensionPrefs;

class SetUpdateUrlDataFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("extension.setUpdateUrlData");

 protected:
  virtual ~SetUpdateUrlDataFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  ExtensionPrefs* extension_prefs();
};

class IsAllowedIncognitoAccessFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("extension.isAllowedIncognitoAccess");

 protected:
  virtual ~IsAllowedIncognitoAccessFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class IsAllowedFileSchemeAccessFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("extension.isAllowedFileSchemeAccess");

 protected:
  virtual ~IsAllowedFileSchemeAccessFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MODULE_H__
