// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_AURA_SHELL_TEST_BASE_H_
#define ASH_TEST_AURA_SHELL_TEST_BASE_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/aura/test/aura_test_base.h"

namespace ash {
namespace test {

class AuraShellTestBase : public aura::test::AuraTestBase {
 public:
  AuraShellTestBase();
  virtual ~AuraShellTestBase();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AuraShellTestBase);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_AURA_SHELL_TEST_BASE_H_
