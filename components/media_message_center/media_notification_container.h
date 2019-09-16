// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONTAINER_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONTAINER_H_

#include <set>

#include "base/component_export.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace media_message_center {

// MediaNotificationContainer is an interface for containers of
// MediaNotificationView components to receive events from the
// MediaNotificationView.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationContainer {
 public:
  // Called when MediaNotificationView's expanded state changes.
  virtual void OnExpanded(bool expanded) = 0;

  // TODO(https://crbug.com/1003847): Use base::flat_set isntead.
  // Called when the set of visible MediaSessionActions changes.
  virtual void OnVisibleActionsChanged(
      const std::set<media_session::mojom::MediaSessionAction>& actions) = 0;

  // Called when the media artwork changes.
  virtual void OnMediaArtworkChanged(const gfx::ImageSkia& image) = 0;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONTAINER_H_
