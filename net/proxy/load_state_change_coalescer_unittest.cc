// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/load_state_change_coalescer.h"

#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class LoadStateChangeCoalescerTest : public testing::Test {
 protected:
  void SetUp() override {
    coalescer_.reset(new LoadStateChangeCoalescer(
        base::Bind(&LoadStateChangeCoalescerTest::LoadStateChanged,
                   base::Unretained(this)),
        base::TimeDelta(), LOAD_STATE_IDLE));
  }

  void TearDown() override { coalescer_.reset(); }

  void LoadStateChanged(LoadState load_state) {
    load_state_changes_.push_back(load_state);
  }

  void WaitUntilIdle() { base::RunLoop().RunUntilIdle(); }

  scoped_ptr<LoadStateChangeCoalescer> coalescer_;
  std::vector<LoadState> load_state_changes_;
};

TEST_F(LoadStateChangeCoalescerTest, SingleChange) {
  coalescer_->LoadStateChanged(LOAD_STATE_RESOLVING_PROXY_FOR_URL);
  WaitUntilIdle();
  ASSERT_EQ(1u, load_state_changes_.size());
  EXPECT_EQ(LOAD_STATE_RESOLVING_PROXY_FOR_URL, load_state_changes_[0]);
}

TEST_F(LoadStateChangeCoalescerTest, TwoChangesCoalesce) {
  coalescer_->LoadStateChanged(LOAD_STATE_RESOLVING_PROXY_FOR_URL);
  coalescer_->LoadStateChanged(LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT);
  WaitUntilIdle();
  ASSERT_EQ(1u, load_state_changes_.size());
  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT, load_state_changes_[0]);
}

TEST_F(LoadStateChangeCoalescerTest, ThreeChangesCoalesce) {
  coalescer_->LoadStateChanged(LOAD_STATE_RESOLVING_PROXY_FOR_URL);
  coalescer_->LoadStateChanged(LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT);
  coalescer_->LoadStateChanged(LOAD_STATE_DOWNLOADING_PROXY_SCRIPT);
  WaitUntilIdle();
  ASSERT_EQ(1u, load_state_changes_.size());
  EXPECT_EQ(LOAD_STATE_DOWNLOADING_PROXY_SCRIPT, load_state_changes_[0]);
}

TEST_F(LoadStateChangeCoalescerTest, CoalesceToOriginalLoadState) {
  coalescer_->LoadStateChanged(LOAD_STATE_RESOLVING_PROXY_FOR_URL);
  coalescer_->LoadStateChanged(LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT);
  coalescer_->LoadStateChanged(LOAD_STATE_IDLE);
  WaitUntilIdle();
  EXPECT_TRUE(load_state_changes_.empty());
}

TEST_F(LoadStateChangeCoalescerTest, AlternateLoadStatesWithWait) {
  coalescer_->LoadStateChanged(LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT);
  coalescer_->LoadStateChanged(LOAD_STATE_RESOLVING_PROXY_FOR_URL);
  WaitUntilIdle();
  ASSERT_EQ(1u, load_state_changes_.size());
  EXPECT_EQ(LOAD_STATE_RESOLVING_PROXY_FOR_URL, load_state_changes_[0]);

  coalescer_->LoadStateChanged(LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT);
  coalescer_->LoadStateChanged(LOAD_STATE_IDLE);
  WaitUntilIdle();
  ASSERT_EQ(2u, load_state_changes_.size());
  EXPECT_EQ(LOAD_STATE_IDLE, load_state_changes_[1]);
}

}  // namespace net
