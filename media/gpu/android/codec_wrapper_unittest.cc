// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_wrapper.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/mock_media_codec_bridge.h"
#include "media/base/encryption_scheme.h"
#include "media/base/subsample_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::Return;
using testing::DoAll;
using testing::SetArgPointee;
using testing::NiceMock;
using testing::_;

namespace media {

class CodecWrapperTest : public testing::Test {
 public:
  CodecWrapperTest() {
    auto codec = base::MakeUnique<NiceMock<MockMediaCodecBridge>>();
    codec_ = codec.get();
    wrapper_ = base::MakeUnique<CodecWrapper>(std::move(codec));
    ON_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
        .WillByDefault(Return(MEDIA_CODEC_OK));
  }

  ~CodecWrapperTest() override {
    // ~CodecWrapper asserts that the codec was taken.
    wrapper_->TakeCodec();
  }

  std::unique_ptr<CodecOutputBuffer> DequeueCodecOutputBuffer() {
    std::unique_ptr<CodecOutputBuffer> codec_buffer;
    wrapper_->DequeueOutputBuffer(base::TimeDelta(), nullptr, nullptr,
                                  &codec_buffer);
    return codec_buffer;
  }

  NiceMock<MockMediaCodecBridge>* codec_;
  std::unique_ptr<CodecWrapper> wrapper_;
};

TEST_F(CodecWrapperTest, TakeCodecReturnsTheCodecFirstAndNullLater) {
  ASSERT_EQ(wrapper_->TakeCodec().get(), codec_);
  ASSERT_EQ(wrapper_->TakeCodec(), nullptr);
}

TEST_F(CodecWrapperTest, NoCodecOutputBufferReturnedIfDequeueFails) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MEDIA_CODEC_ERROR));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer, nullptr);
}

TEST_F(CodecWrapperTest, InitiallyThereAreNoValidCodecOutputBuffers) {
  ASSERT_FALSE(wrapper_->HasValidCodecOutputBuffers());
}

TEST_F(CodecWrapperTest, FlushInvalidatesCodecOutputBuffers) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  wrapper_->Flush();
  ASSERT_FALSE(codec_buffer->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, TakingTheCodecInvalidatesCodecOutputBuffers) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  wrapper_->TakeCodec();
  ASSERT_FALSE(codec_buffer->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, SetSurfaceInvalidatesCodecOutputBuffers) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  wrapper_->SetSurface(0);
  ASSERT_FALSE(codec_buffer->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, CodecOutputBuffersAreAllInvalidatedTogether) {
  auto codec_buffer1 = DequeueCodecOutputBuffer();
  auto codec_buffer2 = DequeueCodecOutputBuffer();
  wrapper_->Flush();
  ASSERT_FALSE(codec_buffer1->ReleaseToSurface());
  ASSERT_FALSE(codec_buffer2->ReleaseToSurface());
  ASSERT_FALSE(wrapper_->HasValidCodecOutputBuffers());
}

TEST_F(CodecWrapperTest, CodecOutputBuffersAfterFlushAreValid) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  wrapper_->Flush();
  codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_TRUE(codec_buffer->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, CodecOutputBufferReleaseUsesCorrectIndex) {
  // The second arg is the buffer index pointer.
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(42), Return(MEDIA_CODEC_OK)));
  auto codec_buffer = DequeueCodecOutputBuffer();
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(42, true));
  codec_buffer->ReleaseToSurface();
}

TEST_F(CodecWrapperTest, CodecOutputBuffersAreInvalidatedByRelease) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  codec_buffer->ReleaseToSurface();
  ASSERT_FALSE(codec_buffer->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, CodecOutputBuffersReleaseOnDestruction) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(_, false));
  codec_buffer = nullptr;
}

TEST_F(CodecWrapperTest, CodecOutputBuffersDoNotReleaseIfAlreadyReleased) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  codec_buffer->ReleaseToSurface();
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(_, _)).Times(0);
  codec_buffer = nullptr;
}

TEST_F(CodecWrapperTest, ReleasingCodecOutputBuffersAfterTheCodecIsSafe) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  wrapper_->TakeCodec();
  codec_buffer->ReleaseToSurface();
}

TEST_F(CodecWrapperTest, DeletingCodecOutputBuffersAfterTheCodecIsSafe) {
  auto codec_buffer = DequeueCodecOutputBuffer();
  wrapper_->TakeCodec();
  // This test ensures the destructor doesn't crash.
  codec_buffer = nullptr;
}

TEST_F(CodecWrapperTest, CodecOutputBufferReleaseInvalidatesEarlierOnes) {
  auto codec_buffer1 = DequeueCodecOutputBuffer();
  auto codec_buffer2 = DequeueCodecOutputBuffer();
  codec_buffer2->ReleaseToSurface();
  ASSERT_FALSE(codec_buffer1->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, CodecOutputBufferReleaseDoesNotInvalidateLaterOnes) {
  auto codec_buffer1 = DequeueCodecOutputBuffer();
  auto codec_buffer2 = DequeueCodecOutputBuffer();
  codec_buffer1->ReleaseToSurface();
  ASSERT_TRUE(codec_buffer2->ReleaseToSurface());
}

TEST_F(CodecWrapperTest, FormatChangedStatusIsSwallowed) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MEDIA_CODEC_OUTPUT_FORMAT_CHANGED))
      .WillOnce(Return(MEDIA_CODEC_TRY_AGAIN_LATER));
  std::unique_ptr<CodecOutputBuffer> codec_buffer;
  auto status = wrapper_->DequeueOutputBuffer(base::TimeDelta(), nullptr,
                                              nullptr, &codec_buffer);
  ASSERT_EQ(status, MEDIA_CODEC_TRY_AGAIN_LATER);
}

TEST_F(CodecWrapperTest, BuffersChangedStatusIsSwallowed) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED))
      .WillOnce(Return(MEDIA_CODEC_TRY_AGAIN_LATER));
  std::unique_ptr<CodecOutputBuffer> codec_buffer;
  auto status = wrapper_->DequeueOutputBuffer(base::TimeDelta(), nullptr,
                                              nullptr, &codec_buffer);
  ASSERT_EQ(status, MEDIA_CODEC_TRY_AGAIN_LATER);
}

TEST_F(CodecWrapperTest, MultipleFormatChangedStatusesIsAnError) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillRepeatedly(Return(MEDIA_CODEC_OUTPUT_FORMAT_CHANGED));
  std::unique_ptr<CodecOutputBuffer> codec_buffer;
  auto status = wrapper_->DequeueOutputBuffer(base::TimeDelta(), nullptr,
                                              nullptr, &codec_buffer);
  ASSERT_EQ(status, MEDIA_CODEC_ERROR);
}

TEST_F(CodecWrapperTest, CodecOutputBuffersHaveTheCorrectSize) {
  EXPECT_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillOnce(Return(MEDIA_CODEC_OUTPUT_FORMAT_CHANGED))
      .WillOnce(Return(MEDIA_CODEC_OK));
  EXPECT_CALL(*codec_, GetOutputSize(_))
      .WillOnce(
          DoAll(SetArgPointee<0>(gfx::Size(42, 42)), Return(MEDIA_CODEC_OK)));
  auto codec_buffer = DequeueCodecOutputBuffer();
  ASSERT_EQ(codec_buffer->size(), gfx::Size(42, 42));
}

#if DCHECK_IS_ON()
TEST_F(CodecWrapperTest, CallsDcheckAfterTakingTheCodec) {
  wrapper_->TakeCodec();
  ASSERT_DEATH(wrapper_->Flush(), "");
}

TEST_F(CodecWrapperTest, CallsDcheckAfterAnError) {
  EXPECT_CALL(*codec_, Flush()).WillOnce(Return(MEDIA_CODEC_ERROR));
  wrapper_->Flush();
  ASSERT_DEATH(wrapper_->SetSurface(0), "");
}
#endif

}  // namespace media
