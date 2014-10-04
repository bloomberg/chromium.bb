// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SHELL_PRIVATE_SHELL_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_SHELL_PRIVATE_SHELL_PRIVATE_API_H_

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Synchronously prints "hello" to the console.
class ShellPrivatePrintHelloFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("shellPrivate.printHello", UNKNOWN);

  ShellPrivatePrintHelloFunction();

 protected:
  virtual ~ShellPrivatePrintHelloFunction();

  // ExtensionFunction:
  virtual ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellPrivatePrintHelloFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SHELL_PRIVATE_SHELL_PRIVATE_API_H_
