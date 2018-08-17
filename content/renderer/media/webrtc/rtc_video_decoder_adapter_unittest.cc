// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "content/renderer/media/webrtc/rtc_video_decoder_adapter.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/decode_status.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace content {

namespace {

class MockVideoDecoder : public media::VideoDecoder {
 public:
  std::string GetDisplayName() const override { return "MockVideoDecoder"; }
  MOCK_METHOD6(
      Initialize,
      void(const media::VideoDecoderConfig& config,
           bool low_delay,
           media::CdmContext* cdm_context,
           const InitCB& init_cb,
           const OutputCB& output_cb,
           const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb));
  MOCK_METHOD2(Decode,
               void(scoped_refptr<media::DecoderBuffer> buffer,
                    const DecodeCB&));
  MOCK_METHOD1(Reset, void(const base::RepeatingClosure&));
  bool NeedsBitstreamConversion() const override { return false; }
  bool CanReadWithoutStalling() const override { return true; }
  int GetMaxDecodeRequests() const override { return 1; }
};

// Wraps a callback as a webrtc::DecodedImageCallback.
class DecodedImageCallback : public webrtc::DecodedImageCallback {
 public:
  DecodedImageCallback(
      base::RepeatingCallback<void(const webrtc::VideoFrame&)> callback)
      : callback_(callback) {}

  int32_t Decoded(webrtc::VideoFrame& decodedImage) override {
    callback_.Run(decodedImage);
    // TODO(sandersd): Does the return value matter? RTCVideoDecoder
    // ignores it.
    return 0;
  }

 private:
  base::RepeatingCallback<void(const webrtc::VideoFrame&)> callback_;

  DISALLOW_COPY_AND_ASSIGN(DecodedImageCallback);
};

}  // namespace

class RTCVideoDecoderAdapterTest : public ::testing::Test {
 public:
  RTCVideoDecoderAdapterTest()
      : media_thread_("Media Thread"),
        gpu_factories_(nullptr),
        decoded_image_callback_(decoded_cb_.Get()) {
    media_thread_.Start();
    ON_CALL(gpu_factories_, GetTaskRunner())
        .WillByDefault(Return(media_thread_.task_runner()));
    EXPECT_CALL(gpu_factories_, GetTaskRunner()).Times(AtLeast(0));
    owned_video_decoder_ = std::make_unique<StrictMock<MockVideoDecoder>>();
    video_decoder_ = owned_video_decoder_.get();
  }

  ~RTCVideoDecoderAdapterTest() {
    if (!rtc_video_decoder_adapter_)
      return;

    RTCVideoDecoderAdapter::DeleteSoonOnMediaThread(
        std::move(rtc_video_decoder_adapter_), &gpu_factories_);
    media_thread_.FlushForTesting();
  }

 protected:
  std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
      media::MediaLog* media_log) {
    DCHECK(owned_video_decoder_);
    return std::move(owned_video_decoder_);
  }

  bool BasicSetup() {
    if (!CreateAndInitialize())
      return false;
    if (InitDecode() != WEBRTC_VIDEO_CODEC_OK)
      return false;
    if (RegisterDecodeCompleteCallback() != WEBRTC_VIDEO_CODEC_OK)
      return false;
    return true;
  }

  bool BasicTeardown() {
    if (Release() != WEBRTC_VIDEO_CODEC_OK)
      return false;
    return true;
  }

  bool CreateAndInitialize(bool init_cb_result = true) {
    EXPECT_CALL(*video_decoder_, Initialize(_, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<4>(&output_cb_),
                        media::RunCallback<3>(init_cb_result)));
    rtc_video_decoder_adapter_ = RTCVideoDecoderAdapter::Create(
        webrtc::kVideoCodecVP9, &gpu_factories_,
        base::BindRepeating(&RTCVideoDecoderAdapterTest::CreateVideoDecoder,
                            base::Unretained(this)));
    return !!rtc_video_decoder_adapter_;
  }

  int32_t InitDecode() {
    webrtc::VideoCodec codec_settings;
    codec_settings.codecType = webrtc::kVideoCodecVP9;
    return rtc_video_decoder_adapter_->InitDecode(&codec_settings, 1);
  }

  int32_t RegisterDecodeCompleteCallback() {
    return rtc_video_decoder_adapter_->RegisterDecodeCompleteCallback(
        &decoded_image_callback_);
  }

  int32_t Decode(uint32_t timestamp) {
    uint8_t buf[] = {0};
    webrtc::EncodedImage input_image(&buf[0], 1, 1);
    input_image._completeFrame = true;
    input_image._timeStamp = timestamp;
    return rtc_video_decoder_adapter_->Decode(input_image, false, nullptr, 0);
  }

  void FinishDecode(uint32_t timestamp) {
    media_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RTCVideoDecoderAdapterTest::FinishDecodeOnMediaThread,
                       base::Unretained(this), timestamp));
  }

  void FinishDecodeOnMediaThread(uint32_t timestamp) {
    DCHECK(media_thread_.task_runner()->BelongsToCurrentThread());
    gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes];
    mailbox_holders[0].mailbox = gpu::Mailbox::Generate();
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::WrapNativeTextures(
            media::PIXEL_FORMAT_ARGB, mailbox_holders,
            media::VideoFrame::ReleaseMailboxCB(), gfx::Size(640, 360),
            gfx::Rect(640, 360), gfx::Size(640, 360),
            base::TimeDelta::FromMicroseconds(timestamp));
    output_cb_.Run(std::move(frame));
  }

  int32_t Release() { return rtc_video_decoder_adapter_->Release(); }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::Thread media_thread_;

  // Owned by |rtc_video_decoder_adapter_|.
  StrictMock<MockVideoDecoder>* video_decoder_ = nullptr;

  StrictMock<base::MockCallback<
      base::RepeatingCallback<void(const webrtc::VideoFrame&)>>>
      decoded_cb_;

 private:
  StrictMock<media::MockGpuVideoAcceleratorFactories> gpu_factories_;
  std::unique_ptr<RTCVideoDecoderAdapter> rtc_video_decoder_adapter_;
  std::unique_ptr<StrictMock<MockVideoDecoder>> owned_video_decoder_;
  DecodedImageCallback decoded_image_callback_;
  media::VideoDecoder::OutputCB output_cb_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoderAdapterTest);
};

// Crashing on various platforms. See https://crbug.com/875278.
TEST_F(RTCVideoDecoderAdapterTest, DISABLED_Lifecycle) {
  ASSERT_TRUE(BasicSetup());
  ASSERT_TRUE(BasicTeardown());
}

TEST_F(RTCVideoDecoderAdapterTest, InitializationFailure) {
  ASSERT_FALSE(CreateAndInitialize(false));
}

TEST_F(RTCVideoDecoderAdapterTest, Decode) {
  ASSERT_TRUE(BasicSetup());

  EXPECT_CALL(*video_decoder_, Decode(_, _))
      .WillOnce(media::RunCallback<1>(media::DecodeStatus::OK));

  ASSERT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);

  EXPECT_CALL(decoded_cb_, Run(_));
  FinishDecode(0);
  media_thread_.FlushForTesting();
}

TEST_F(RTCVideoDecoderAdapterTest, Decode_Error) {
  ASSERT_TRUE(BasicSetup());

  EXPECT_CALL(*video_decoder_, Decode(_, _))
      .WillOnce(media::RunCallback<1>(media::DecodeStatus::DECODE_ERROR));

  ASSERT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);
  media_thread_.FlushForTesting();

  ASSERT_EQ(Decode(1), WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
}

TEST_F(RTCVideoDecoderAdapterTest, Decode_Hang_Short) {
  ASSERT_TRUE(BasicSetup());

  // Ignore Decode() calls.
  EXPECT_CALL(*video_decoder_, Decode(_, _)).Times(AtLeast(1));

  for (int counter = 0; counter < 10; counter++) {
    int32_t result = Decode(counter);
    if (result == WEBRTC_VIDEO_CODEC_ERROR) {
      ASSERT_GT(counter, 2);
      return;
    }
    media_thread_.FlushForTesting();
  }

  FAIL();
}

TEST_F(RTCVideoDecoderAdapterTest, Decode_Hang_Long) {
  ASSERT_TRUE(BasicSetup());

  // Ignore Decode() calls.
  EXPECT_CALL(*video_decoder_, Decode(_, _)).Times(AtLeast(1));

  for (int counter = 0; counter < 100; counter++) {
    int32_t result = Decode(counter);
    if (result == WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE) {
      ASSERT_GT(counter, 10);
      return;
    }
    media_thread_.FlushForTesting();
  }

  FAIL();
}

}  // namespace content
