// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_APP_WINDOW_API_H_
#define APPS_SHELL_BROWSER_SHELL_APP_WINDOW_API_H_

#include "base/compiler_specific.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// A simplified implementation of the chrome.app.window.create() function for
// app_shell. Opens a fullscreen window and invokes the window callback. Most
// of the response is stub data, but the JS contentWindow is valid.
class ShellAppWindowCreateFunction : public UIThreadExtensionFunction {
 public:
  ShellAppWindowCreateFunction();

  // Don't use the APP_WINDOW_CREATE histogram so we don't pollute the
  // statistics for the real Chrome implementation.
  DECLARE_EXTENSION_FUNCTION("app.window.create", UNKNOWN);

 private:
  virtual ~ShellAppWindowCreateFunction();
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // APPS_SHELL_BROWSER_SHELL_APP_WINDOW_API_H_
