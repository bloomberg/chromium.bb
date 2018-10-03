// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_delegate.h"

#include "content/browser/media/session/audio_focus_manager.h"
#include "content/browser/media/session/media_session_impl.h"
#include "services/media_session/public/cpp/switches.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace content {

using media_session::mojom::AudioFocusType;

namespace {

// AudioFocusDelegateDefault is the default implementation of
// AudioFocusDelegate which only handles audio focus between WebContents.
class AudioFocusDelegateDefault : public AudioFocusDelegate {
 public:
  explicit AudioFocusDelegateDefault(MediaSessionImpl* media_session);
  ~AudioFocusDelegateDefault() override;

  // AudioFocusDelegate implementation.
  AudioFocusResult RequestAudioFocus(AudioFocusType audio_focus_type) override;
  void AbandonAudioFocus() override;
  base::Optional<AudioFocusType> GetCurrentFocusType() const override;

 private:
  // Holds the current audio focus request id for |media_session_|.
  base::Optional<AudioFocusManager::RequestId> request_id_;

  // Weak pointer because |this| is owned by |media_session_|.
  MediaSessionImpl* media_session_;

  // The last requested AudioFocusType by the associated |media_session_|.
  base::Optional<AudioFocusType> audio_focus_type_if_disabled_;
};

}  // anonymous namespace

AudioFocusDelegateDefault::AudioFocusDelegateDefault(
    MediaSessionImpl* media_session)
    : media_session_(media_session) {}

AudioFocusDelegateDefault::~AudioFocusDelegateDefault() = default;

AudioFocusDelegate::AudioFocusResult
AudioFocusDelegateDefault::RequestAudioFocus(AudioFocusType audio_focus_type) {
  audio_focus_type_if_disabled_ = audio_focus_type;

  if (!media_session::IsAudioFocusEnabled())
    return AudioFocusDelegate::AudioFocusResult::kSuccess;

  media_session::mojom::MediaSessionInfoPtr session_info =
      media_session_->GetMediaSessionInfoSync();

  // Create a mojo interface pointer to our media session. This will allow the
  // AudioFocusManager to interact with the media session across processes.
  media_session::mojom::MediaSessionPtr media_session;
  media_session_->BindToMojoRequest(mojo::MakeRequest(&media_session));

  AudioFocusManager::RequestResponse response =
      AudioFocusManager::GetInstance()->RequestAudioFocus(
          std::move(media_session), std::move(session_info), audio_focus_type,
          request_id_);

  request_id_ = response.first;

  return response.second ? AudioFocusDelegate::AudioFocusResult::kSuccess
                         : AudioFocusDelegate::AudioFocusResult::kFailed;
}

void AudioFocusDelegateDefault::AbandonAudioFocus() {
  audio_focus_type_if_disabled_.reset();

  if (!request_id_.has_value())
    return;

  AudioFocusManager::GetInstance()->AbandonAudioFocus(request_id_.value());
  request_id_.reset();
}

base::Optional<AudioFocusType> AudioFocusDelegateDefault::GetCurrentFocusType()
    const {
  if (!media_session::IsAudioFocusEnabled())
    return audio_focus_type_if_disabled_;

  if (!request_id_.has_value())
    return base::Optional<AudioFocusType>();

  return AudioFocusManager::GetInstance()->GetFocusTypeForSession(
      request_id_.value());
}

// static
std::unique_ptr<AudioFocusDelegate> AudioFocusDelegate::Create(
    MediaSessionImpl* media_session) {
  return std::unique_ptr<AudioFocusDelegate>(
      new AudioFocusDelegateDefault(media_session));
}

}  // namespace content
