// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef C_SALT_TEST_GTEST_NACL_ENVIRONMENT_H_
#define C_SALT_TEST_GTEST_NACL_ENVIRONMENT_H_

#include <cassert>
#include "gtest/gtest.h"

namespace pp {
class Instance;
}  // namespace pp

namespace c_salt {

// A custom environment for NaCl gtest.
class GTestNaclEnvironment : public ::testing::Environment {
 public:
  // Set the global NaCl instance that will be shared by all the tests.
  static void set_global_instance(pp::Instance* instance) {
    global_instance_ = instance;
  }

  // Get the global NaCl instance.
  static pp::Instance* global_instance() { return global_instance_; }

  // Environment overrides.
  virtual void SetUp();
  virtual void TearDown();

 private:
  static pp::Instance* global_instance_;
};

}  // namespace c_salt

#endif  // C_SALT_TEST_GTEST_NACL_ENVIRONMENT_H_

