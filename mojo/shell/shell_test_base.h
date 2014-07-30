// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SHELL_TEST_BASE_H_
#define MOJO_SHELL_SHELL_TEST_BASE_H_

#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/shell/context.h"
#include "testing/gtest/include/gtest/gtest.h"

class GURL;

namespace net {
namespace test_server {
class EmbeddedTestServer;
}
}  // namespace net

namespace mojo {
namespace shell {
namespace test {

class ShellTestBase : public testing::Test {
 public:
  ShellTestBase();
  virtual ~ShellTestBase();

  virtual void SetUp() OVERRIDE;

  // |application_url| should typically be a mojo: URL (the origin will be set
  // to an "appropriate" file: URL).
  // TODO(tim): Should the test base be a ServiceProvider?
  ScopedMessagePipeHandle ConnectToService(
      const GURL& application_url,
      const std::string& service_name);

  ScopedMessagePipeHandle ConnectToServiceViaNetwork(
      const GURL& application_url,
      const std::string& service_name);

  base::MessageLoop* message_loop() { return &message_loop_; }
  Context* shell_context() { return &shell_context_; }

 private:
  scoped_ptr<net::test_server::EmbeddedTestServer> test_server_;
  Context shell_context_;
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ShellTestBase);
};

}  // namespace test
}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SHELL_TEST_BASE_H_
