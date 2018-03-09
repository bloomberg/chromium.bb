// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/offloading_video_decoder.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "media/base/decoder_buffer.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::DoAll;
using testing::SaveArg;

namespace media {

ACTION_P(VerifyOn, task_runner) {
  ASSERT_TRUE(task_runner->RunsTasksInCurrentSequence());
}

ACTION_P(VerifyNotOn, task_runner) {
  ASSERT_FALSE(task_runner->RunsTasksInCurrentSequence());
}

class MockOffloadableVideoDecoder : public OffloadableVideoDecoder {
 public:
  // OffloadableVideoDecoder implementation.
  std::string GetDisplayName() const override {
    return "MockOffloadableVideoDecoder";
  }
  MOCK_METHOD6(
      Initialize,
      void(const VideoDecoderConfig& config,
           bool low_delay,
           CdmContext* cdm_context,
           const InitCB& init_cb,
           const OutputCB& output_cb,
           const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb));
  MOCK_METHOD2(Decode,
               void(const scoped_refptr<DecoderBuffer>& buffer,
                    const DecodeCB&));
  MOCK_METHOD1(Reset, void(const base::Closure&));
  MOCK_METHOD0(Detach, void(void));
};

class OffloadingVideoDecoderTest : public testing::Test {
 public:
  OffloadingVideoDecoderTest()
      : task_env_(base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT,
                  base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED) {}

  void CreateWrapper(int offload_width, VideoCodec codec) {
    decoder_ = new testing::StrictMock<MockOffloadableVideoDecoder>();
    offloading_decoder_ = std::make_unique<OffloadingVideoDecoder>(
        offload_width, std::vector<VideoCodec>(1, codec),
        std::unique_ptr<OffloadableVideoDecoder>(decoder_));
  }

  VideoDecoder::InitCB ExpectInitCB(bool success) {
    EXPECT_CALL(*this, InitDone(success))
        .WillOnce(VerifyOn(task_env_.GetMainThreadTaskRunner()));
    return base::Bind(&OffloadingVideoDecoderTest::InitDone,
                      base::Unretained(this));
  }

  VideoDecoder::OutputCB ExpectOutputCB() {
    EXPECT_CALL(*this, OutputDone(_))
        .WillOnce(VerifyOn(task_env_.GetMainThreadTaskRunner()));
    return base::Bind(&OffloadingVideoDecoderTest::OutputDone,
                      base::Unretained(this));
  }

  VideoDecoder::DecodeCB ExpectDecodeCB(DecodeStatus status) {
    EXPECT_CALL(*this, DecodeDone(status))
        .WillOnce(VerifyOn(task_env_.GetMainThreadTaskRunner()));
    return base::Bind(&OffloadingVideoDecoderTest::DecodeDone,
                      base::Unretained(this));
  }

  base::Closure ExpectResetCB() {
    EXPECT_CALL(*this, ResetDone())
        .WillOnce(VerifyOn(task_env_.GetMainThreadTaskRunner()));
    return base::Bind(&OffloadingVideoDecoderTest::ResetDone,
                      base::Unretained(this));
  }

  void TestNoOffloading(const VideoDecoderConfig& config) {
    // Display name should be a simple passthrough.
    EXPECT_EQ(offloading_decoder_->GetDisplayName(),
              decoder_->GetDisplayName());

    // Verify methods are called on the current thread since the offload codec
    // requirement is not satisfied.
    VideoDecoder::OutputCB output_cb;
    EXPECT_CALL(*decoder_, Initialize(_, false, nullptr, _, _, _))
        .WillOnce(DoAll(VerifyOn(task_env_.GetMainThreadTaskRunner()),
                        RunCallback<3>(true), SaveArg<4>(&output_cb)));
    offloading_decoder_->Initialize(config, false, nullptr, ExpectInitCB(true),
                                    ExpectOutputCB(),
                                    VideoDecoder::WaitingForDecryptionKeyCB());
    task_env_.RunUntilIdle();

    // Verify decode works and is called on the right thread.
    EXPECT_CALL(*decoder_, Decode(_, _))
        .WillOnce(DoAll(VerifyOn(task_env_.GetMainThreadTaskRunner()),
                        RunClosure(base::Bind(output_cb, nullptr)),
                        RunCallback<1>(DecodeStatus::OK)));
    offloading_decoder_->Decode(DecoderBuffer::CreateEOSBuffer(),
                                ExpectDecodeCB(DecodeStatus::OK));
    task_env_.RunUntilIdle();

    // Reset so we can call Initialize() again.
    EXPECT_CALL(*decoder_, Reset(_))
        .WillOnce(DoAll(VerifyOn(task_env_.GetMainThreadTaskRunner()),
                        RunCallback<0>()));
    offloading_decoder_->Reset(ExpectResetCB());
    task_env_.RunUntilIdle();
  }

  void TestOffloading(const VideoDecoderConfig& config, bool detach = false) {
    // Display name should be a simple passthrough.
    EXPECT_EQ(offloading_decoder_->GetDisplayName(),
              decoder_->GetDisplayName());

    // Since this Initialize() should be happening on another thread, set the
    // expectation after we make the call.
    VideoDecoder::OutputCB output_cb;
    if (detach) {
      EXPECT_CALL(*decoder_, Detach())
          .WillOnce(VerifyOn(task_env_.GetMainThreadTaskRunner()));
    }
    offloading_decoder_->Initialize(config, false, nullptr, ExpectInitCB(true),
                                    ExpectOutputCB(),
                                    VideoDecoder::WaitingForDecryptionKeyCB());
    EXPECT_CALL(*decoder_, Initialize(_, false, nullptr, _, _, _))
        .WillOnce(DoAll(VerifyNotOn(task_env_.GetMainThreadTaskRunner()),
                        RunCallback<3>(true), SaveArg<4>(&output_cb)));
    task_env_.RunUntilIdle();

    // Verify decode works and is called on the right thread.
    offloading_decoder_->Decode(DecoderBuffer::CreateEOSBuffer(),
                                ExpectDecodeCB(DecodeStatus::OK));
    EXPECT_CALL(*decoder_, Decode(_, _))
        .WillOnce(DoAll(VerifyNotOn(task_env_.GetMainThreadTaskRunner()),
                        RunClosure(base::Bind(output_cb, nullptr)),
                        RunCallback<1>(DecodeStatus::OK)));
    task_env_.RunUntilIdle();

    // Reset so we can call Initialize() again.
    offloading_decoder_->Reset(ExpectResetCB());
    EXPECT_CALL(*decoder_, Reset(_))
        .WillOnce(DoAll(VerifyNotOn(task_env_.GetMainThreadTaskRunner()),
                        RunCallback<0>()));
    task_env_.RunUntilIdle();
  }

  MOCK_METHOD1(InitDone, void(bool));
  MOCK_METHOD1(OutputDone, void(const scoped_refptr<VideoFrame>&));
  MOCK_METHOD1(DecodeDone, void(DecodeStatus));
  MOCK_METHOD0(ResetDone, void(void));

  base::test::ScopedTaskEnvironment task_env_;
  std::unique_ptr<OffloadingVideoDecoder> offloading_decoder_;
  testing::StrictMock<MockOffloadableVideoDecoder>* decoder_ =
      nullptr;  // Owned by |offloading_decoder_|.

 private:
  DISALLOW_COPY_AND_ASSIGN(OffloadingVideoDecoderTest);
};

TEST_F(OffloadingVideoDecoderTest, NoOffloadingTooSmall) {
  auto offload_config = TestVideoConfig::Large(kCodecVP9);
  CreateWrapper(offload_config.coded_size().width(), kCodecVP9);
  TestNoOffloading(TestVideoConfig::Normal(kCodecVP9));
}

TEST_F(OffloadingVideoDecoderTest, NoOffloadingDifferentCodec) {
  auto offload_config = TestVideoConfig::Large(kCodecVP9);
  CreateWrapper(offload_config.coded_size().width(), kCodecVP9);
  TestNoOffloading(TestVideoConfig::Large(kCodecVP8));
}

TEST_F(OffloadingVideoDecoderTest, NoOffloadingHasEncryption) {
  auto offload_config = TestVideoConfig::Large(kCodecVP9);
  CreateWrapper(offload_config.coded_size().width(), kCodecVP9);
  TestNoOffloading(TestVideoConfig::LargeEncrypted(kCodecVP9));
}

TEST_F(OffloadingVideoDecoderTest, Offloading) {
  auto offload_config = TestVideoConfig::Large(kCodecVP9);
  CreateWrapper(offload_config.coded_size().width(), kCodecVP9);
  TestOffloading(offload_config);
}

TEST_F(OffloadingVideoDecoderTest, OffloadingAfterNoOffloading) {
  auto offload_config = TestVideoConfig::Large(kCodecVP9);
  CreateWrapper(offload_config.coded_size().width(), kCodecVP9);

  // Setup and test the no offloading path first.
  TestNoOffloading(TestVideoConfig::Normal(kCodecVP9));

  // Test offloading now.
  TestOffloading(offload_config, true);

  // Reinitialize decoder with a stream which should not be offloaded. Detach()
  // should be called on the right thread. Again since the first part of this
  // should happen asynchronously, set expectation after the call.
  VideoDecoder::OutputCB output_cb;
  offloading_decoder_->Initialize(
      TestVideoConfig::Normal(kCodecVP9), false, nullptr, ExpectInitCB(true),
      base::Bind(&OffloadingVideoDecoderTest::OutputDone,
                 base::Unretained(this)),
      VideoDecoder::WaitingForDecryptionKeyCB());
  EXPECT_CALL(*decoder_, Detach())
      .WillOnce(VerifyNotOn(task_env_.GetMainThreadTaskRunner()));
  EXPECT_CALL(*decoder_, Initialize(_, false, nullptr, _, _, _))
      .WillOnce(DoAll(VerifyOn(task_env_.GetMainThreadTaskRunner()),
                      RunCallback<3>(true), SaveArg<4>(&output_cb)));
  task_env_.RunUntilIdle();
}

TEST_F(OffloadingVideoDecoderTest, InitializeWithoutDetach) {
  auto offload_config = TestVideoConfig::Large(kCodecVP9);
  CreateWrapper(offload_config.coded_size().width(), kCodecVP9);

  EXPECT_CALL(*decoder_, Detach()).Times(0);
  TestNoOffloading(TestVideoConfig::Normal(kCodecVP9));
  TestNoOffloading(TestVideoConfig::Normal(kCodecVP9));
}

}  // namespace media
