// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/message_center_controller.h"

#include "ui/message_center/message_center.h"

namespace ash {

MessageCenterController::MessageCenterController()
    : fullscreen_notification_blocker_(message_center::MessageCenter::Get()),
      inactive_user_notification_blocker_(message_center::MessageCenter::Get()),
      login_notification_blocker_(message_center::MessageCenter::Get()) {}

MessageCenterController::~MessageCenterController() {}

}  // namespace ash
