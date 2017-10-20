// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_
#define ASH_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_

#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "ash/system/web_notification/fullscreen_notification_blocker.h"
#include "ash/system/web_notification/inactive_user_notification_blocker.h"
#include "ash/system/web_notification/login_state_notification_blocker.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

// This class manages the ash message center. For now it just houses
// Ash-specific notification blockers. In the future, when the MessageCenter
// lives in the Ash process, it will also manage adding and removing
// notifications sent from clients (like Chrome).
class MessageCenterController : public mojom::AshMessageCenterController {
 public:
  MessageCenterController();
  ~MessageCenterController() override;

  void BindRequest(mojom::AshMessageCenterControllerRequest request);

  // AshMessageCenterController:
  void SetClient(
      mojom::AshMessageCenterClientAssociatedPtrInfo client) override;
  void ShowClientNotification(
      const message_center::Notification& notification) override;

  InactiveUserNotificationBlocker*
  inactive_user_notification_blocker_for_testing() {
    return &inactive_user_notification_blocker_;
  }

 private:
  FullscreenNotificationBlocker fullscreen_notification_blocker_;
  InactiveUserNotificationBlocker inactive_user_notification_blocker_;
  LoginStateNotificationBlocker login_notification_blocker_;

  mojo::Binding<mojom::AshMessageCenterController> binding_;

  mojom::AshMessageCenterClientAssociatedPtr client_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterController);
};

}  // namespace ash

#endif  // ASH_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_
