// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "c_salt/test/gtest_nacl_environment.h"

namespace c_salt {

pp::Instance* GTestNaclEnvironment::global_instance_ = NULL;

void GTestNaclEnvironment::SetUp() {
  // Check that the global instance is set before running the tests.
  assert(global_instance_ != NULL);
}

void GTestNaclEnvironment::TearDown() {
}

}  // namespace c_salt

