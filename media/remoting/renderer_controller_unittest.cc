// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/renderer_controller.h"

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
#include "media/remoting/fake_remoter.h"
#include "media/remoting/remoting_cdm.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace remoting {

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

class RendererControllerTest : public ::testing::Test,
                               public MediaObserverClient {
 public:
  RendererControllerTest() {}
  ~RendererControllerTest() override {}

  void TearDown() final { RunUntilIdle(); }

  static void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  // MediaObserverClient implementation.
  void SwitchRenderer(bool disable_pipeline_auto_suspend) override {
    is_rendering_remotely_ = controller_->remote_rendering_started();
    disable_pipeline_suspend_ = disable_pipeline_auto_suspend;
  }

  void ActivateViewportIntersectionMonitoring(bool activate) override {
    activate_viewport_intersection_monitoring_ = activate;
  }

  void CreateCdm(bool is_remoting) { is_remoting_cdm_ = is_remoting; }

  base::MessageLoop message_loop_;

 protected:
  std::unique_ptr<RendererController> controller_;
  bool is_rendering_remotely_ = false;
  bool is_remoting_cdm_ = false;
  bool activate_viewport_intersection_monitoring_ = false;
  bool disable_pipeline_suspend_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererControllerTest);
};

TEST_F(RendererControllerTest, ToggleRendererOnFullscreenChange) {
  EXPECT_FALSE(is_rendering_remotely_);
  const scoped_refptr<SharedSession> shared_session =
      FakeRemoterFactory::CreateSharedSession(false);
  controller_ = base::MakeUnique<RendererController>(shared_session);
  controller_->SetClient(this);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_FALSE(activate_viewport_intersection_monitoring_);
  EXPECT_FALSE(disable_pipeline_suspend_);
  shared_session->OnSinkAvailable(kAllCapabilities);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_TRUE(activate_viewport_intersection_monitoring_);
  EXPECT_FALSE(disable_pipeline_suspend_);
  controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnMetadataChanged(DefaultMetadata());
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnRemotePlaybackDisabled(false);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_TRUE(is_rendering_remotely_);  // All requirements now satisfied.
  EXPECT_TRUE(disable_pipeline_suspend_);

  // Leaving fullscreen should shut down remoting.
  controller_->OnExitedFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_FALSE(activate_viewport_intersection_monitoring_);
  EXPECT_FALSE(disable_pipeline_suspend_);
}

TEST_F(RendererControllerTest, ToggleRendererOnSinkCapabilities) {
  EXPECT_FALSE(is_rendering_remotely_);
  const scoped_refptr<SharedSession> shared_session =
      FakeRemoterFactory::CreateSharedSession(false);
  controller_ = base::MakeUnique<RendererController>(shared_session);
  controller_->SetClient(this);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnMetadataChanged(DefaultMetadata());
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnRemotePlaybackDisabled(false);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_FALSE(activate_viewport_intersection_monitoring_);
  EXPECT_FALSE(disable_pipeline_suspend_);
  // An available sink that does not support remote rendering should not cause
  // the controller to toggle remote rendering on.
  shared_session->OnSinkAvailable(mojom::RemotingSinkCapabilities::NONE);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  shared_session->OnSinkGone();  // Bye-bye useless sink!
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_FALSE(activate_viewport_intersection_monitoring_);
  EXPECT_FALSE(disable_pipeline_suspend_);
  // A sink that *does* support remote rendering *does* cause the controller to
  // toggle remote rendering on.
  shared_session->OnSinkAvailable(kAllCapabilities);
  RunUntilIdle();
  EXPECT_TRUE(is_rendering_remotely_);
  EXPECT_TRUE(activate_viewport_intersection_monitoring_);
  EXPECT_TRUE(disable_pipeline_suspend_);
  controller_->OnExitedFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_FALSE(activate_viewport_intersection_monitoring_);
  EXPECT_FALSE(disable_pipeline_suspend_);
}

TEST_F(RendererControllerTest, ToggleRendererOnDisableChange) {
  EXPECT_FALSE(is_rendering_remotely_);
  const scoped_refptr<SharedSession> shared_session =
      FakeRemoterFactory::CreateSharedSession(false);
  controller_ = base::MakeUnique<RendererController>(shared_session);
  controller_->SetClient(this);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnRemotePlaybackDisabled(true);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  shared_session->OnSinkAvailable(kAllCapabilities);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_TRUE(activate_viewport_intersection_monitoring_);
  EXPECT_FALSE(disable_pipeline_suspend_);
  controller_->OnMetadataChanged(DefaultMetadata());
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnRemotePlaybackDisabled(false);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_TRUE(is_rendering_remotely_);  // All requirements now satisfied.
  EXPECT_TRUE(activate_viewport_intersection_monitoring_);
  EXPECT_TRUE(disable_pipeline_suspend_);

  // If the page disables remote playback (e.g., by setting the
  // disableRemotePlayback attribute), this should shut down remoting.
  controller_->OnRemotePlaybackDisabled(true);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_FALSE(activate_viewport_intersection_monitoring_);
  EXPECT_FALSE(disable_pipeline_suspend_);
}

TEST_F(RendererControllerTest, StartFailed) {
  EXPECT_FALSE(is_rendering_remotely_);
  const scoped_refptr<SharedSession> shared_session =
      FakeRemoterFactory::CreateSharedSession(true);
  controller_ = base::MakeUnique<RendererController>(shared_session);
  controller_->SetClient(this);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  shared_session->OnSinkAvailable(kAllCapabilities);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_TRUE(activate_viewport_intersection_monitoring_);
  EXPECT_FALSE(disable_pipeline_suspend_);
  controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnMetadataChanged(DefaultMetadata());
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnRemotePlaybackDisabled(false);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_FALSE(disable_pipeline_suspend_);
}

TEST_F(RendererControllerTest, EncryptedWithRemotingCdm) {
  EXPECT_FALSE(is_rendering_remotely_);
  controller_ = base::MakeUnique<RendererController>(
      FakeRemoterFactory::CreateSharedSession(false));
  controller_->SetClient(this);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnMetadataChanged(EncryptedMetadata());
  controller_->OnRemotePlaybackDisabled(false);
  controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  const scoped_refptr<SharedSession> cdm_shared_session =
      FakeRemoterFactory::CreateSharedSession(false);
  std::unique_ptr<RemotingCdmController> cdm_controller =
      base::MakeUnique<RemotingCdmController>(cdm_shared_session);
  cdm_shared_session->OnSinkAvailable(kAllCapabilities);
  cdm_controller->ShouldCreateRemotingCdm(
      base::Bind(&RendererControllerTest::CreateCdm, base::Unretained(this)));
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_TRUE(is_remoting_cdm_);

  // Create a RemotingCdm with |cdm_controller|.
  const scoped_refptr<RemotingCdm> remoting_cdm = new RemotingCdm(
      std::string(), GURL(), CdmConfig(), SessionMessageCB(), SessionClosedCB(),
      SessionKeysChangeCB(), SessionExpirationUpdateCB(), CdmCreatedCB(),
      std::move(cdm_controller));
  std::unique_ptr<RemotingCdmContext> remoting_cdm_context =
      base::MakeUnique<RemotingCdmContext>(remoting_cdm.get());
  controller_->OnSetCdm(remoting_cdm_context.get());
  RunUntilIdle();
  EXPECT_TRUE(is_rendering_remotely_);

  // For encrypted contents, entering/exiting full screen has no effect.
  controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_TRUE(is_rendering_remotely_);
  controller_->OnExitedFullscreen();
  RunUntilIdle();
  EXPECT_TRUE(is_rendering_remotely_);

  EXPECT_NE(SharedSession::SESSION_PERMANENTLY_STOPPED,
            controller_->session()->state());
  cdm_shared_session->OnSinkGone();
  RunUntilIdle();
  EXPECT_EQ(SharedSession::SESSION_PERMANENTLY_STOPPED,
            controller_->session()->state());
  // Don't switch renderer in this case. Still using the remoting renderer to
  // show the failure interstitial.
  EXPECT_TRUE(is_rendering_remotely_);
}

TEST_F(RendererControllerTest, EncryptedWithLocalCdm) {
  EXPECT_FALSE(is_rendering_remotely_);
  const scoped_refptr<SharedSession> initial_shared_session =
      FakeRemoterFactory::CreateSharedSession(false);
  controller_ = base::MakeUnique<RendererController>(initial_shared_session);
  controller_->SetClient(this);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  initial_shared_session->OnSinkAvailable(kAllCapabilities);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnMetadataChanged(EncryptedMetadata());
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnRemotePlaybackDisabled(false);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);

  const scoped_refptr<SharedSession> cdm_shared_session =
      FakeRemoterFactory::CreateSharedSession(true);
  std::unique_ptr<RemotingCdmController> cdm_controller =
      base::MakeUnique<RemotingCdmController>(cdm_shared_session);
  cdm_shared_session->OnSinkAvailable(kAllCapabilities);
  cdm_controller->ShouldCreateRemotingCdm(
      base::Bind(&RendererControllerTest::CreateCdm, base::Unretained(this)));
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_FALSE(is_remoting_cdm_);
}

TEST_F(RendererControllerTest, EncryptedWithFailedRemotingCdm) {
  EXPECT_FALSE(is_rendering_remotely_);
  controller_ = base::MakeUnique<RendererController>(
      FakeRemoterFactory::CreateSharedSession(false));
  controller_->SetClient(this);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnEnteredFullscreen();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnMetadataChanged(EncryptedMetadata());
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnRemotePlaybackDisabled(false);
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  controller_->OnPlaying();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);

  const scoped_refptr<SharedSession> cdm_shared_session =
      FakeRemoterFactory::CreateSharedSession(false);
  std::unique_ptr<RemotingCdmController> cdm_controller =
      base::MakeUnique<RemotingCdmController>(cdm_shared_session);
  cdm_shared_session->OnSinkAvailable(kAllCapabilities);
  cdm_controller->ShouldCreateRemotingCdm(
      base::Bind(&RendererControllerTest::CreateCdm, base::Unretained(this)));
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_TRUE(is_remoting_cdm_);

  cdm_shared_session->OnSinkGone();
  RunUntilIdle();
  EXPECT_FALSE(is_rendering_remotely_);
  EXPECT_NE(SharedSession::SESSION_PERMANENTLY_STOPPED,
            controller_->session()->state());

  const scoped_refptr<RemotingCdm> remoting_cdm = new RemotingCdm(
      std::string(), GURL(), CdmConfig(), SessionMessageCB(), SessionClosedCB(),
      SessionKeysChangeCB(), SessionExpirationUpdateCB(), CdmCreatedCB(),
      std::move(cdm_controller));
  std::unique_ptr<RemotingCdmContext> remoting_cdm_context =
      base::MakeUnique<RemotingCdmContext>(remoting_cdm.get());
  controller_->OnSetCdm(remoting_cdm_context.get());
  RunUntilIdle();
  // Switch to using the remoting renderer, even when the remoting CDM session
  // was already terminated, to show the failure interstitial.
  EXPECT_TRUE(is_rendering_remotely_);
  EXPECT_EQ(SharedSession::SESSION_PERMANENTLY_STOPPED,
            controller_->session()->state());
}

}  // namespace remoting
}  // namespace media
