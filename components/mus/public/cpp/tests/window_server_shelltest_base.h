// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_SHELLTEST_BASE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_SHELLTEST_BASE_H_

#include "base/macros.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/shell_test.h"

namespace mus {

// Base class for all window manager shelltests to perform some common setup.
class WindowServerShellTestBase : public shell::test::ShellTest {
 public:
  WindowServerShellTestBase();
  ~WindowServerShellTestBase() override;

  virtual bool AcceptConnection(shell::Connection* connection) = 0;

 private:
  // shell::test::ShellTest:
  std::unique_ptr<shell::ShellClient> CreateShellClient() override;

  DISALLOW_COPY_AND_ASSIGN(WindowServerShellTestBase);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_SHELLTEST_BASE_H_
