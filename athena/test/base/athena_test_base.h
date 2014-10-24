// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_BASE_ATHENA_TEST_BASE_H_
#define ATHENA_TEST_BASE_ATHENA_TEST_BASE_H_

#include "athena/test/base/athena_test_helper.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace aura {
class Window;
class WindowDelegate;
}

namespace athena {
namespace test {

// A base class for athena unit tests.
class AthenaTestBase : public testing::Test {
 public:
  AthenaTestBase();
  ~AthenaTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 protected:
  void RunAllPendingInMessageLoop();

  aura::Window* root_window() { return helper_->GetRootWindow(); }
  aura::WindowTreeHost* host() { return helper_->GetHost(); }

 private:
  bool setup_called_;
  bool teardown_called_;

  base::MessageLoopForUI message_loop_;
  scoped_ptr<AthenaTestHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(AthenaTestBase);
};

}  // namespace test
}  // namespace athena

#endif  // ATHENA_TEST_BASE_ATHENA_TEST_BASE_H_
