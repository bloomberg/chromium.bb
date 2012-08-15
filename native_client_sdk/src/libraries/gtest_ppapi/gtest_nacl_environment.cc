// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "gtest_ppapi/gtest_nacl_environment.h"

pp::Instance* GTestNaclEnvironment::global_instance_ = NULL;

void GTestNaclEnvironment::SetUp() {
  // Check that the global instance is set before running the tests.
  assert(global_instance_ != NULL);
}

void GTestNaclEnvironment::TearDown() {
}
