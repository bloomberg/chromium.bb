// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/env/public/athena_env.h"

#include "athena/test/athena_test_base.h"
#include "base/bind.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace athena {

typedef test::AthenaTestBase AthenaEnvTest;

namespace {

class TerminatingCallback {
 public:
  TerminatingCallback()
      : on_terminating_count_(0),
        on_terminating_2_count_(0) {
  }

  void OnTerminating() {
    on_terminating_count_ ++;
  }

  void OnTerminating2() {
    on_terminating_2_count_ ++;
  }

  int on_terminating_count() const { return on_terminating_count_; }
  int on_terminating_2_count() const { return on_terminating_2_count_; }

  void Reset() {
    on_terminating_count_ = 0;
    on_terminating_2_count_ = 0;
  }

 private:
  int on_terminating_count_;
  int on_terminating_2_count_;

  DISALLOW_COPY_AND_ASSIGN(TerminatingCallback);
};

}  // namespace

TEST_F(AthenaEnvTest, TerminatingCallback) {
  TerminatingCallback callback_1;
  TerminatingCallback callback_2;
  AthenaEnv* env = AthenaEnv::Get();
  base::Closure cb_1_1 =
      base::Bind(&TerminatingCallback::OnTerminating,
                 base::Unretained(&callback_1));
  base::Closure cb_1_2 =
      base::Bind(&TerminatingCallback::OnTerminating2,
                 base::Unretained(&callback_1));
  base::Closure cb_2_1 =
      base::Bind(&TerminatingCallback::OnTerminating,
                 base::Unretained(&callback_2));

  env->AddTerminatingCallback(cb_1_1);
  env->AddTerminatingCallback(cb_1_2);
  env->AddTerminatingCallback(cb_2_1);
  env->OnTerminating();

  EXPECT_EQ(1, callback_1.on_terminating_count());
  EXPECT_EQ(1, callback_1.on_terminating_2_count());
  EXPECT_EQ(1, callback_2.on_terminating_count());

  // Remove callbacks.
  callback_1.Reset();
  callback_2.Reset();
  env->RemoveTerminatingCallback(cb_1_2);
  env->OnTerminating();
  EXPECT_EQ(1, callback_1.on_terminating_count());
  EXPECT_EQ(0, callback_1.on_terminating_2_count());
  EXPECT_EQ(1, callback_2.on_terminating_count());

  callback_1.Reset();
  callback_2.Reset();
  env->RemoveTerminatingCallback(cb_1_1);
  env->OnTerminating();
  EXPECT_EQ(0, callback_1.on_terminating_count());
  EXPECT_EQ(0, callback_1.on_terminating_2_count());
  EXPECT_EQ(1, callback_2.on_terminating_count());

  // Add removed callback.
  callback_1.Reset();
  callback_2.Reset();
  env->AddTerminatingCallback(cb_1_2);
  env->OnTerminating();
  EXPECT_EQ(0, callback_1.on_terminating_count());
  EXPECT_EQ(1, callback_1.on_terminating_2_count());
  EXPECT_EQ(1, callback_2.on_terminating_count());

  // Adding empty callback should not fail.
  env->AddTerminatingCallback(base::Closure());
  env->OnTerminating();
  env->RemoveTerminatingCallback(cb_1_2);
  env->RemoveTerminatingCallback(cb_2_1);
}

namespace {

class AthenaShutdownTest : public test::AthenaTestBase {
 public:
  AthenaShutdownTest() {}
  virtual ~AthenaShutdownTest() {}

  virtual void TearDown() {
    test::AthenaTestBase::TearDown();
    ASSERT_NE(
        gfx::Display::kInvalidDisplayID,
        gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaShutdownTest);
};

}  // namespace

// gfx::Screen should be accessible after shutdown.
TEST_F(AthenaShutdownTest, Shutdown) {
}

}  // namespace athena
