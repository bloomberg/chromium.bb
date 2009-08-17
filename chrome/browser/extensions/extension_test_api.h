// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_API_H_

#include "chrome/browser/extensions/extension_function.h"

// Function names.
namespace extension_test_api_functions {
  extern const char kPassFunction[];
  extern const char kFailFunction[];
};  // namespace extension_test_api_functions

class ExtensionTestPassFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

class ExtensionTestFailFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TEST_API_H_
