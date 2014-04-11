// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_COMMON_SHELL_APP_RUNTIME_H_
#define APPS_SHELL_COMMON_SHELL_APP_RUNTIME_H_

#include "base/macros.h"

namespace extensions {

// Support for a simplified implementation of the chrome.app.runtime API.
class ShellAppRuntime {
 public:
  // Returns the API name.
  static const char* GetName();

  // Returns a simplified schema for the API.
  static const char* GetSchema();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ShellAppRuntime);
};

}  // namespace extensions

#endif  // APPS_SHELL_COMMON_SHELL_APP_RUNTIME_H_
