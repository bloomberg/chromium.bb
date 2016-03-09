// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_SHELLTEST_BASE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_SHELLTEST_BASE_H_

#include "base/macros.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/shell_test.h"

namespace mus {

// Base class for all window manager shelltests to perform some common setup.
class WindowServerShellTestBase : public mojo::test::ShellTest {
 public:
  WindowServerShellTestBase();
  ~WindowServerShellTestBase() override;

  virtual bool AcceptConnection(mojo::Connection* connection) = 0;

 private:
  // mojo::test::ShellTest:
  scoped_ptr<mojo::ShellClient> CreateShellClient() override;

  DISALLOW_COPY_AND_ASSIGN(WindowServerShellTestBase);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_SHELLTEST_BASE_H_
