// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_
#define CHROMEOS_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {
namespace assistant {

// MediaSession manages the media session and audio focus for Assistant.
// MediaSession allows clients to observe its changes via MediaSessionObserver,
// and allows clients to resume/suspend/stop the managed players.
class AssistantMediaSession : public media_session::mojom::MediaSession {
 public:
  enum class State { ACTIVE, INACTIVE };

  explicit AssistantMediaSession(service_manager::Connector* connector);
  ~AssistantMediaSession() override;

  // media_session.mojom.MediaSession overrides:
  void Suspend(SuspendType suspend_type) override {}
  void Resume(SuspendType suspend_type) override {}
  void StartDucking() override {}
  void StopDucking() override {}
  void GetMediaSessionInfo(GetMediaSessionInfoCallback callback) override;
  void GetDebugInfo(GetDebugInfoCallback callback) override;
  void AddObserver(
      media_session::mojom::MediaSessionObserverPtr observer) override {}
  void PreviousTrack() override {}
  void NextTrack() override {}
  void SkipAd() override {}
  void Seek(base::TimeDelta seek_time) override {}
  void Stop(SuspendType suspend_type) override {}
  void GetMediaImageBitmap(const media_session::MediaImage& image,
                           int minimum_size_px,
                           int desired_size_px,
                           GetMediaImageBitmapCallback callback) override {}

  // Requests/abandons audio focus to the AudioFocusManager.
  void RequestAudioFocus(media_session::mojom::AudioFocusType audio_focus_type);
  void AbandonAudioFocusIfNeeded();

 private:
  // Ensures that |audio_focus_ptr_| is connected.
  void EnsureServiceConnection();

  // Called by AudioFocusManager when an async audio focus request is completed.
  void FinishAudioFocusRequest(media_session::mojom::AudioFocusType type);
  void FinishInitialAudioFocusRequest(media_session::mojom::AudioFocusType type,
                                      const base::UnguessableToken& request_id);

  // Returns information about |this|.
  media_session::mojom::MediaSessionInfoPtr GetMediaSessionInfoInternal();

  // Sets |audio_focus_state_| and notifies observers about the state change.
  void SetAudioFocusState(State audio_focus_state);

  // Notifies mojo observers that the MediaSessionInfo has changed.
  void NotifyMediaSessionInfoChanged();

  // Returns if the session is currently active.
  bool IsActive() const;

  service_manager::Connector* connector_;

  // Binding for Mojo pointer to |this| held by AudioFocusManager.
  mojo::Binding<media_session::mojom::MediaSession> binding_;

  // Holds a pointer to the MediaSessionService.
  media_session::mojom::AudioFocusManagerPtr audio_focus_ptr_;

  // If the media session has acquired audio focus then this will contain a
  // pointer to that requests AudioFocusRequestClient.
  media_session::mojom::AudioFocusRequestClientPtr request_client_ptr_;

  State audio_focus_state_ = State::INACTIVE;

  DISALLOW_COPY_AND_ASSIGN(AssistantMediaSession);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_
