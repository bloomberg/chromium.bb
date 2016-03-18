// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_SHELL_TEST_H_
#define MOJO_SHELL_PUBLIC_CPP_SHELL_TEST_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class MessageLoop;
}

namespace mojo {
namespace shell {
class BackgroundShell;
}
namespace test {

class ShellTest;

// A default implementation of ShellClient for use in ShellTests. Tests wishing
// to customize this should subclass this class instead of ShellClient,
// otherwise they will have to call ShellTest::InitializeCalled() to forward
// metadata from Initialize() to the test.
class ShellTestClient : public mojo::ShellClient {
 public:
  explicit ShellTestClient(ShellTest* test);
  ~ShellTestClient() override;

 protected:
  void Initialize(Connector* connector,
                  const Identity& identity,
                  uint32_t id) override;

 private:
  ShellTest* test_;

  DISALLOW_COPY_AND_ASSIGN(ShellTestClient);
};

class ShellTest : public testing::Test {
 public:
  ShellTest();
  // Initialize passing the name to use as the identity for the test itself.
  // Once set via this constructor, it cannot be changed later by calling
  // InitTestName(). The test executable must provide a manifest in the
  // appropriate location that specifies this name also.
  explicit ShellTest(const std::string& test_name);
  ~ShellTest() override;

 protected:
  // See constructor. Can only be called once.
  void InitTestName(const std::string& test_name);

  Connector* connector() { return connector_; }

  // Instance information received from the Shell during Initialize().
  const std::string& test_name() const { return initialize_name_; }
  const std::string& test_userid() const { return initialize_userid_; }
  uint32_t test_instance_id() const { return initialize_instance_id_; }

  // By default, creates a simple ShellClient that captures the metadata sent
  // via Initialize(). Override to customize, but custom implementations must
  // call InitializeCalled() to forward the metadata so test_name() etc all
  // work.
  virtual scoped_ptr<ShellClient> CreateShellClient();

  virtual scoped_ptr<base::MessageLoop> CreateMessageLoop();

  // Call to set Initialize() metadata when GetShellClient() is overridden.
  void InitializeCalled(Connector* connector,
                        const std::string& name,
                        const std::string& userid,
                        uint32_t id);

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 private:
  friend ShellTestClient;

  scoped_ptr<ShellClient> shell_client_;

  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<shell::BackgroundShell> background_shell_;
  scoped_ptr<ShellConnection> shell_connection_;

  // See constructor.
  std::string test_name_;

  Connector* connector_ = nullptr;
  std::string initialize_name_;
  std::string initialize_userid_ = shell::mojom::kInheritUserID;
  uint32_t initialize_instance_id_ = shell::mojom::kInvalidInstanceID;

  base::Closure initialize_called_;

  DISALLOW_COPY_AND_ASSIGN(ShellTest);
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_SHELL_TEST_H_
