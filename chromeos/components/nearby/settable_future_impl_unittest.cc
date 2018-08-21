// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/settable_future_impl.h"

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "chromeos/components/nearby/library/exception.h"
#include "chromeos/components/nearby/library/settable_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace nearby {

class SettableFutureImplTest : public testing::Test {
 protected:
  SettableFutureImplTest() = default;

  void SetUp() override {
    settable_future_ = std::make_unique<SettableFutureImpl<bool>>();
  }

  location::nearby::SettableFuture<bool>* settable_future() {
    return settable_future_.get();
  }

  void VerifyFutureBool(bool value) {
    location::nearby::ExceptionOr<bool> result = settable_future()->get();
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(value, result.result());
  }

  base::UnguessableToken PostGetAsyncResult() {
    base::UnguessableToken id = base::UnguessableToken::Create();
    base::PostTaskWithTraits(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&SettableFutureImplTest::GetAsyncResult,
                       base::Unretained(this), id));
    return id;
  }

  size_t GetMapSize() {
    base::AutoLock al(map_lock_);
    return id_to_async_result_map_.size();
  }

  void VerifyAsyncResultForId(const base::UnguessableToken& id,
                              bool expected_result) {
    base::AutoLock al(map_lock_);
    EXPECT_TRUE(base::ContainsKey(id_to_async_result_map_, id));
    EXPECT_TRUE(id_to_async_result_map_[id]);
    EXPECT_TRUE(id_to_async_result_map_[id]->ok());
    EXPECT_EQ(expected_result, id_to_async_result_map_[id]->result());
  }

  // It's necessary to sleep before checking |id_to_async_result_map_| as we
  // need to give GetTask() enough of a time buffer to either block (if we
  // expect |id_to_async_result_map_| to not contain the latest result) or
  // finish running get() (if we expect |id_to_async_result_map_| to contain the
  // latest result).
  void TinyTimeout() {
    // As of writing, tiny_timeout() is 100ms.
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  void GetAsyncResult(const base::UnguessableToken& id) {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
    location::nearby::ExceptionOr<bool> result = settable_future_->get();
    base::AutoLock al(map_lock_);
    id_to_async_result_map_[id] = result;
  }

  std::unique_ptr<location::nearby::SettableFuture<bool>> settable_future_;
  base::Lock map_lock_;
  base::flat_map<base::UnguessableToken,
                 base::Optional<location::nearby::ExceptionOr<bool>>>
      id_to_async_result_map_;

  DISALLOW_COPY_AND_ASSIGN(SettableFutureImplTest);
};

TEST_F(SettableFutureImplTest, GetValueAfterSetting) {
  EXPECT_TRUE(settable_future()->set(true));
  VerifyFutureBool(true);
}

TEST_F(SettableFutureImplTest, CannotSetAgainIfValueAlreadyBeenSet) {
  EXPECT_TRUE(settable_future()->set(true));
  EXPECT_FALSE(settable_future()->set(true));
}

TEST_F(SettableFutureImplTest, BlocksUntilValueIsSet) {
  base::UnguessableToken id = PostGetAsyncResult();
  TinyTimeout();
  EXPECT_EQ(0u, GetMapSize());

  EXPECT_TRUE(settable_future()->set(true));
  TinyTimeout();
  EXPECT_EQ(1u, GetMapSize());
  VerifyAsyncResultForId(id, true);
  VerifyFutureBool(true);
}

TEST_F(SettableFutureImplTest, BlocksMultipleUntilValueIsSet) {
  base::UnguessableToken id0 = PostGetAsyncResult();
  base::UnguessableToken id1 = PostGetAsyncResult();
  base::UnguessableToken id2 = PostGetAsyncResult();
  TinyTimeout();
  EXPECT_EQ(0u, GetMapSize());

  EXPECT_TRUE(settable_future()->set(true));
  TinyTimeout();
  EXPECT_EQ(3u, GetMapSize());
  VerifyAsyncResultForId(id0, true);
  VerifyAsyncResultForId(id1, true);
  VerifyAsyncResultForId(id2, true);
  VerifyFutureBool(true);
}

}  // namespace nearby

}  // namespace chromeos
