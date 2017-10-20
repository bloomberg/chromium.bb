// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_chromeos.h"

#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

// TODO(estade): remove this function. NotificationPlatformBridge should either
// get Profile* pointers or, longer term, all profile management should be moved
// up a layer to NativeNotificationDisplayService.
Profile* GetProfile(const std::string& profile_id, bool incognito) {
  ProfileManager* manager = g_browser_process->profile_manager();
  Profile* profile =
      manager->GetProfile(manager->user_data_dir().AppendASCII(profile_id));
  return incognito ? profile->GetOffTheRecordProfile() : profile;
}

class NotificationPlatformBridgeChromeOsImpl
    : public NotificationPlatformBridge,
      public ash::mojom::AshMessageCenterClient {
 public:
  explicit NotificationPlatformBridgeChromeOsImpl(
      NotificationPlatformBridgeDelegate* delegate);

  ~NotificationPlatformBridgeChromeOsImpl() override;

  // NotificationPlatformBridge:
  void Display(NotificationCommon::Type notification_type,
               const std::string& notification_id,
               const std::string& profile_id,
               bool is_incognito,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(const std::string& profile_id,
             const std::string& notification_id) override;
  void GetDisplayed(
      const std::string& profile_id,
      bool incognito,
      const GetDisplayedNotificationsCallback& callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;

  // ash::mojom::AshMessageCenterClient:
  void HandleNotificationClosed(const std::string& id, bool by_user) override;
  void HandleNotificationClicked(const std::string& id) override;
  void HandleNotificationButtonClicked(const std::string& id,
                                       int button_index) override;

 private:
  NotificationPlatformBridgeDelegate* delegate_;

  ash::mojom::AshMessageCenterControllerPtr controller_;
  mojo::AssociatedBinding<ash::mojom::AshMessageCenterClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeChromeOsImpl);
};

NotificationPlatformBridgeChromeOsImpl::NotificationPlatformBridgeChromeOsImpl(
    NotificationPlatformBridgeDelegate* delegate)
    : delegate_(delegate), binding_(this) {
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(ash::mojom::kServiceName, &controller_);

  // Register this object as the client interface implementation.
  ash::mojom::AshMessageCenterClientAssociatedPtrInfo ptr_info;
  binding_.Bind(mojo::MakeRequest(&ptr_info));
  controller_->SetClient(std::move(ptr_info));
}

NotificationPlatformBridgeChromeOsImpl::
    ~NotificationPlatformBridgeChromeOsImpl() {}

// The unused variables here will not be a part of the future
// NotificationPlatformBridge interface.
void NotificationPlatformBridgeChromeOsImpl::Display(
    NotificationCommon::Type /*notification_type*/,
    const std::string& /*notification_id*/,
    const std::string& /*profile_id*/,
    bool /*is_incognito*/,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  controller_->ShowClientNotification(notification);
}

// The unused variable here will not be a part of the future
// NotificationPlatformBridge interface.
void NotificationPlatformBridgeChromeOsImpl::Close(
    const std::string& /*profile_id*/,
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

// The unused variables here will not be a part of the future
// NotificationPlatformBridge interface.
void NotificationPlatformBridgeChromeOsImpl::GetDisplayed(
    const std::string& /*profile_id*/,
    bool /*incognito*/,
    const GetDisplayedNotificationsCallback& callback) const {
  NOTIMPLEMENTED();
}

void NotificationPlatformBridgeChromeOsImpl::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  std::move(callback).Run(true);
}

void NotificationPlatformBridgeChromeOsImpl::HandleNotificationClosed(
    const std::string& id,
    bool by_user) {
  delegate_->HandleNotificationClosed(id, by_user);
}

void NotificationPlatformBridgeChromeOsImpl::HandleNotificationClicked(
    const std::string& id) {
  delegate_->HandleNotificationClicked(id);
}

void NotificationPlatformBridgeChromeOsImpl::HandleNotificationButtonClicked(
    const std::string& id,
    int button_index) {
  delegate_->HandleNotificationButtonClicked(id, button_index);
}

}  // namespace

// static
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
  return new NotificationPlatformBridgeChromeOs();
}

NotificationPlatformBridgeChromeOs::NotificationPlatformBridgeChromeOs()
    : impl_(std::make_unique<NotificationPlatformBridgeChromeOsImpl>(this)) {}

NotificationPlatformBridgeChromeOs::~NotificationPlatformBridgeChromeOs() {}

void NotificationPlatformBridgeChromeOs::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const std::string& profile_id,
    bool is_incognito,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  auto active_notification = std::make_unique<ProfileNotification>(
      GetProfile(profile_id, is_incognito), notification, notification_type);
  impl_->Display(NotificationCommon::TYPE_MAX, std::string(), std::string(),
                 false, active_notification->notification(),
                 std::move(metadata));

  std::string profile_notification_id =
      active_notification->notification().id();
  active_notifications_.emplace(profile_notification_id,
                                std::move(active_notification));
}

void NotificationPlatformBridgeChromeOs::Close(
    const std::string& profile_id,
    const std::string& notification_id) {
  impl_->Close(profile_id, notification_id);
}

void NotificationPlatformBridgeChromeOs::GetDisplayed(
    const std::string& profile_id,
    bool incognito,
    const GetDisplayedNotificationsCallback& callback) const {
  impl_->GetDisplayed(profile_id, incognito, callback);
}

void NotificationPlatformBridgeChromeOs::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  impl_->SetReadyCallback(std::move(callback));
}

void NotificationPlatformBridgeChromeOs::HandleNotificationClosed(
    const std::string& id,
    bool by_user) {
  auto iter = active_notifications_.find(id);
  DCHECK(iter != active_notifications_.end());
  ProfileNotification* notification = iter->second.get();
  NotificationDisplayServiceFactory::GetForProfile(notification->profile())
      ->ProcessNotificationOperation(
          NotificationCommon::CLOSE, notification->type(),
          notification->notification().origin_url().possibly_invalid_spec(),
          notification->original_id(), base::nullopt, base::nullopt, by_user);
  active_notifications_.erase(iter);
}

void NotificationPlatformBridgeChromeOs::HandleNotificationClicked(
    const std::string& id) {
  ProfileNotification* notification = GetProfileNotification(id);
  NotificationDisplayServiceFactory::GetForProfile(notification->profile())
      ->ProcessNotificationOperation(
          NotificationCommon::CLICK, notification->type(),
          notification->notification().origin_url().possibly_invalid_spec(),
          notification->original_id(), base::nullopt, base::nullopt,
          base::nullopt);
}

void NotificationPlatformBridgeChromeOs::HandleNotificationButtonClicked(
    const std::string& id,
    int button_index) {
  ProfileNotification* notification = GetProfileNotification(id);
  NotificationDisplayServiceFactory::GetForProfile(notification->profile())
      ->ProcessNotificationOperation(
          NotificationCommon::CLICK, notification->type(),
          notification->notification().origin_url().possibly_invalid_spec(),
          notification->original_id(), button_index, base::nullopt,
          base::nullopt);
}

ProfileNotification* NotificationPlatformBridgeChromeOs::GetProfileNotification(
    const std::string& profile_notification_id) {
  auto iter = active_notifications_.find(profile_notification_id);
  DCHECK(iter != active_notifications_.end());
  return iter->second.get();
}
