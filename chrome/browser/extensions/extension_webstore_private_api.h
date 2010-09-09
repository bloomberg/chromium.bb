// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBSTORE_PRIVATE_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBSTORE_PRIVATE_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class InstallFunction : public SyncExtensionFunction {
 public:
  static void SetTestingInstallBaseUrl(const char* testing_install_base_url);

 protected:
  ~InstallFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.install");
};

class GetSyncLoginFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.getSyncLogin");
};

class GetStoreLoginFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.getStoreLogin");
};

class SetStoreLoginFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("webstorePrivate.setStoreLogin");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBSTORE_PRIVATE_API_H__
