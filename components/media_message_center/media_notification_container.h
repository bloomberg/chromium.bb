// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONTAINER_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONTAINER_H_

#include "base/component_export.h"

namespace media_message_center {

// MediaNotificationContainer is an interface for containers of
// MediaNotificationView components to receive events from the
// MediaNotificationView.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationContainer {
 public:
  // Called when MediaNotificationView's expanded state changes.
  virtual void OnExpanded(bool expanded) = 0;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONTAINER_H_
