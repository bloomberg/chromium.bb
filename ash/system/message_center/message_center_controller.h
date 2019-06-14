// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "ash/system/message_center/arc/arc_notification_manager.h"
#include "ash/system/message_center/fullscreen_notification_blocker.h"
#include "ash/system/message_center/inactive_user_notification_blocker.h"
#include "ash/system/message_center/session_state_notification_blocker.h"
#include "base/macros.h"
#include "components/arc/common/notifications.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class PrefRegistrySimple;

namespace ash {

// This class manages the ash message center and allows clients (like Chrome) to
// add and remove notifications.
class ASH_EXPORT MessageCenterController
    : public mojom::AshMessageCenterController {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  MessageCenterController();
  ~MessageCenterController() override;

  void BindRequest(mojom::AshMessageCenterControllerRequest request);

  // mojom::AshMessageCenterController:
  void SetClient(
      mojom::AshMessageCenterClientAssociatedPtrInfo client) override;
  void SetArcNotificationsInstance(
      arc::mojom::NotificationsInstancePtr arc_notification_instance) override;

  // Handles get app id calls from ArcNotificationManager.
  using GetAppIdByPackageNameCallback =
      base::OnceCallback<void(const std::string& app_id)>;
  void GetArcAppIdByPackageName(const std::string& package_name,
                                GetAppIdByPackageNameCallback callback);

  InactiveUserNotificationBlocker*
  inactive_user_notification_blocker_for_testing() {
    return inactive_user_notification_blocker_.get();
  }

 private:
  std::unique_ptr<FullscreenNotificationBlocker>
      fullscreen_notification_blocker_;
  std::unique_ptr<InactiveUserNotificationBlocker>
      inactive_user_notification_blocker_;
  std::unique_ptr<SessionStateNotificationBlocker>
      session_state_notification_blocker_;
  std::unique_ptr<message_center::NotificationBlocker> all_popup_blocker_;

  mojo::BindingSet<mojom::AshMessageCenterController> binding_set_;

  mojom::AshMessageCenterClientAssociatedPtr client_;

  std::unique_ptr<ArcNotificationManager> arc_notification_manager_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_
