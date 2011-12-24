// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SUITE_H_
#define ASH_TEST_TEST_SUITE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/test/test_suite.h"

namespace ash {
namespace test {

class AuraShellTestSuite : public base::TestSuite {
 public:
  AuraShellTestSuite(int argc, char** argv);

 protected:
  // base::TestSuite:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SUITE_H_
