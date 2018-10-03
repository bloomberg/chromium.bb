// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <memory>

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/browser/media/session/audio_focus_test_util.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_content_type.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/public/cpp/switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace content {

using media_session::mojom::AudioFocusType;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;

namespace {

class MockMediaSessionPlayerObserver : public MediaSessionPlayerObserver {
 public:
  void OnSuspend(int player_id) override {}
  void OnResume(int player_id) override {}
  void OnSeekForward(int player_id, base::TimeDelta seek_time) override {}
  void OnSeekBackward(int player_id, base::TimeDelta seek_time) override {}
  void OnSetVolumeMultiplier(int player_id, double volume_multiplier) override {
  }
  RenderFrameHost* render_frame_host() const override { return nullptr; }
};

class MockMediaSessionMojoObserver
    : public media_session::mojom::MediaSessionObserver {
 public:
  explicit MockMediaSessionMojoObserver(MediaSessionImpl* media_session)
      : binding_(this) {
    media_session::mojom::MediaSessionObserverPtr observer;
    binding_.Bind(mojo::MakeRequest(&observer));
    media_session->AddObserver(std::move(observer));
  }

  void MediaSessionInfoChanged(MediaSessionInfoPtr session) override {
    session_info_ = std::move(session);
  }

  MediaSessionInfoPtr session_info_;

 private:
  mojo::Binding<media_session::mojom::MediaSessionObserver> binding_;
};

}  // anonymous namespace

class MediaSessionImplTest : public testing::Test {
 public:
  MediaSessionImplTest() = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        media_session::switches::kEnableAudioFocus);

    rph_factory_.reset(new MockRenderProcessHostFactory());
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        rph_factory_.get());
    browser_context_.reset(new TestBrowserContext());
    pepper_observer_.reset(new MockMediaSessionPlayerObserver());
  }

  void TearDown() override {
    browser_context_.reset();
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(nullptr);
    rph_factory_.reset();
  }

  void RequestAudioFocus(MediaSessionImpl* session,
                         AudioFocusType audio_focus_type) {
    session->RequestSystemAudioFocus(audio_focus_type);
  }

  void AbandonAudioFocus(MediaSessionImpl* session) {
    session->AbandonSystemAudioFocusIfNeeded();
  }

  bool GetForceDuck(MediaSessionImpl* session) {
    return test::GetMediaSessionInfoSync(session)->force_duck;
  }

  MediaSessionInfo::SessionState GetState(MediaSessionImpl* session) {
    return test::GetMediaSessionInfoSync(session)->state;
  }

  bool HasMojoObservers(MediaSessionImpl* session) {
    return !session->mojo_observers_.empty();
  }

  void FlushForTesting(MediaSessionImpl* session) {
    session->FlushForTesting();
  }

  std::unique_ptr<WebContents> CreateWebContents() {
    return TestWebContents::Create(
        browser_context_.get(), SiteInstance::Create(browser_context_.get()));
  }

  std::unique_ptr<MediaSessionPlayerObserver> pepper_observer_;

 private:
  TestBrowserThreadBundle test_browser_thread_bundle_;

  std::unique_ptr<MockRenderProcessHostFactory> rph_factory_;
  std::unique_ptr<TestBrowserContext> browser_context_;
};

TEST_F(MediaSessionImplTest, SessionInfoState) {
  std::unique_ptr<WebContents> web_contents(CreateWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());
  EXPECT_EQ(MediaSessionInfo::SessionState::kInactive, GetState(media_session));

  {
    MockMediaSessionMojoObserver observer(media_session);
    RequestAudioFocus(media_session, AudioFocusType::kGain);
    FlushForTesting(media_session);

    EXPECT_EQ(MediaSessionInfo::SessionState::kActive, GetState(media_session));
    EXPECT_TRUE(observer.session_info_.Equals(
        test::GetMediaSessionInfoSync(media_session)));
  }

  {
    MockMediaSessionMojoObserver observer(media_session);
    media_session->StartDucking();
    FlushForTesting(media_session);

    EXPECT_EQ(MediaSessionInfo::SessionState::kDucking,
              GetState(media_session));
    EXPECT_TRUE(observer.session_info_.Equals(
        test::GetMediaSessionInfoSync(media_session)));
  }

  {
    MockMediaSessionMojoObserver observer(media_session);
    media_session->StopDucking();
    FlushForTesting(media_session);

    EXPECT_EQ(MediaSessionInfo::SessionState::kActive, GetState(media_session));
    EXPECT_TRUE(observer.session_info_.Equals(
        test::GetMediaSessionInfoSync(media_session)));
  }

  {
    MockMediaSessionMojoObserver observer(media_session);
    media_session->Suspend(MediaSession::SuspendType::kSystem);
    FlushForTesting(media_session);

    EXPECT_EQ(MediaSessionInfo::SessionState::kSuspended,
              GetState(media_session));
    EXPECT_TRUE(observer.session_info_.Equals(
        test::GetMediaSessionInfoSync(media_session)));
  }

  {
    MockMediaSessionMojoObserver observer(media_session);
    media_session->Resume(MediaSession::SuspendType::kSystem);
    FlushForTesting(media_session);

    EXPECT_EQ(MediaSessionInfo::SessionState::kActive, GetState(media_session));
    EXPECT_TRUE(observer.session_info_.Equals(
        test::GetMediaSessionInfoSync(media_session)));
  }

  {
    MockMediaSessionMojoObserver observer(media_session);
    AbandonAudioFocus(media_session);
    FlushForTesting(media_session);

    EXPECT_EQ(MediaSessionInfo::SessionState::kInactive,
              GetState(media_session));
    EXPECT_TRUE(observer.session_info_.Equals(
        test::GetMediaSessionInfoSync(media_session)));
  }
}

TEST_F(MediaSessionImplTest, PepperForcesDuckAndRequestsFocus) {
  std::unique_ptr<WebContents> web_contents(CreateWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());

  media_session->AddPlayer(pepper_observer_.get(), 0,
                           media::MediaContentType::Pepper);
  EXPECT_EQ(MediaSessionInfo::SessionState::kActive, GetState(media_session));
  EXPECT_TRUE(GetForceDuck(media_session));

  media_session->RemovePlayer(pepper_observer_.get(), 0);
  EXPECT_EQ(MediaSessionInfo::SessionState::kInactive, GetState(media_session));
  EXPECT_FALSE(GetForceDuck(media_session));
}

TEST_F(MediaSessionImplTest, RegisterMojoObserver) {
  std::unique_ptr<WebContents> web_contents(CreateWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());

  EXPECT_FALSE(HasMojoObservers(media_session));

  MockMediaSessionMojoObserver observer(media_session);
  FlushForTesting(media_session);

  EXPECT_TRUE(HasMojoObservers(media_session));
}

#if !defined(OS_ANDROID)

TEST_F(MediaSessionImplTest, WebContentsDestroyed_ReleasesFocus) {
  std::unique_ptr<WebContents> web_contents(CreateWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());

  {
    test::TestAudioFocusObserver observer;
    RequestAudioFocus(media_session, AudioFocusType::kGain);
    observer.WaitForGainedEvent();
  }

  EXPECT_EQ(MediaSessionInfo::SessionState::kActive, GetState(media_session));

  {
    test::TestAudioFocusObserver observer;
    web_contents.reset();
    observer.WaitForLostEvent();
  }
}

TEST_F(MediaSessionImplTest, WebContentsDestroyed_ReleasesTransients) {
  std::unique_ptr<WebContents> web_contents(CreateWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());

  {
    test::TestAudioFocusObserver observer;
    RequestAudioFocus(media_session, AudioFocusType::kGainTransientMayDuck);
    observer.WaitForGainedEvent();
  }

  EXPECT_EQ(MediaSessionInfo::SessionState::kActive, GetState(media_session));

  {
    test::TestAudioFocusObserver observer;
    web_contents.reset();
    observer.WaitForLostEvent();
  }
}

TEST_F(MediaSessionImplTest, WebContentsDestroyed_StopsDucking) {
  std::unique_ptr<WebContents> web_contents_1(CreateWebContents());
  MediaSessionImpl* media_session_1 =
      MediaSessionImpl::Get(web_contents_1.get());

  std::unique_ptr<WebContents> web_contents_2(CreateWebContents());
  MediaSessionImpl* media_session_2 =
      MediaSessionImpl::Get(web_contents_2.get());

  {
    test::TestAudioFocusObserver observer;
    RequestAudioFocus(media_session_1, AudioFocusType::kGain);
    observer.WaitForGainedEvent();
  }

  EXPECT_NE(MediaSessionInfo::SessionState::kDucking,
            GetState(media_session_1));

  {
    test::TestAudioFocusObserver observer;
    RequestAudioFocus(media_session_2, AudioFocusType::kGainTransientMayDuck);
    observer.WaitForGainedEvent();
  }

  EXPECT_EQ(MediaSessionInfo::SessionState::kDucking,
            GetState(media_session_1));

  {
    test::TestAudioFocusObserver observer;
    web_contents_2.reset();
    observer.WaitForLostEvent();
  }

  EXPECT_NE(MediaSessionInfo::SessionState::kDucking,
            GetState(media_session_1));
}

#endif  // !defined(OS_ANDROID)

}  // namespace content
