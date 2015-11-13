// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_RUNNER_SHELL_TEST_BASE_H_
#define MOJO_RUNNER_SHELL_TEST_BASE_H_

#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/runner/context.h"
#include "testing/gtest/include/gtest/gtest.h"

class GURL;

namespace mojo {
namespace runner {
namespace test {

class ShellTestBase : public testing::Test {
 public:
  ShellTestBase();
  ~ShellTestBase() override;

  void SetUp() override;
  void TearDown() override;

  // |application_url| should typically be a mojo: URL (the origin will be set
  // to an "appropriate" file: URL).
  // TODO(tim): Should the test base be a ServiceProvider?
  ScopedMessagePipeHandle ConnectToService(const GURL& application_url,
                                           const std::string& service_name);

  template <typename Interface>
  void ConnectToService(const GURL& application_url,
                        InterfacePtr<Interface>* ptr) {
    ptr->Bind(InterfacePtrInfo<Interface>(
        ConnectToService(application_url, Interface::Name_).Pass(), 0u));
  }

  base::MessageLoop* message_loop() { return &message_loop_; }
  Context* shell_context() { return &shell_context_; }

 private:
  base::FilePath GetTestAppFilePath() const;

  Context shell_context_;
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ShellTestBase);
};

}  // namespace test
}  // namespace runner
}  // namespace mojo

#endif  // MOJO_RUNNER_SHELL_TEST_BASE_H_
