// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/virtual_time_tqm_delegate.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scheduler {

class VirtualTimeTqmDelegateTest : public testing::Test {
 public:
  VirtualTimeTqmDelegateTest() {}

  ~VirtualTimeTqmDelegateTest() override {}

  void SetUp() override {
    message_loop_.reset(new base::MessageLoop());
    initial_time_ = base::TimeTicks() + base::TimeDelta::FromSeconds(100);
    virtual_time_tqm_delegate_ =
        VirtualTimeTqmDelegate::Create(message_loop_.get(), initial_time_);
  }

  base::TimeTicks initial_time_;
  scoped_ptr<base::MessageLoop> message_loop_;

  scoped_refptr<VirtualTimeTqmDelegate> virtual_time_tqm_delegate_;
};

namespace {
void TestFunc(bool* run) {
  *run = true;
}
}

TEST_F(VirtualTimeTqmDelegateTest, OnNoMoreImmediateWork_TimersFastForward) {
  bool was_run = false;
  virtual_time_tqm_delegate_->PostDelayedTask(FROM_HERE,
                                              base::Bind(TestFunc, &was_run),
                                              base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(initial_time_, virtual_time_tqm_delegate_->NowTicks());

  virtual_time_tqm_delegate_->OnNoMoreImmediateWork();

  EXPECT_EQ(initial_time_ + base::TimeDelta::FromSeconds(1),
            virtual_time_tqm_delegate_->NowTicks());

  message_loop_->RunUntilIdle();
  EXPECT_TRUE(was_run);
}

}  // namespace scheduler
