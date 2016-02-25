// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "mojo/shell/tests/capability_filter_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace test {

class CapabilityFilterApplicationTest : public CapabilityFilterTest {
 public:
  CapabilityFilterApplicationTest() {}
  ~CapabilityFilterApplicationTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CapabilityFilterApplicationTest);
};

TEST_F(CapabilityFilterApplicationTest, DISABLED_Blocking) {
  CreateLoader<TestApplication>("test:trusted");
  CreateLoader<TestApplication>("test:untrusted");
  RunBlockingTest();
}

TEST_F(CapabilityFilterApplicationTest, DISABLED_Wildcards) {
  CreateLoader<TestApplication>("test:wildcard");
  CreateLoader<TestApplication>("test:blocked");
  CreateLoader<TestApplication>("test:wildcard2");
  CreateLoader<TestApplication>("test:wildcard3");
  RunWildcardTest();
}

}  // namespace test
}  // namespace shell
}  // namespace mojo
