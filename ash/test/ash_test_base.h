// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_BASE_H_
#define ASH_TEST_ASH_TEST_BASE_H_
#pragma once

#include "ash/shell.h"
#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_helper.h"

namespace ash {
namespace test {

class AshTestBase : public testing::Test {
 public:
  AshTestBase();
  virtual ~AshTestBase();

  MessageLoopForUI* message_loop() { return helper_.message_loop(); }

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  void RunAllPendingInMessageLoop();

 private:
  aura::test::AuraTestHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(AshTestBase);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_BASE_H_
