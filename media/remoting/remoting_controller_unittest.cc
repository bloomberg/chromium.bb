// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_controller.h"

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/limits.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

constexpr gfx::Size kCodedSize(320, 240);
constexpr gfx::Rect kVisibleRect(320, 240);
constexpr gfx::Size kNaturalSize(320, 240);

PipelineMetadata defaultMetadata() {
  PipelineMetadata data;
  data.has_audio = true;
  data.has_video = true;
  data.video_decoder_config = VideoDecoderConfig(
      kCodecVP8, VP8PROFILE_ANY, VideoPixelFormat::PIXEL_FORMAT_I420,
      ColorSpace::COLOR_SPACE_SD_REC601, kCodedSize, kVisibleRect, kNaturalSize,
      EmptyExtraData(), Unencrypted());
  data.audio_decoder_config = AudioDecoderConfig(
      kCodecOpus, SampleFormat::kSampleFormatU8,
      ChannelLayout::CHANNEL_LAYOUT_MONO, limits::kMinSampleRate,
      EmptyExtraData(), Unencrypted());
  return data;
}

class FakeRemoter final : public mojom::Remoter {
 public:
  // |start_will_fail| indicates whether starting remoting will fail.
  FakeRemoter(mojom::RemotingSourcePtr source, bool start_will_fail)
      : source_(std::move(source)), start_will_fail_(start_will_fail) {}
  ~FakeRemoter() override {}

  // mojom::Remoter implementations.
  void Start() override {
    if (start_will_fail_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&FakeRemoter::StartFailed, base::Unretained(this)));
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&FakeRemoter::Started, base::Unretained(this)));
    }
  }

  void StartDataStreams(
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      mojom::RemotingDataStreamSenderRequest audio_sender_request,
      mojom::RemotingDataStreamSenderRequest video_sender_request) override {}

  void Stop(mojom::RemotingStopReason reason) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&FakeRemoter::Stopped, base::Unretained(this), reason));
  }

  void SendMessageToSink(const std::vector<uint8_t>& message) override {}

 private:
  void Started() { source_->OnStarted(); }
  void StartFailed() {
    source_->OnStartFailed(mojom::RemotingStartFailReason::ROUTE_TERMINATED);
  }
  void Stopped(mojom::RemotingStopReason reason) { source_->OnStopped(reason); }

  mojom::RemotingSourcePtr source_;
  bool start_will_fail_;

  DISALLOW_COPY_AND_ASSIGN(FakeRemoter);
};

class FakeRemoterFactory final : public mojom::RemoterFactory {
 public:
  // |start_will_fail| indicates whether starting remoting will fail.
  explicit FakeRemoterFactory(bool start_will_fail)
      : start_will_fail_(start_will_fail) {}
  ~FakeRemoterFactory() override {}

  void Create(mojom::RemotingSourcePtr source,
              mojom::RemoterRequest request) override {
    mojo::MakeStrongBinding(
        base::MakeUnique<FakeRemoter>(std::move(source), start_will_fail_),
        std::move(request));
  }

 private:
  bool start_will_fail_;

  DISALLOW_COPY_AND_ASSIGN(FakeRemoterFactory);
};

std::unique_ptr<RemotingController> CreateRemotingController(
    bool start_will_fail) {
  mojom::RemotingSourcePtr remoting_source;
  mojom::RemotingSourceRequest remoting_source_request =
      mojo::GetProxy(&remoting_source);
  mojom::RemoterPtr remoter;
  std::unique_ptr<mojom::RemoterFactory> remoter_factory =
      base::MakeUnique<FakeRemoterFactory>(start_will_fail);
  remoter_factory->Create(std::move(remoting_source), mojo::GetProxy(&remoter));
  std::unique_ptr<RemotingController> remoting_controller =
      base::MakeUnique<RemotingController>(std::move(remoting_source_request),
                                           std::move(remoter));
  return remoting_controller;
}

}  // namespace

class RemotingControllerTest : public ::testing::Test {
 public:
  RemotingControllerTest()
      : remoting_controller_(CreateRemotingController(false)),
        is_remoting_(false) {
    remoting_controller_->SetSwitchRendererCallback(base::Bind(
        &RemotingControllerTest::ToggleRenderer, base::Unretained(this)));
  }
  ~RemotingControllerTest() override {}

  void TearDown() final { RunUntilIdle(); }

  static void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  void ToggleRenderer() { is_remoting_ = remoting_controller_->is_remoting(); }

  base::MessageLoop message_loop_;

 protected:
  std::unique_ptr<RemotingController> remoting_controller_;
  bool is_remoting_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemotingControllerTest);
};

TEST_F(RemotingControllerTest, ToggleRenderer) {
  EXPECT_FALSE(is_remoting_);
  remoting_controller_->OnSinkAvailable();
  remoting_controller_->OnEnteredFullscreen();
  EXPECT_FALSE(is_remoting_);
  remoting_controller_->OnMetadataChanged(defaultMetadata());
  RunUntilIdle();
  EXPECT_TRUE(is_remoting_);
  remoting_controller_->OnExitedFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_remoting_);
}

TEST_F(RemotingControllerTest, StartFailed) {
  EXPECT_FALSE(is_remoting_);
  remoting_controller_ = CreateRemotingController(true);
  remoting_controller_->SetSwitchRendererCallback(base::Bind(
      &RemotingControllerTest::ToggleRenderer, base::Unretained(this)));
  remoting_controller_->OnSinkAvailable();
  remoting_controller_->OnEnteredFullscreen();
  remoting_controller_->OnMetadataChanged(defaultMetadata());
  RunUntilIdle();
  EXPECT_FALSE(is_remoting_);
}

}  // namespace media
