// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_
#define ASH_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "ash/system/web_notification/fullscreen_notification_blocker.h"
#include "ash/system/web_notification/inactive_user_notification_blocker.h"
#include "ash/system/web_notification/login_state_notification_blocker.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace message_center {
struct NotifierId;
}

namespace ash {

// This class manages the ash message center and allows clients (like Chrome) to
// add and remove notifications.
class ASH_EXPORT MessageCenterController
    : public mojom::AshMessageCenterController {
 public:
  MessageCenterController();
  ~MessageCenterController() override;

  void BindRequest(mojom::AshMessageCenterControllerRequest request);

  // Called when the user has toggled a notifier in the inline settings UI.
  void SetNotifierEnabled(const message_center::NotifierId& notifier_id,
                          bool enabled);

  // Called upon request for more information about a particular notifier.
  void OnNotifierAdvancedSettingsRequested(
      const message_center::NotifierId& notifier_id);

  // mojom::AshMessageCenterController:
  void SetClient(
      mojom::AshMessageCenterClientAssociatedPtrInfo client) override;
  void ShowClientNotification(
      const message_center::Notification& notification) override;
  void UpdateNotifierIcon(const message_center::NotifierId& notifier_id,
                          const gfx::ImageSkia& icon) override;
  void NotifierEnabledChanged(const message_center::NotifierId& notifier_id,
                              bool enabled) override;

  InactiveUserNotificationBlocker*
  inactive_user_notification_blocker_for_testing() {
    return &inactive_user_notification_blocker_;
  }

  // An interface used to listen for changes to notifier settings information,
  // implemented by the view that displays notifier settings.
  class NotifierSettingsListener {
   public:
    // Sets the user-visible and toggle-able list of notifiers.
    virtual void SetNotifierList(
        const std::vector<mojom::NotifierUiDataPtr>& ui_data) = 0;

    // Updates an icon for a notifier previously sent via SetNotifierList.
    virtual void UpdateNotifierIcon(
        const message_center::NotifierId& notifier_id,
        const gfx::ImageSkia& icon) = 0;
  };

  // Sets |notifier_id_| and asks the client for the list of notifiers to
  // display.
  void SetNotifierSettingsListener(NotifierSettingsListener* listener);

 private:
  // Callback for GetNotifierList.
  void OnGotNotifierList(std::vector<mojom::NotifierUiDataPtr> ui_data);

  FullscreenNotificationBlocker fullscreen_notification_blocker_;
  InactiveUserNotificationBlocker inactive_user_notification_blocker_;
  LoginStateNotificationBlocker login_notification_blocker_;

  NotifierSettingsListener* notifier_id_ = nullptr;

  mojo::Binding<mojom::AshMessageCenterController> binding_;

  mojom::AshMessageCenterClientAssociatedPtr client_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterController);
};

}  // namespace ash

#endif  // ASH_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_
