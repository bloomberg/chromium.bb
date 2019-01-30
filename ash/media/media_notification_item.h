// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_MEDIA_NOTIFICATION_ITEM_H_
#define ASH_MEDIA_MEDIA_NOTIFICATION_ITEM_H_

#include <set>
#include <string>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace ash {

class MediaNotificationView;

// MediaNotificationItem manages hiding/showing a media notification and
// updating the metadata for a single media session.
class ASH_EXPORT MediaNotificationItem
    : public media_session::mojom::MediaControllerObserver {
 public:
  MediaNotificationItem(const std::string& id,
                        media_session::mojom::MediaControllerPtr controller,
                        media_session::mojom::MediaSessionInfoPtr session_info);
  ~MediaNotificationItem() override;

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override;

  void SetView(MediaNotificationView* view);

  void FlushForTesting();

  void SetMediaControllerForTesting(
      media_session::mojom::MediaControllerPtr controller) {
    media_controller_ptr_ = std::move(controller);
  }

 private:
  void MaybeHideOrShowNotification();

  void HideNotification();

  // Weak reference to the view of the currently shown media notification.
  MediaNotificationView* view_ = nullptr;

  void OnNotificationClicked(base::Optional<int> button_id);

  // The id is the |request_id| of the media session and is guaranteed to be
  // globally unique. It is also used as the id of the notification for this
  // media session.
  const std::string id_;

  media_session::mojom::MediaControllerPtr media_controller_ptr_;

  media_session::mojom::MediaSessionInfoPtr session_info_;

  media_session::MediaMetadata session_metadata_;

  std::set<media_session::mojom::MediaSessionAction> session_actions_;

  mojo::Binding<media_session::mojom::MediaControllerObserver>
      observer_binding_{this};

  base::WeakPtrFactory<MediaNotificationItem> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationItem);
};

}  // namespace ash

#endif  // ASH_MEDIA_MEDIA_NOTIFICATION_ITEM_H_
