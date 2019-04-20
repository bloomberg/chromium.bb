// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_
#define CHROMEOS_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "libassistant/shared/public/media_manager.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace assistant_client {
struct MediaStatus;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

class AssistantManagerServiceImpl;

// MediaSession manages the media session and audio focus for Assistant.
// MediaSession allows clients to observe its changes via MediaSessionObserver,
// and allows clients to resume/suspend/stop the managed players.
class AssistantMediaSession : public media_session::mojom::MediaSession {
 public:
  enum class State { ACTIVE, SUSPENDED, INACTIVE };

  explicit AssistantMediaSession(
      service_manager::Connector* connector,
      AssistantManagerServiceImpl* assistant_manager);
  ~AssistantMediaSession() override;

  // media_session.mojom.MediaSession overrides:
  void Suspend(SuspendType suspend_type) override;
  void Resume(SuspendType suspend_type) override;
  void StartDucking() override {}
  void StopDucking() override {}
  void GetMediaSessionInfo(GetMediaSessionInfoCallback callback) override;
  void GetDebugInfo(GetDebugInfoCallback callback) override;
  void AddObserver(
      media_session::mojom::MediaSessionObserverPtr observer) override;
  void PreviousTrack() override {}
  void NextTrack() override {}
  void NotifyMediaSessionMetadataChanged(
      const assistant_client::MediaStatus& status);
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

  base::WeakPtr<AssistantMediaSession> GetWeakPtr();

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

  // Returns if the session is currently suspended.
  bool IsSuspended() const;

  // The current metadata associated with the current media session.
  media_session::MediaMetadata metadata_;

  AssistantManagerServiceImpl* const assistant_manager_service_;
  service_manager::Connector* connector_;

  // Binding for Mojo pointer to |this| held by AudioFocusManager.
  mojo::Binding<media_session::mojom::MediaSession> binding_;

  assistant_client::TrackType current_track_;

  mojo::InterfacePtrSet<media_session::mojom::MediaSessionObserver> observers_;

  // Holds a pointer to the MediaSessionService.
  media_session::mojom::AudioFocusManagerPtr audio_focus_ptr_;

  // If the media session has acquired audio focus then this will contain a
  // pointer to that requests AudioFocusRequestClient.
  media_session::mojom::AudioFocusRequestClientPtr request_client_ptr_;

  // The last updated |MediaSessionInfo| that was sent to |observers_|.
  media_session::mojom::MediaSessionInfoPtr session_info_;

  State audio_focus_state_ = State::INACTIVE;

  base::WeakPtrFactory<AssistantMediaSession> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantMediaSession);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_
