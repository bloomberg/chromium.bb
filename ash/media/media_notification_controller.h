// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_H_
#define ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"

namespace ash {

// MediaNotificationController does the actual hiding and showing of the media
// notification.
class ASH_EXPORT MediaNotificationController {
 public:
  // Shows/hides a notification with the given request id. Called by
  // MediaNotificationItem when the notification should be shown/hidden.
  virtual void ShowNotification(const std::string& id) = 0;
  virtual void HideNotification(const std::string& id) = 0;
};

}  // namespace ash

#endif  // ASH_MEDIA_MEDIA_NOTIFICATION_CONTROLLER_H_
