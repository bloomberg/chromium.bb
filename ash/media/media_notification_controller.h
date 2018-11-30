// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_H_
#define ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "ui/message_center/message_center.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace ash {

class MediaNotificationView;

// MediaNotificationController will show/hide a media notification when a media
// session is active. This notification will show metadata and playback
// controls.
class ASH_EXPORT MediaNotificationController
    : public media_session::mojom::AudioFocusObserver,
      public media_session::mojom::MediaSessionObserver {
 public:
  explicit MediaNotificationController(service_manager::Connector* connector);
  ~MediaNotificationController() override;

  // media_session::mojom::AudioFocusObserver:
  void OnFocusGained(media_session::mojom::MediaSessionInfoPtr session_info,
                     media_session::mojom::AudioFocusType type) override {}
  void OnFocusLost(
      media_session::mojom::MediaSessionInfoPtr session_info) override {}
  void OnActiveSessionChanged(
      media_session::mojom::AudioFocusRequestStatePtr session) override;

  // media_session::mojom::MediaSessionObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override;

  void FlushForTesting();
  void SetMediaControllerForTesting(
      media_session::mojom::MediaControllerPtr controller) {
    media_controller_ptr_ = std::move(controller);
  }

  void SetView(MediaNotificationView* view);

 private:
  // Weak reference to the view of the currently shown media notification.
  MediaNotificationView* view_ = nullptr;

  void OnNotificationClicked(base::Optional<int> button_id);

  media_session::mojom::MediaControllerPtr media_controller_ptr_;

  media_session::mojom::MediaSessionInfoPtr session_info_;

  mojo::Binding<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_binding_{this};

  mojo::Binding<media_session::mojom::MediaSessionObserver>
      media_session_observer_binding_{this};

  base::WeakPtrFactory<MediaNotificationController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationController);
};

}  // namespace ash

#endif  // ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_H_
