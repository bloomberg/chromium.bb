// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_API_SHELL_GCD_SHELL_GCD_API_H_
#define EXTENSIONS_SHELL_BROWSER_API_SHELL_GCD_SHELL_GCD_API_H_

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// See shell_gcd.idl for documentation.
class ShellGcdGetSetupStatusFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("shell.gcd.getSetupStatus", UNKNOWN);

  ShellGcdGetSetupStatusFunction();

 protected:
  ~ShellGcdGetSetupStatusFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // Callback for status from DBus call to GCD privet daemon.
  void OnSetupStatus(const std::string& status_string);

  DISALLOW_COPY_AND_ASSIGN(ShellGcdGetSetupStatusFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_API_SHELL_GCD_SHELL_GCD_API_H_
