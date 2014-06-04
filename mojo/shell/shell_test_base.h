// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SHELL_TEST_BASE_H_
#define MOJO_SHELL_SHELL_TEST_BASE_H_

#include "base/at_exit.h"
#include "base/macros.h"
#include "mojo/public/cpp/environment/environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace test {

class ShellTestBase : public testing::Test {
 public:
  ShellTestBase();
  virtual ~ShellTestBase();

 private:
  base::ShadowingAtExitManager at_exit_manager_;
  Environment environment_;

  DISALLOW_COPY_AND_ASSIGN(ShellTestBase);
};

}  // namespace test
}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SYSTEM_SHELL_SHELLASE_H_
