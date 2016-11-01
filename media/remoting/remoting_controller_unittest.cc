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
#include "media/remoting/fake_remoting_controller.h"
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
