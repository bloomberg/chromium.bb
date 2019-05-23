// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_MEDIA_NOTIFICATION_CONTAINER_H_
#define ASH_MEDIA_MEDIA_NOTIFICATION_CONTAINER_H_

#include "ash/ash_export.h"

namespace ash {

// MediaNotificationContainer is an interface for containers of
// MediaNotificationView components to receive events from the
// MediaNotificationView.
class ASH_EXPORT MediaNotificationContainer {
 public:
  // Called when MediaNotificationView's expanded state changes.
  virtual void OnExpanded(bool expanded) = 0;
};

}  // namespace ash

#endif  // ASH_MEDIA_MEDIA_NOTIFICATION_CONTAINER_H_
