// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_manager.h"

#include "content/browser/media/session/media_session.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/test/test_web_contents.h"

namespace content {

using AudioFocusType = AudioFocusManager::AudioFocusType;
using SuspendType = MediaSession::SuspendType;

class AudioFocusManagerTest : public testing::Test {
 public:
  const double kDuckingVolumeMultiplier = 0.2;
  const double kDefaultVolumeMultiplier = 1.0;

  AudioFocusManagerTest() : ui_thread_(BrowserThread::UI, &message_loop_) {}

  void SetUp() override {
    rph_factory_.reset(new MockRenderProcessHostFactory());
    SiteInstanceImpl::set_render_process_host_factory(rph_factory_.get());
    browser_context_.reset(new TestBrowserContext());
  }

  void TearDown() override {
    browser_context_.reset();
    SiteInstanceImpl::set_render_process_host_factory(nullptr);
    rph_factory_.reset();
  }

  WebContents* GetAudioFocusedContent() const {
    if (!AudioFocusManager::GetInstance()->focus_entry_)
      return nullptr;
    return AudioFocusManager::GetInstance()->focus_entry_->web_contents();
  }

  int GetTransientMaybeDuckCount() const {
    return AudioFocusManager::GetInstance()->TransientMayDuckEntriesCount();
  }

  double GetVolumeMultiplier(MediaSession* session) {
    return session->volume_multiplier_;
  }

  WebContents* CreateWebContents() {
    return TestWebContents::Create(browser_context_.get(),
        SiteInstance::SiteInstance::Create(browser_context_.get()));
  }

 private:
  base::MessageLoopForUI message_loop_;
  TestBrowserThread ui_thread_;

  std::unique_ptr<MockRenderProcessHostFactory> rph_factory_;
  std::unique_ptr<TestBrowserContext> browser_context_;
};

TEST_F(AudioFocusManagerTest, InstanceAvailableAndSame) {
  AudioFocusManager* audio_focus_manager = AudioFocusManager::GetInstance();
  ASSERT_TRUE(!!audio_focus_manager);
  ASSERT_EQ(audio_focus_manager, AudioFocusManager::GetInstance());
}

TEST_F(AudioFocusManagerTest, RequestAudioFocusGain_ReplaceFocusedEntry) {
  std::unique_ptr<WebContents> web_contents_1(CreateWebContents());
  MediaSession* media_session_1 = MediaSession::Get(web_contents_1.get());

  std::unique_ptr<WebContents> web_contents_2(CreateWebContents());
  MediaSession* media_session_2 = MediaSession::Get(web_contents_2.get());

  std::unique_ptr<WebContents> web_contents_3(CreateWebContents());
  MediaSession* media_session_3 = MediaSession::Get(web_contents_3.get());

  ASSERT_EQ(nullptr, GetAudioFocusedContent());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_1, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(web_contents_1.get(), GetAudioFocusedContent());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_2, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(web_contents_2.get(), GetAudioFocusedContent());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_3, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(web_contents_3.get(), GetAudioFocusedContent());
}

TEST_F(AudioFocusManagerTest, RequestAudioFocusGain_Duplicate) {
  std::unique_ptr<WebContents> web_contents(CreateWebContents());
  MediaSession* media_session = MediaSession::Get(web_contents.get());

  ASSERT_EQ(nullptr, GetAudioFocusedContent());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(web_contents.get(), GetAudioFocusedContent());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(web_contents.get(), GetAudioFocusedContent());
}

TEST_F(AudioFocusManagerTest, RequestAudioFocusGain_FromTransient) {
  std::unique_ptr<WebContents> web_contents(CreateWebContents());
  MediaSession* media_session = MediaSession::Get(web_contents.get());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session, AudioFocusManager::AudioFocusType::GainTransientMayDuck);
  ASSERT_EQ(nullptr, GetAudioFocusedContent());
  ASSERT_EQ(1, GetTransientMaybeDuckCount());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(web_contents.get(), GetAudioFocusedContent());
  ASSERT_EQ(0, GetTransientMaybeDuckCount());
}

TEST_F(AudioFocusManagerTest, RequestAudioFocusTransient_FromGain) {
  std::unique_ptr<WebContents> web_contents(CreateWebContents());
  MediaSession* media_session = MediaSession::Get(web_contents.get());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(web_contents.get(), GetAudioFocusedContent());
  ASSERT_EQ(0, GetTransientMaybeDuckCount());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session, AudioFocusManager::AudioFocusType::GainTransientMayDuck);
  ASSERT_EQ(nullptr, GetAudioFocusedContent());
  ASSERT_EQ(1, GetTransientMaybeDuckCount());
  ASSERT_EQ(kDefaultVolumeMultiplier, GetVolumeMultiplier(media_session));
}

TEST_F(AudioFocusManagerTest, RequestAudioFocusTransient_FromGainWhileDucking) {
  std::unique_ptr<WebContents> web_contents_1(CreateWebContents());
  MediaSession* media_session_1 = MediaSession::Get(web_contents_1.get());

  std::unique_ptr<WebContents> web_contents_2(CreateWebContents());
  MediaSession* media_session_2 = MediaSession::Get(web_contents_2.get());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_1, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(0, GetTransientMaybeDuckCount());
  ASSERT_EQ(kDefaultVolumeMultiplier, GetVolumeMultiplier(media_session_1));

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_2, AudioFocusManager::AudioFocusType::GainTransientMayDuck);
  ASSERT_EQ(1, GetTransientMaybeDuckCount());
  ASSERT_EQ(kDuckingVolumeMultiplier, GetVolumeMultiplier(media_session_1));

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_1, AudioFocusManager::AudioFocusType::GainTransientMayDuck);
  ASSERT_EQ(2, GetTransientMaybeDuckCount());
  ASSERT_EQ(kDefaultVolumeMultiplier, GetVolumeMultiplier(media_session_1));
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_RemovesFocusedEntry) {
  std::unique_ptr<WebContents> web_contents(CreateWebContents());
  MediaSession* media_session = MediaSession::Get(web_contents.get());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(web_contents.get(), GetAudioFocusedContent());

  AudioFocusManager::GetInstance()->AbandonAudioFocus(media_session);
  ASSERT_EQ(nullptr, GetAudioFocusedContent());
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_NoAssociatedEntry) {
  std::unique_ptr<WebContents> web_contents(CreateWebContents());
  MediaSession* media_session = MediaSession::Get(web_contents.get());

  AudioFocusManager::GetInstance()->AbandonAudioFocus(media_session);
  ASSERT_EQ(nullptr, GetAudioFocusedContent());
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_RemovesTransientEntry) {
  std::unique_ptr<WebContents> web_contents(CreateWebContents());
  MediaSession* media_session = MediaSession::Get(web_contents.get());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session, AudioFocusManager::AudioFocusType::GainTransientMayDuck);
  ASSERT_EQ(1, GetTransientMaybeDuckCount());

  AudioFocusManager::GetInstance()->AbandonAudioFocus(media_session);
  ASSERT_EQ(0, GetTransientMaybeDuckCount());
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_WhileDuckingThenResume) {
  std::unique_ptr<WebContents> web_contents_1(CreateWebContents());
  MediaSession* media_session_1 = MediaSession::Get(web_contents_1.get());

  std::unique_ptr<WebContents> web_contents_2(CreateWebContents());
  MediaSession* media_session_2 = MediaSession::Get(web_contents_2.get());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_1, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(0, GetTransientMaybeDuckCount());
  ASSERT_EQ(kDefaultVolumeMultiplier, GetVolumeMultiplier(media_session_1));

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_2, AudioFocusManager::AudioFocusType::GainTransientMayDuck);
  ASSERT_EQ(1, GetTransientMaybeDuckCount());
  ASSERT_EQ(kDuckingVolumeMultiplier, GetVolumeMultiplier(media_session_1));

  AudioFocusManager::GetInstance()->AbandonAudioFocus(media_session_1);
  ASSERT_EQ(1, GetTransientMaybeDuckCount());

  AudioFocusManager::GetInstance()->AbandonAudioFocus(media_session_2);
  ASSERT_EQ(0, GetTransientMaybeDuckCount());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_1, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(kDefaultVolumeMultiplier, GetVolumeMultiplier(media_session_1));
}

TEST_F(AudioFocusManagerTest, AbandonAudioFocus_StopsDucking) {
  std::unique_ptr<WebContents> web_contents_1(CreateWebContents());
  MediaSession* media_session_1 = MediaSession::Get(web_contents_1.get());

  std::unique_ptr<WebContents> web_contents_2(CreateWebContents());
  MediaSession* media_session_2 = MediaSession::Get(web_contents_2.get());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_1, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(0, GetTransientMaybeDuckCount());
  ASSERT_EQ(kDefaultVolumeMultiplier, GetVolumeMultiplier(media_session_1));

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_2, AudioFocusManager::AudioFocusType::GainTransientMayDuck);
  ASSERT_EQ(1, GetTransientMaybeDuckCount());
  ASSERT_EQ(kDuckingVolumeMultiplier, GetVolumeMultiplier(media_session_1));

  AudioFocusManager::GetInstance()->AbandonAudioFocus(media_session_2);
  ASSERT_EQ(0, GetTransientMaybeDuckCount());
  ASSERT_EQ(kDefaultVolumeMultiplier, GetVolumeMultiplier(media_session_1));

}

TEST_F(AudioFocusManagerTest, DuckWhilePlaying) {
  std::unique_ptr<WebContents> web_contents_1(CreateWebContents());
  MediaSession* media_session_1 = MediaSession::Get(web_contents_1.get());

  std::unique_ptr<WebContents> web_contents_2(CreateWebContents());
  MediaSession* media_session_2 = MediaSession::Get(web_contents_2.get());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_1, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(kDefaultVolumeMultiplier, GetVolumeMultiplier(media_session_1));

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_2, AudioFocusManager::AudioFocusType::GainTransientMayDuck);
  ASSERT_EQ(kDuckingVolumeMultiplier, GetVolumeMultiplier(media_session_1));
}

TEST_F(AudioFocusManagerTest, DuckWhenStarting) {
  std::unique_ptr<WebContents> web_contents_1(CreateWebContents());
  MediaSession* media_session_1 = MediaSession::Get(web_contents_1.get());

  std::unique_ptr<WebContents> web_contents_2(CreateWebContents());
  MediaSession* media_session_2 = MediaSession::Get(web_contents_2.get());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_2, AudioFocusManager::AudioFocusType::GainTransientMayDuck);

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_1, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(kDuckingVolumeMultiplier, GetVolumeMultiplier(media_session_1));
}

TEST_F(AudioFocusManagerTest, DuckWithMultipleTransients) {
  std::unique_ptr<WebContents> web_contents_1(CreateWebContents());
  MediaSession* media_session_1 = MediaSession::Get(web_contents_1.get());

  std::unique_ptr<WebContents> web_contents_2(CreateWebContents());
  MediaSession* media_session_2 = MediaSession::Get(web_contents_2.get());

  std::unique_ptr<WebContents> web_contents_3(CreateWebContents());
  MediaSession* media_session_3 = MediaSession::Get(web_contents_3.get());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_1, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(kDefaultVolumeMultiplier, GetVolumeMultiplier(media_session_1));

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_2, AudioFocusManager::AudioFocusType::GainTransientMayDuck);
  ASSERT_EQ(kDuckingVolumeMultiplier, GetVolumeMultiplier(media_session_1));

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_3, AudioFocusManager::AudioFocusType::GainTransientMayDuck);
  ASSERT_EQ(kDuckingVolumeMultiplier, GetVolumeMultiplier(media_session_1));

  AudioFocusManager::GetInstance()->AbandonAudioFocus(media_session_2);
  ASSERT_EQ(kDuckingVolumeMultiplier, GetVolumeMultiplier(media_session_1));

  AudioFocusManager::GetInstance()->AbandonAudioFocus(media_session_3);
  ASSERT_EQ(kDefaultVolumeMultiplier, GetVolumeMultiplier(media_session_1));
}

TEST_F(AudioFocusManagerTest, WebContentsDestroyed_ReleasesFocus) {
  std::unique_ptr<WebContents> web_contents(CreateWebContents());
  MediaSession* media_session = MediaSession::Get(web_contents.get());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(web_contents.get(), GetAudioFocusedContent());

  web_contents.reset();
  ASSERT_EQ(nullptr, GetAudioFocusedContent());
}

TEST_F(AudioFocusManagerTest, WebContentsDestroyed_ReleasesTransients) {
  std::unique_ptr<WebContents> web_contents(CreateWebContents());
  MediaSession* media_session = MediaSession::Get(web_contents.get());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session, AudioFocusManager::AudioFocusType::GainTransientMayDuck);
  ASSERT_EQ(1, GetTransientMaybeDuckCount());

  web_contents.reset();
  ASSERT_EQ(0, GetTransientMaybeDuckCount());
}

TEST_F(AudioFocusManagerTest, WebContentsDestroyed_StopsDucking) {
  std::unique_ptr<WebContents> web_contents_1(CreateWebContents());
  MediaSession* media_session_1 = MediaSession::Get(web_contents_1.get());

  std::unique_ptr<WebContents> web_contents_2(CreateWebContents());
  MediaSession* media_session_2 = MediaSession::Get(web_contents_2.get());

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_1, AudioFocusManager::AudioFocusType::Gain);
  ASSERT_EQ(kDefaultVolumeMultiplier, GetVolumeMultiplier(media_session_1));

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_2, AudioFocusManager::AudioFocusType::GainTransientMayDuck);
  ASSERT_EQ(kDuckingVolumeMultiplier, GetVolumeMultiplier(media_session_1));

  web_contents_2.reset();
  ASSERT_EQ(kDefaultVolumeMultiplier, GetVolumeMultiplier(media_session_1));
}

}  // namespace content
