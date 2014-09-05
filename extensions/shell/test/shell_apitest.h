// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_TEST_SHELL_API_TEST_H_
#define EXTENSIONS_SHELL_TEST_SHELL_API_TEST_H_

#include <string>

#include "extensions/shell/test/shell_test.h"

namespace extensions {

// Base class for app shell browser API tests that load an app/extension
// and wait for a success message from the chrome.test API.
class ShellApiTest : public AppShellTest {
 public:
  ShellApiTest();
  virtual ~ShellApiTest();

  // Loads an unpacked platform app from a directory using the current
  // ExtensionSystem, launches it, and waits for a chrome.test success
  // notification. Returns true if the test succeeds. |app_dir| is a
  // subpath under extensions/test/data.
  bool RunAppTest(const std::string& app_dir);

 protected:
  // If it failed, what was the error message?
  std::string message_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellApiTest);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_TEST_SHELL_API_TEST_H_
