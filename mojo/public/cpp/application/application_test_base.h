// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_APPLICATION_APPLICATION_TEST_BASE_H_
#define MOJO_PUBLIC_CPP_APPLICATION_APPLICATION_TEST_BASE_H_

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/system/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

class ApplicationDelegate;
class ApplicationImpl;

namespace test {

// A GTEST base class for application testing executed in mojo_shell.
class ApplicationTestBase : public testing::Test {
 public:
  explicit ApplicationTestBase(Array<String> args);
  ~ApplicationTestBase() override;

 protected:
  ApplicationImpl* application_impl() { return application_impl_; }

  // Get the ApplicationDelegate for the application to be tested.
  virtual ApplicationDelegate* GetApplicationDelegate() = 0;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 private:
  // The command line arguments supplied to each test application instance.
  Array<String> args_;

  // The application implementation instance, reconstructed for each test.
  ApplicationImpl* application_impl_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ApplicationTestBase);
};

}  // namespace test

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_APPLICATION_APPLICATION_TEST_BASE_H_
