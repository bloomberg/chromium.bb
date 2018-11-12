// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_EXO_TEST_BASE_CAST_H_
#define COMPONENTS_EXO_TEST_EXO_TEST_BASE_CAST_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"

namespace exo {

class WMHelper;

namespace test {

// Basic implementation of ExoTestBase without
// ash dependencies
class ExoTestBaseCast : public aura::test::AuraTestBase {
 public:
  ExoTestBaseCast();
  ~ExoTestBaseCast() override;

  // Overridden from test::Test.
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<WMHelper> wm_helper_;
};

}  // namespace test
}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_EXO_TEST_BASE_CAST_H_
