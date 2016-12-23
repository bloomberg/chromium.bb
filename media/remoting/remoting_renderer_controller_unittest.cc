// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_renderer_controller.h"

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_config.h"
#include "media/base/limits.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder_config.h"
#include "media/remoting/fake_remoting_controller.h"
#include "media/remoting/remoting_cdm.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

constexpr mojom::RemotingSinkCapabilities kAllCapabilities =
    mojom::RemotingSinkCapabilities::CONTENT_DECRYPTION_AND_RENDERING;

PipelineMetadata DefaultMetadata() {
  PipelineMetadata data;
  data.has_audio = true;
  data.has_video = true;
  data.video_decoder_config = TestVideoConfig::Normal();
  return data;
}

PipelineMetadata EncryptedMetadata() {
  PipelineMetadata data;
  data.has_audio = true;
  data.has_video = true;
  data.video_decoder_config = TestVideoConfig::NormalEncrypted();
  return data;
}

}  // namespace

class RemotingRendererControllerTest : public ::testing::Test {
 public:
  RemotingRendererControllerTest() {}
  ~RemotingRendererControllerTest() override {}

  void TearDown() final { RunUntilIdle(); }

  static void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  void ToggleRenderer() {
    is_rendering_remotely_ =
        remoting_renderer_controller_->remote_rendering_started();
  }

  void CreateCdm(bool is_remoting) { is_remoting_cdm_ = is_remoting; }

  base::MessageLoop message_loop_;

 protected:
  std::unique_ptr<RemotingRendererController> remoting_renderer_controller_;
  bool is_rendering_remotely_ = false;
  bool is_remoting_cdm_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemotingRendererControllerTest);
};

TEST_F(RemotingRendererControllerTest, ToggleRendererOnFullscreenChange) {
  EXPECT_FALSE(is_rendering_remotely_);
  scoped_refptr<RemotingSourceImpl> remoting_source_impl =
      CreateRemotingSourceImpl(false);
  remoting_renderer_controller_ =
      base::MakeUnique<RemotingRendererController>(remoting_source_impl);
  remoting_renderer_controller_->SetSwitchRendererCallback(base::Bind(
      &RemotingRendererControllerTest::ToggleRenderer, base::Unretained(this)));
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_source_impl->OnSinkAvailable(kAllCapabilities);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnMetadataChanged(DefaultMetadata());
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnRemotePlaybackDisabled(false);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_TRUE(is_rendering_remotely_);  // All requirements now satisfied.

  // Leaving fullscreen should shut down remoting.
  remoting_renderer_controller_->OnExitedFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
}

TEST_F(RemotingRendererControllerTest, ToggleRendererOnSinkCapabilities) {
  EXPECT_FALSE(is_rendering_remotely_);
  scoped_refptr<RemotingSourceImpl> remoting_source_impl =
      CreateRemotingSourceImpl(false);
  remoting_renderer_controller_ =
      base::MakeUnique<RemotingRendererController>(remoting_source_impl);
  remoting_renderer_controller_->SetSwitchRendererCallback(base::Bind(
      &RemotingRendererControllerTest::ToggleRenderer, base::Unretained(this)));
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnMetadataChanged(DefaultMetadata());
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnRemotePlaybackDisabled(false);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  // An available sink that does not support remote rendering should not cause
  // the controller to toggle remote rendering on.
  remoting_source_impl->OnSinkAvailable(mojom::RemotingSinkCapabilities::NONE);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_source_impl->OnSinkGone();  // Bye-bye useless sink!
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  // A sink that *does* support remote rendering *does* cause the controller to
  // toggle remote rendering on.
  remoting_source_impl->OnSinkAvailable(kAllCapabilities);
  RunUntilIdle();
  EXPECT_TRUE(is_rendering_remotely_);
  remoting_renderer_controller_->OnExitedFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
}

TEST_F(RemotingRendererControllerTest, ToggleRendererOnDisableChange) {
  EXPECT_FALSE(is_rendering_remotely_);
  scoped_refptr<RemotingSourceImpl> remoting_source_impl =
      CreateRemotingSourceImpl(false);
  remoting_renderer_controller_ =
      base::MakeUnique<RemotingRendererController>(remoting_source_impl);
  remoting_renderer_controller_->SetSwitchRendererCallback(base::Bind(
      &RemotingRendererControllerTest::ToggleRenderer, base::Unretained(this)));
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnRemotePlaybackDisabled(true);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_source_impl->OnSinkAvailable(kAllCapabilities);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnMetadataChanged(DefaultMetadata());
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnRemotePlaybackDisabled(false);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_TRUE(is_rendering_remotely_);  // All requirements now satisfied.

  // If the page disables remote playback (e.g., by setting the
  // disableRemotePlayback attribute), this should shut down remoting.
  remoting_renderer_controller_->OnRemotePlaybackDisabled(true);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
}

TEST_F(RemotingRendererControllerTest, StartFailed) {
  EXPECT_FALSE(is_rendering_remotely_);
  scoped_refptr<RemotingSourceImpl> remoting_source_impl =
      CreateRemotingSourceImpl(true);
  remoting_renderer_controller_ =
      base::MakeUnique<RemotingRendererController>(remoting_source_impl);
  remoting_renderer_controller_->SetSwitchRendererCallback(base::Bind(
      &RemotingRendererControllerTest::ToggleRenderer, base::Unretained(this)));
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_source_impl->OnSinkAvailable(kAllCapabilities);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnMetadataChanged(DefaultMetadata());
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnRemotePlaybackDisabled(false);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
}

TEST_F(RemotingRendererControllerTest, EncryptedWithRemotingCdm) {
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_ = base::MakeUnique<RemotingRendererController>(
      CreateRemotingSourceImpl(false));
  remoting_renderer_controller_->SetSwitchRendererCallback(base::Bind(
      &RemotingRendererControllerTest::ToggleRenderer, base::Unretained(this)));
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnMetadataChanged(EncryptedMetadata());
  remoting_renderer_controller_->OnRemotePlaybackDisabled(false);
  remoting_renderer_controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  scoped_refptr<RemotingSourceImpl> cdm_remoting_source_impl =
      CreateRemotingSourceImpl(false);
  std::unique_ptr<RemotingCdmController> remoting_cdm_controller =
      base::MakeUnique<RemotingCdmController>(cdm_remoting_source_impl);
  cdm_remoting_source_impl->OnSinkAvailable(kAllCapabilities);
  remoting_cdm_controller->ShouldCreateRemotingCdm(base::Bind(
      &RemotingRendererControllerTest::CreateCdm, base::Unretained(this)));
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_TRUE(is_remoting_cdm_);

  // Create a RemotingCdm with |remoting_cdm_controller|.
  scoped_refptr<RemotingCdm> remoting_cdm = new RemotingCdm(
      std::string(), GURL(), CdmConfig(), SessionMessageCB(), SessionClosedCB(),
      SessionKeysChangeCB(), SessionExpirationUpdateCB(), CdmCreatedCB(),
      std::move(remoting_cdm_controller));
  std::unique_ptr<RemotingCdmContext> remoting_cdm_context =
      base::MakeUnique<RemotingCdmContext>(remoting_cdm.get());
  remoting_renderer_controller_->OnSetCdm(remoting_cdm_context.get());
  RunUntilIdle();
  EXPECT_TRUE(is_rendering_remotely_);

  // For encrypted contents, entering/exiting full screen has no effect.
  remoting_renderer_controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_TRUE(is_rendering_remotely_);
  remoting_renderer_controller_->OnExitedFullscreen();
  RunUntilIdle();
  EXPECT_TRUE(is_rendering_remotely_);

  EXPECT_NE(RemotingSessionState::SESSION_PERMANENTLY_STOPPED,
            remoting_renderer_controller_->remoting_source()->state());
  cdm_remoting_source_impl->OnSinkGone();
  RunUntilIdle();
  EXPECT_EQ(RemotingSessionState::SESSION_PERMANENTLY_STOPPED,
            remoting_renderer_controller_->remoting_source()->state());
  // Don't switch renderer in this case. Still using the remoting renderer to
  // show the failure interstitial.
  EXPECT_TRUE(is_rendering_remotely_);
}

TEST_F(RemotingRendererControllerTest, EncryptedWithLocalCdm) {
  EXPECT_FALSE(is_rendering_remotely_);
  scoped_refptr<RemotingSourceImpl> renderer_remoting_source_impl =
      CreateRemotingSourceImpl(false);
  remoting_renderer_controller_ = base::MakeUnique<RemotingRendererController>(
      renderer_remoting_source_impl);
  remoting_renderer_controller_->SetSwitchRendererCallback(base::Bind(
      &RemotingRendererControllerTest::ToggleRenderer, base::Unretained(this)));
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  renderer_remoting_source_impl->OnSinkAvailable(kAllCapabilities);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnMetadataChanged(EncryptedMetadata());
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnRemotePlaybackDisabled(false);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);

  scoped_refptr<RemotingSourceImpl> cdm_remoting_source_impl =
      CreateRemotingSourceImpl(true);
  std::unique_ptr<RemotingCdmController> remoting_cdm_controller =
      base::MakeUnique<RemotingCdmController>(cdm_remoting_source_impl);
  cdm_remoting_source_impl->OnSinkAvailable(kAllCapabilities);
  remoting_cdm_controller->ShouldCreateRemotingCdm(base::Bind(
      &RemotingRendererControllerTest::CreateCdm, base::Unretained(this)));
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_FALSE(is_remoting_cdm_);
}

TEST_F(RemotingRendererControllerTest, EncryptedWithFailedRemotingCdm) {
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_ = base::MakeUnique<RemotingRendererController>(
      CreateRemotingSourceImpl(false));
  remoting_renderer_controller_->SetSwitchRendererCallback(base::Bind(
      &RemotingRendererControllerTest::ToggleRenderer, base::Unretained(this)));
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnMetadataChanged(EncryptedMetadata());
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnRemotePlaybackDisabled(false);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  remoting_renderer_controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);

  scoped_refptr<RemotingSourceImpl> cdm_remoting_source_impl =
      CreateRemotingSourceImpl(false);
  std::unique_ptr<RemotingCdmController> remoting_cdm_controller =
      base::MakeUnique<RemotingCdmController>(cdm_remoting_source_impl);
  cdm_remoting_source_impl->OnSinkAvailable(kAllCapabilities);
  remoting_cdm_controller->ShouldCreateRemotingCdm(base::Bind(
      &RemotingRendererControllerTest::CreateCdm, base::Unretained(this)));
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_TRUE(is_remoting_cdm_);

  cdm_remoting_source_impl->OnSinkGone();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_NE(RemotingSessionState::SESSION_PERMANENTLY_STOPPED,
            remoting_renderer_controller_->remoting_source()->state());

  scoped_refptr<RemotingCdm> remoting_cdm = new RemotingCdm(
      std::string(), GURL(), CdmConfig(), SessionMessageCB(), SessionClosedCB(),
      SessionKeysChangeCB(), SessionExpirationUpdateCB(), CdmCreatedCB(),
      std::move(remoting_cdm_controller));
  std::unique_ptr<RemotingCdmContext> remoting_cdm_context =
      base::MakeUnique<RemotingCdmContext>(remoting_cdm.get());
  remoting_renderer_controller_->OnSetCdm(remoting_cdm_context.get());
  RunUntilIdle();
  // Switch to using the remoting renderer, even when the remoting CDM session
  // was already terminated, to show the failure interstitial.
  EXPECT_TRUE(is_rendering_remotely_);
  EXPECT_EQ(RemotingSessionState::SESSION_PERMANENTLY_STOPPED,
            remoting_renderer_controller_->remoting_source()->state());
}

}  // namespace media
