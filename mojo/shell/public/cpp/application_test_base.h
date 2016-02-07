// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_APPLICATION_TEST_BASE_H_
#define MOJO_SHELL_PUBLIC_CPP_APPLICATION_TEST_BASE_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/interfaces/application.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

class ApplicationImpl;

namespace test {

// Run all application tests. This must be called after the environment is
// initialized, to support construction of a default run loop.
MojoResult RunAllTests(MojoHandle application_request_handle);

// Used to configure the ApplicationImpl. This is used internally by
// ApplicationTestBase, but useful if you do not want to subclass
// ApplicationTestBase.
class TestHelper {
 public:
  explicit TestHelper(ApplicationDelegate* delegate);
  ~TestHelper();

  Shell* shell() { return application_impl_.get(); }
  std::string shell_url() { return url_; }

 private:
  // The application delegate used if GetApplicationDelegate is not overridden.
  ApplicationDelegate default_application_delegate_;

  // The application implementation instance, reconstructed for each test.
  scoped_ptr<ApplicationImpl> application_impl_;

  std::string url_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(TestHelper);
};

// A GTEST base class for application testing executed in mojo_shell.
class ApplicationTestBase : public testing::Test {
 public:
  ApplicationTestBase();
  ~ApplicationTestBase() override;

 protected:
  Shell* shell() {
    return test_helper_ ? test_helper_->shell() : nullptr;
  }
  std::string shell_url() const {
    return test_helper_ ? test_helper_->shell_url() : std::string();
  }

  // Get the ApplicationDelegate for the application to be tested.
  virtual ApplicationDelegate* GetApplicationDelegate();

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // True by default, which indicates a MessageLoop will automatically be
  // created for the application.  Tests may override this function to prevent
  // a default loop from being created.
  virtual bool ShouldCreateDefaultRunLoop();

 private:
  scoped_ptr<TestHelper> test_helper_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ApplicationTestBase);
};

}  // namespace test

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_APPLICATION_TEST_BASE_H_
