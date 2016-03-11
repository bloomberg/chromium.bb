// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_APPLICATION_TEST_BASE_H_
#define MOJO_SHELL_PUBLIC_CPP_APPLICATION_TEST_BASE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

class ShellConnection;

namespace test {

// Run all application tests. This must be called after the environment is
// initialized, to support construction of a default run loop.
MojoResult RunAllTests(MojoHandle shell_client_request_handle);

// Used to configure the ShellConnection. This is used internally by
// ApplicationTestBase, but useful if you do not want to subclass
// ApplicationTestBase.
class TestHelper {
 public:
  explicit TestHelper(ShellClient* client);
  ~TestHelper();

  Connector* connector() { return shell_connection_->connector(); }
  const std::string& test_name() { return name_; }
  const std::string& test_userid() { return userid_; }
  uint32_t test_instance_id() { return instance_id_; }

 private:
  // The application delegate used if GetShellClient is not overridden.
  ShellClient default_shell_client_;

  // The application implementation instance, reconstructed for each test.
  scoped_ptr<ShellConnection> shell_connection_;

  std::string name_;
  std::string userid_;
  uint32_t instance_id_;

  DISALLOW_COPY_AND_ASSIGN(TestHelper);
};

// A GTEST base class for application testing executed in mojo_shell.
class ApplicationTestBase : public testing::Test {
 public:
  ApplicationTestBase();
  ~ApplicationTestBase() override;

 protected:
  Connector* connector() {
    return test_helper_ ? test_helper_->connector() : nullptr;
  }
  const std::string& test_name() const {
    return test_helper_ ? test_helper_->test_name() : empty_;
  }
  const std::string& test_userid() const {
    return test_helper_ ? test_helper_->test_userid() : inherit_user_id_;
  }
  uint32_t test_instance_id() const {
    return test_helper_ ? test_helper_->test_instance_id() :
        shell::mojom::kInvalidInstanceID;
  }

  // Get the ShellClient for the application to be tested.
  virtual ShellClient* GetShellClient();

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // True by default, which indicates a MessageLoop will automatically be
  // created for the application.  Tests may override this function to prevent
  // a default loop from being created.
  virtual bool ShouldCreateDefaultRunLoop();

 private:
  scoped_ptr<TestHelper> test_helper_;
  std::string empty_;
  std::string inherit_user_id_ = shell::mojom::kInheritUserID;

  DISALLOW_COPY_AND_ASSIGN(ApplicationTestBase);
};

}  // namespace test

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_APPLICATION_TEST_BASE_H_
