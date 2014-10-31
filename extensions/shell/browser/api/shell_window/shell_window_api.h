// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_API_SHELL_WINDOW_SHELL_WINDOW_API_H_
#define EXTENSIONS_SHELL_BROWSER_API_SHELL_WINDOW_SHELL_WINDOW_API_H_

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// TODO(zork): Implement this function.
class ShellWindowShowAppFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("shell.window.showApp", UNKNOWN);

  ShellWindowShowAppFunction();

 protected:
  ~ShellWindowShowAppFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellWindowShowAppFunction);
};

// TODO(zork): Implement this function.
class ShellWindowRequestShowFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("shell.window.requestShow", UNKNOWN);

  ShellWindowRequestShowFunction();

 protected:
  ~ShellWindowRequestShowFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellWindowRequestShowFunction);
};

// TODO(zork): Implement this function.
class ShellWindowRequestHideFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("shell.window.requestHide", UNKNOWN);

  ShellWindowRequestHideFunction();

 protected:
  ~ShellWindowRequestHideFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellWindowRequestHideFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_API_SHELL_WINDOW_SHELL_WINDOW_API_H_
