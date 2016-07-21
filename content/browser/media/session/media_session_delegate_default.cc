// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_delegate.h"

#include "base/command_line.h"
#include "content/browser/media/session/audio_focus_manager.h"
#include "media/base/media_switches.h"

namespace content {

using AudioFocusType = AudioFocusManager::AudioFocusType;

namespace {

// MediaSessionDelegateDefault is the default implementation of
// MediaSessionDelegate which only handles audio focus between WebContents.
class MediaSessionDelegateDefault : public MediaSessionDelegate {
 public:
  explicit MediaSessionDelegateDefault(MediaSession* media_session);

  // MediaSessionDelegate implementation.
  bool RequestAudioFocus(MediaSession::Type type) override;
  void AbandonAudioFocus() override;

 private:
  // Weak pointer because |this| is owned by |media_session_|.
  MediaSession* media_session_;
};

}  // anonymous namespace

MediaSessionDelegateDefault::MediaSessionDelegateDefault(
    MediaSession* media_session)
    : media_session_(media_session) {
}

bool MediaSessionDelegateDefault::RequestAudioFocus(MediaSession::Type type) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDefaultMediaSession)) {
    return true;
  }

  AudioFocusManager::GetInstance()->RequestAudioFocus(
      media_session_, type == MediaSession::Type::Content
                          ? AudioFocusType::Gain
                          : AudioFocusType::GainTransientMayDuck);
  return true;
}

void MediaSessionDelegateDefault::AbandonAudioFocus() {
  AudioFocusManager::GetInstance()->AbandonAudioFocus(media_session_);
}

// static
std::unique_ptr<MediaSessionDelegate> MediaSessionDelegate::Create(
    MediaSession* media_session) {
  return std::unique_ptr<MediaSessionDelegate>(
      new MediaSessionDelegateDefault(media_session));
}

}  // namespace content
