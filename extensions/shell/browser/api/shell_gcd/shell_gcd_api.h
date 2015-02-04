// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_API_SHELL_GCD_SHELL_GCD_API_H_
#define EXTENSIONS_SHELL_BROWSER_API_SHELL_GCD_SHELL_GCD_API_H_

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Used for manual testing in app_shell. See shell_gcd.idl for documentation.
class ShellGcdPingFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("shell.gcd.ping", UNKNOWN);

  ShellGcdPingFunction();

 protected:
  ~ShellGcdPingFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // Callback for status from DBus call to GCD privet daemon.
  void OnPing(bool success);

  DISALLOW_COPY_AND_ASSIGN(ShellGcdPingFunction);
};

///////////////////////////////////////////////////////////////////////////////

// Used for manual testing in app_shell. See shell_gcd.idl for documentation.
class ShellGcdGetWiFiBootstrapStateFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("shell.gcd.getWiFiBootstrapState", UNKNOWN);

  ShellGcdGetWiFiBootstrapStateFunction();

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~ShellGcdGetWiFiBootstrapStateFunction() override;

  DISALLOW_COPY_AND_ASSIGN(ShellGcdGetWiFiBootstrapStateFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_API_SHELL_GCD_SHELL_GCD_API_H_
