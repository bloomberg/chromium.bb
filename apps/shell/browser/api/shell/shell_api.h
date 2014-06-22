// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_API_SHELL_SHELL_API_H_
#define APPS_SHELL_BROWSER_API_SHELL_SHELL_API_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace apps {

// Implementation of the chrome.shell.createWindow() function for app_shell.
// Opens a fullscreen window and invokes the window callback to allow access to
// the JS contentWindow.
class ShellCreateWindowFunction : public UIThreadExtensionFunction {
 public:
  ShellCreateWindowFunction();

  DECLARE_EXTENSION_FUNCTION("shell.createWindow", SHELL_CREATEWINDOW);

 private:
  virtual ~ShellCreateWindowFunction();
  virtual ResponseAction Run() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ShellCreateWindowFunction);
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_API_SHELL_SHELL_API_H_
