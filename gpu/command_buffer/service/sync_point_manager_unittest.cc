// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <queue>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class SyncPointManagerTest : public testing::Test {
 public:
  SyncPointManagerTest() : sync_point_manager_(new SyncPointManager) {}
  ~SyncPointManagerTest() override {}

 protected:
  // Simple static function which can be used to test callbacks.
  static void SetIntegerFunction(int* test, int value) { *test = value; }

  std::unique_ptr<SyncPointManager> sync_point_manager_;
};

struct SyncPointStream {
  scoped_refptr<SyncPointOrderData> order_data;
  std::unique_ptr<SyncPointClient> client;
  std::queue<uint32_t> order_numbers;

  SyncPointStream(SyncPointManager* sync_point_manager,
                  CommandBufferNamespace namespace_id,
                  CommandBufferId command_buffer_id)
      : order_data(SyncPointOrderData::Create()),
        client(base::MakeUnique<SyncPointClient>(sync_point_manager,
                                                 order_data,
                                                 namespace_id,
                                                 command_buffer_id)) {}

  ~SyncPointStream() {
    order_data->Destroy();
    order_data = nullptr;
  }

  void AllocateOrderNum(SyncPointManager* sync_point_manager) {
    order_numbers.push(
        order_data->GenerateUnprocessedOrderNumber(sync_point_manager));
  }

  void BeginProcessing() {
    ASSERT_FALSE(order_numbers.empty());
    order_data->BeginProcessingOrderNumber(order_numbers.front());
  }

  void EndProcessing() {
    ASSERT_FALSE(order_numbers.empty());
    order_data->FinishProcessingOrderNumber(order_numbers.front());
    order_numbers.pop();
  }
};

TEST_F(SyncPointManagerTest, BasicSyncPointOrderDataTest) {
  scoped_refptr<SyncPointOrderData> order_data = SyncPointOrderData::Create();

  EXPECT_EQ(0u, order_data->current_order_num());
  EXPECT_EQ(0u, order_data->processed_order_num());
  EXPECT_EQ(0u, order_data->unprocessed_order_num());

  uint32_t order_num =
      order_data->GenerateUnprocessedOrderNumber(sync_point_manager_.get());
  EXPECT_EQ(1u, order_num);

  EXPECT_EQ(0u, order_data->current_order_num());
  EXPECT_EQ(0u, order_data->processed_order_num());
  EXPECT_EQ(order_num, order_data->unprocessed_order_num());

  order_data->BeginProcessingOrderNumber(order_num);
  EXPECT_EQ(order_num, order_data->current_order_num());
  EXPECT_EQ(0u, order_data->processed_order_num());
  EXPECT_EQ(order_num, order_data->unprocessed_order_num());
  EXPECT_TRUE(order_data->IsProcessingOrderNumber());

  order_data->PauseProcessingOrderNumber(order_num);
  EXPECT_FALSE(order_data->IsProcessingOrderNumber());

  order_data->BeginProcessingOrderNumber(order_num);
  EXPECT_TRUE(order_data->IsProcessingOrderNumber());

  order_data->FinishProcessingOrderNumber(order_num);
  EXPECT_EQ(order_num, order_data->current_order_num());
  EXPECT_EQ(order_num, order_data->processed_order_num());
  EXPECT_EQ(order_num, order_data->unprocessed_order_num());
  EXPECT_FALSE(order_data->IsProcessingOrderNumber());
}

TEST_F(SyncPointManagerTest, BasicFenceSyncRelease) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kBufferId = CommandBufferId::FromUnsafeValue(0x123);

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, 0, kBufferId, release_count);

  // Can't wait for sync token before client is registered.
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));

  SyncPointStream stream(sync_point_manager_.get(), kNamespaceId, kBufferId);

  stream.AllocateOrderNum(sync_point_manager_.get());

  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));

  stream.order_data->BeginProcessingOrderNumber(1);
  stream.client->ReleaseFenceSync(release_count);
  stream.order_data->FinishProcessingOrderNumber(1);

  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_F(SyncPointManagerTest, MultipleClientsPerOrderData) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kCmdBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kCmdBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream stream1(sync_point_manager_.get(), kNamespaceId,
                          kCmdBufferId1);
  SyncPointStream stream2(sync_point_manager_.get(), kNamespaceId,
                          kCmdBufferId2);

  uint64_t release_count = 1;
  SyncToken sync_token1(kNamespaceId, 0, kCmdBufferId1, release_count);
  stream1.AllocateOrderNum(sync_point_manager_.get());

  SyncToken sync_token2(kNamespaceId, 0, kCmdBufferId2, release_count);
  stream2.AllocateOrderNum(sync_point_manager_.get());

  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token1));
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token2));

  stream1.order_data->BeginProcessingOrderNumber(1);
  stream1.client->ReleaseFenceSync(release_count);
  stream1.order_data->FinishProcessingOrderNumber(1);

  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token1));
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token2));
}

TEST_F(SyncPointManagerTest, BasicFenceSyncWaitRelease) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  release_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, 0, kReleaseCmdBufferId, release_count);

  wait_stream.BeginProcessing();
  int test_num = 10;
  bool valid_wait = wait_stream.client->Wait(
      sync_token,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));

  release_stream.BeginProcessing();
  release_stream.client->ReleaseFenceSync(release_count);
  EXPECT_EQ(123, test_num);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_F(SyncPointManagerTest, WaitOnSelfFails) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  release_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, 0, kWaitCmdBufferId, release_count);

  wait_stream.BeginProcessing();
  int test_num = 10;
  bool valid_wait = wait_stream.client->Wait(
      sync_token,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_FALSE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_F(SyncPointManagerTest, OutOfOrderRelease) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  // Generate wait order number first.
  wait_stream.AllocateOrderNum(sync_point_manager_.get());
  release_stream.AllocateOrderNum(sync_point_manager_.get());

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, 0, kReleaseCmdBufferId, release_count);

  wait_stream.BeginProcessing();
  int test_num = 10;
  bool valid_wait = wait_stream.client->Wait(
      sync_token,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_FALSE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_F(SyncPointManagerTest, HigherOrderNumberRelease) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  // Generate wait order number first.
  wait_stream.AllocateOrderNum(sync_point_manager_.get());
  release_stream.AllocateOrderNum(sync_point_manager_.get());

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, 0, kReleaseCmdBufferId, release_count);

  // Order number was higher but it was actually released.
  release_stream.BeginProcessing();
  release_stream.client->ReleaseFenceSync(release_count);
  release_stream.EndProcessing();

  // Release stream has already released so there's no need to wait.
  wait_stream.BeginProcessing();
  int test_num = 10;
  bool valid_wait = wait_stream.client->Wait(
      sync_token,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_FALSE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_F(SyncPointManagerTest, DestroyedClientRelease) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  release_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, 0, kReleaseCmdBufferId, release_count);

  wait_stream.BeginProcessing();

  int test_num = 10;
  bool valid_wait = wait_stream.client->Wait(
      sync_token,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);

  // Destroying the client should release the wait.
  release_stream.client.reset();
  EXPECT_EQ(123, test_num);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_F(SyncPointManagerTest, NonExistentRelease) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  // Assign release stream order [1] and wait stream order [2].
  // This test simply tests that a wait stream of order [2] waiting on
  // release stream of order [1] will still release the fence sync even
  // though nothing was released.
  release_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, 0, kReleaseCmdBufferId, release_count);

  wait_stream.BeginProcessing();
  int test_num = 10;
  bool valid_wait = wait_stream.client->Wait(
      sync_token,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));

  // No release but finishing the order number should automatically release.
  release_stream.BeginProcessing();
  EXPECT_EQ(10, test_num);
  release_stream.EndProcessing();
  EXPECT_EQ(123, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_F(SyncPointManagerTest, NonExistentRelease2) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  // Assign Release stream order [1] and assign Wait stream orders [2, 3].
  // This test is similar to the NonExistentRelease case except
  // we place an extra order number in between the release and wait.
  // The wait stream [3] is waiting on release stream [1] even though
  // order [2] was also generated. Although order [2] only exists on the
  // wait stream so the release stream should only know about order [1].
  release_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, 0, kReleaseCmdBufferId, release_count);

  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));
  // Have wait with order [3] to wait on release.
  wait_stream.BeginProcessing();
  EXPECT_EQ(2u, wait_stream.order_data->current_order_num());
  wait_stream.EndProcessing();
  wait_stream.BeginProcessing();
  EXPECT_EQ(3u, wait_stream.order_data->current_order_num());
  int test_num = 10;
  bool valid_wait = wait_stream.client->Wait(
      sync_token,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));

  // Even though release stream order [1] did not have a release, it
  // should have changed test_num although the fence sync is still not released.
  release_stream.BeginProcessing();
  EXPECT_EQ(1u, release_stream.order_data->current_order_num());
  release_stream.EndProcessing();
  EXPECT_EQ(123, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));

  // Ensure that the wait callback does not get triggered again when it is
  // actually released.
  test_num = 1;
  release_stream.AllocateOrderNum(sync_point_manager_.get());
  release_stream.BeginProcessing();
  release_stream.client->ReleaseFenceSync(release_count);
  release_stream.EndProcessing();
  EXPECT_EQ(1, test_num);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_F(SyncPointManagerTest, NonExistentOrderNumRelease) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  // Assign Release stream orders [1, 4] and assign Wait stream orders [2, 3].
  // Here we are testing that wait order [3] will wait on a fence sync
  // in either order [1] or order [2]. Order [2] was not actually assigned
  // to the release stream so it is essentially non-existent to the release
  // stream's point of view. Once the release stream begins processing the next
  // order [3], it should realize order [2] didn't exist and release the fence.
  release_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());
  wait_stream.AllocateOrderNum(sync_point_manager_.get());
  release_stream.AllocateOrderNum(sync_point_manager_.get());

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, 0, kReleaseCmdBufferId, release_count);

  // Have wait with order [3] to wait on release order [1] or [2].
  wait_stream.BeginProcessing();
  EXPECT_EQ(2u, wait_stream.order_data->current_order_num());
  wait_stream.EndProcessing();
  wait_stream.BeginProcessing();
  EXPECT_EQ(3u, wait_stream.order_data->current_order_num());
  int test_num = 10;
  bool valid_wait = wait_stream.client->Wait(
      sync_token,
      base::Bind(&SyncPointManagerTest::SetIntegerFunction, &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);

  // Release stream should know it should release fence sync by order [3],
  // so going through order [1] should not release it yet.
  release_stream.BeginProcessing();
  EXPECT_EQ(1u, release_stream.order_data->current_order_num());
  release_stream.EndProcessing();
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));
  EXPECT_EQ(10, test_num);

  // Beginning order [4] should immediately trigger the wait although the fence
  // sync is still not released yet.
  release_stream.BeginProcessing();
  EXPECT_EQ(4u, release_stream.order_data->current_order_num());
  EXPECT_EQ(123, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));

  // Ensure that the wait callback does not get triggered again when it is
  // actually released.
  test_num = 1;
  release_stream.client->ReleaseFenceSync(1);
  EXPECT_EQ(1, test_num);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

}  // namespace gpu
