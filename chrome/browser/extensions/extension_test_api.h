// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_API_H_

#include "chrome/browser/extensions/extension_function.h"

class ExtensionTestPassFunction : public SyncExtensionFunction {
  ~ExtensionTestPassFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("test.notifyPass")
};

class ExtensionTestFailFunction : public SyncExtensionFunction {
  ~ExtensionTestFailFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("test.notifyFail")
};

class ExtensionTestLogFunction : public SyncExtensionFunction {
  ~ExtensionTestLogFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("test.log")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_API_H_
