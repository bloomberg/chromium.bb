// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/numerics/checked_math.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

using testing::InSequence;
using testing::StrictMock;

namespace gpu {
class GpuChannel;

// This mock allows individual tests to decide asynchronously when to finish a
// decode by using the FinishOneDecode() method.
class MockImageDecodeAcceleratorWorker : public ImageDecodeAcceleratorWorker {
 public:
  MockImageDecodeAcceleratorWorker() {}

  void Decode(std::vector<uint8_t> encoded_data,
              const gfx::Size& output_size,
              base::OnceCallback<void(std::vector<uint8_t>)> decode_cb) {
    pending_decodes_.push(PendingDecode{output_size, std::move(decode_cb)});
    DoDecode(output_size);
  }

  void FinishOneDecode(bool success) {
    if (pending_decodes_.empty())
      return;
    PendingDecode next_decode = std::move(pending_decodes_.front());
    pending_decodes_.pop();
    if (success) {
      base::CheckedNumeric<size_t> rgba_bytes = 4u;
      rgba_bytes *= next_decode.output_size.width();
      rgba_bytes *= next_decode.output_size.height();
      std::vector<uint8_t> rgba_output(rgba_bytes.ValueOrDie(), 0u);
      std::move(next_decode.decode_cb).Run(std::move(rgba_output));
    } else {
      std::move(next_decode.decode_cb).Run(std::vector<uint8_t>());
    }
  }

  MOCK_METHOD1(DoDecode, void(const gfx::Size&));

 private:
  struct PendingDecode {
    gfx::Size output_size;
    base::OnceCallback<void(std::vector<uint8_t>)> decode_cb;
  };

  base::queue<PendingDecode> pending_decodes_;

  DISALLOW_COPY_AND_ASSIGN(MockImageDecodeAcceleratorWorker);
};

const int kChannelId = 1;

// Test fixture: the general strategy for testing is to have a GPU channel test
// infrastructure (provided by GpuChannelTestCommon), ask the channel to handle
// decode requests, and expect sync token releases and invokations to the
// ImageDecodeAcceleratorWorker functionality.
class ImageDecodeAcceleratorStubTest : public GpuChannelTestCommon {
 public:
  ImageDecodeAcceleratorStubTest() : GpuChannelTestCommon() {}
  ~ImageDecodeAcceleratorStubTest() override = default;

  SyncPointManager* sync_point_manager() const {
    return channel_manager()->sync_point_manager();
  }

  void SetUp() override {
    GpuChannelTestCommon::SetUp();
    // TODO(andrescj): get rid of the |feature_list_| when the feature is
    // enabled by default.
    feature_list_.InitAndEnableFeature(
        features::kVaapiJpegImageDecodeAcceleration);
    channel_manager()->SetImageDecodeAcceleratorWorkerForTesting(
        &image_decode_accelerator_worker_);
    ASSERT_TRUE(CreateChannel(kChannelId, false /* is_gpu_host */));
  }

  void TearDown() override {
    // Make sure the channel is destroyed before the
    // |image_decode_accelerator_worker_| is destroyed.
    channel_manager()->DestroyAllChannels();
  }

  SyncToken SendDecodeRequest(const gfx::Size& output_size,
                              uint64_t release_count) {
    GpuChannel* channel = channel_manager()->LookupChannel(kChannelId);
    if (!channel) {
      // It's possible that the channel was destroyed as part of an earlier
      // SendDecodeRequest() call. This would happen if
      // ImageDecodeAcceleratorStub::OnScheduleImageDecode decides to destroy
      // the channel.
      return SyncToken();
    }

    SyncToken decode_sync_token(
        CommandBufferNamespace::GPU_IO,
        CommandBufferIdFromChannelAndRoute(
            kChannelId, static_cast<int32_t>(
                            GpuChannelReservedRoutes::kImageDecodeAccelerator)),
        release_count);
    GpuChannelMsg_ScheduleImageDecode_Params decode_params;
    decode_params.encoded_data = std::vector<uint8_t>();
    decode_params.output_size = output_size;
    decode_params.raster_decoder_route_id = 1;
    decode_params.transfer_cache_entry_id = 1u;
    decode_params.discardable_handle_shm_id = 0;
    decode_params.discardable_handle_shm_offset = 0u;
    decode_params.target_color_space = gfx::ColorSpace();
    decode_params.needs_mips = false;

    HandleMessage(
        channel,
        new GpuChannelMsg_ScheduleImageDecode(
            static_cast<int32_t>(
                GpuChannelReservedRoutes::kImageDecodeAccelerator),
            std::move(decode_params), decode_sync_token.release_count()));
    return decode_sync_token;
  }

  void RunTasksUntilIdle() {
    while (task_runner()->HasPendingTask() ||
           io_task_runner()->HasPendingTask()) {
      task_runner()->RunUntilIdle();
      io_task_runner()->RunUntilIdle();
    }
  }

 protected:
  StrictMock<MockImageDecodeAcceleratorWorker> image_decode_accelerator_worker_;

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeAcceleratorStubTest);
};

// Tests the following flow: two decode requests are sent. One of the decodes is
// completed. This should cause one sync token to be released and the scheduler
// sequence to be disabled. Then, the second decode is completed. This should
// cause the other sync token to be released.
TEST_F(ImageDecodeAcceleratorStubTest,
       MultipleDecodesCompletedAfterSequenceIsDisabled) {
  {
    InSequence call_sequence;
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(200, 200)))
        .Times(1);
  }
  const SyncToken decode1_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 1u /* release_count */);
  const SyncToken decode2_sync_token = SendDecodeRequest(
      gfx::Size(200, 200) /* output_size */, 2u /* release_count */);

  // A decode sync token should not be released before a decode is finished.
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));

  // Only the first decode sync token should be released after the first decode
  // is finished.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));

  // The second decode sync token should be released after the second decode is
  // finished.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));

  // The channel should still exist at the end.
  EXPECT_TRUE(channel_manager()->LookupChannel(kChannelId));
}

// Tests the following flow: three decode requests are sent. The first decode
// completes which should cause the scheduler sequence to be enabled. Right
// after that (while the sequence is still enabled), the other two decodes
// complete. At the end, all the sync tokens should be released.
TEST_F(ImageDecodeAcceleratorStubTest,
       MultipleDecodesCompletedWhileSequenceIsEnabled) {
  {
    InSequence call_sequence;
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(200, 200)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(300, 300)))
        .Times(1);
  }
  const SyncToken decode1_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 1u /* release_count */);
  const SyncToken decode2_sync_token = SendDecodeRequest(
      gfx::Size(200, 200) /* output_size */, 2u /* release_count */);
  const SyncToken decode3_sync_token = SendDecodeRequest(
      gfx::Size(300, 300) /* output_size */, 3u /* release_count */);

  // A decode sync token should not be released before a decode is finished.
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode3_sync_token));

  // All decode sync tokens should be released after completing all the decodes.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  image_decode_accelerator_worker_.FinishOneDecode(true);
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode3_sync_token));

  // The channel should still exist at the end.
  EXPECT_TRUE(channel_manager()->LookupChannel(kChannelId));
}

// Tests the following flow: three decode requests are sent. The first decode
// fails which should trigger the destruction of the channel. The second
// succeeds and the third one fails. Regardless, the channel should still be
// destroyed and all sync tokens should be released.
TEST_F(ImageDecodeAcceleratorStubTest, FailedDecodes) {
  {
    InSequence call_sequence;
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(200, 200)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(300, 300)))
        .Times(1);
  }
  const SyncToken decode1_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 1u /* release_count */);
  const SyncToken decode2_sync_token = SendDecodeRequest(
      gfx::Size(200, 200) /* output_size */, 2u /* release_count */);
  const SyncToken decode3_sync_token = SendDecodeRequest(
      gfx::Size(300, 300) /* output_size */, 3u /* release_count */);

  // A decode sync token should not be released before a decode is finished.
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode3_sync_token));
  image_decode_accelerator_worker_.FinishOneDecode(false);
  image_decode_accelerator_worker_.FinishOneDecode(true);
  image_decode_accelerator_worker_.FinishOneDecode(false);

  // We expect the destruction of the ImageDecodeAcceleratorStub, which also
  // implies that all decode sync tokens should be released.
  RunTasksUntilIdle();
  EXPECT_FALSE(channel_manager()->LookupChannel(kChannelId));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode3_sync_token));
}

TEST_F(ImageDecodeAcceleratorStubTest, OutOfOrderSyncTokens) {
  EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
      .Times(1);
  const SyncToken decode1_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 2u /* release_count */);
  const SyncToken decode2_sync_token = SendDecodeRequest(
      gfx::Size(200, 200) /* output_size */, 1u /* release_count */);

  // We expect the destruction of the ImageDecodeAcceleratorStub, which also
  // implies that all decode sync tokens should be released.
  RunTasksUntilIdle();
  EXPECT_FALSE(channel_manager()->LookupChannel(kChannelId));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
}

TEST_F(ImageDecodeAcceleratorStubTest, ZeroReleaseCountSyncToken) {
  const SyncToken decode_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 0u /* release_count */);

  // We expect the destruction of the ImageDecodeAcceleratorStub, which also
  // implies that all decode sync tokens should be released.
  RunTasksUntilIdle();
  EXPECT_FALSE(channel_manager()->LookupChannel(kChannelId));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));
}

TEST_F(ImageDecodeAcceleratorStubTest, ZeroWidthOutputSize) {
  const SyncToken decode_sync_token = SendDecodeRequest(
      gfx::Size(0, 100) /* output_size */, 1u /* release_count */);

  // We expect the destruction of the ImageDecodeAcceleratorStub, which also
  // implies that all decode sync tokens should be released.
  RunTasksUntilIdle();
  EXPECT_FALSE(channel_manager()->LookupChannel(kChannelId));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));
}

TEST_F(ImageDecodeAcceleratorStubTest, ZeroHeightOutputSize) {
  const SyncToken decode_sync_token = SendDecodeRequest(
      gfx::Size(100, 0) /* output_size */, 1u /* release_count */);

  // We expect the destruction of the ImageDecodeAcceleratorStub, which also
  // implies that all decode sync tokens should be released.
  RunTasksUntilIdle();
  EXPECT_FALSE(channel_manager()->LookupChannel(kChannelId));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));
}

}  // namespace gpu
