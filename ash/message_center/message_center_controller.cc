// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/message_center/message_center_controller.h"

#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_delegate.h"

using message_center::MessageCenter;

namespace ash {

namespace {

// This notification delegate passes actions back to the client that asked for
// the notification (Chrome).
class AshClientNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  AshClientNotificationDelegate(const std::string& notification_id,
                                mojom::AshMessageCenterClient* client)
      : notification_id_(notification_id), client_(client) {}

  void Close(bool by_user) override {
    client_->HandleNotificationClosed(notification_id_, by_user);
  }

  void Click() override {
    client_->HandleNotificationClicked(notification_id_);
  }

  void ButtonClick(int button_index) override {
    client_->HandleNotificationButtonClicked(notification_id_, button_index);
  }

 private:
  ~AshClientNotificationDelegate() override = default;

  std::string notification_id_;
  mojom::AshMessageCenterClient* client_;

  DISALLOW_COPY_AND_ASSIGN(AshClientNotificationDelegate);
};

}  // namespace

MessageCenterController::MessageCenterController()
    : fullscreen_notification_blocker_(MessageCenter::Get()),
      inactive_user_notification_blocker_(MessageCenter::Get()),
      login_notification_blocker_(MessageCenter::Get()),
      binding_(this) {}

MessageCenterController::~MessageCenterController() {}

void MessageCenterController::BindRequest(
    mojom::AshMessageCenterControllerRequest request) {
  binding_.Bind(std::move(request));
}

void MessageCenterController::SetClient(
    mojom::AshMessageCenterClientAssociatedPtrInfo client) {
  DCHECK(!client_.is_bound());
  client_.Bind(std::move(client));
}

void MessageCenterController::ShowClientNotification(
    const message_center::Notification& notification) {
  DCHECK(client_.is_bound());
  auto message_center_notification =
      std::make_unique<message_center::Notification>(notification);
  message_center_notification->set_delegate(base::WrapRefCounted(
      new AshClientNotificationDelegate(notification.id(), client_.get())));
  MessageCenter::Get()->AddNotification(std::move(message_center_notification));
}

}  // namespace ash
