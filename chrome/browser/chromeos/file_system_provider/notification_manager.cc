// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/notification_manager.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/account_id/account_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Extension icon size for the notification.
const int kIconSize = 48;

// Forwards notification events to the notification manager.
class ProviderNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  // Passing a raw pointer is safe here, since the life of each notification is
  // shorter than life of the |notification_manager|.
  explicit ProviderNotificationDelegate(
      NotificationManager* notification_manager)
      : notification_manager_(notification_manager) {}

  void ButtonClick(int button_index) override {
    notification_manager_->OnButtonClick(button_index);
  }

  void Close(bool by_user) override { notification_manager_->OnClose(); }

 private:
  ~ProviderNotificationDelegate() override {}
  NotificationManager* notification_manager_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(ProviderNotificationDelegate);
};

}  // namespace

NotificationManager::NotificationManager(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info)
    : profile_(profile),
      file_system_info_(file_system_info),
      icon_loader_(
          new extensions::ChromeAppIconLoader(profile, kIconSize, this)) {}

NotificationManager::~NotificationManager() {
  if (callbacks_.size()) {
    g_browser_process->message_center()->RemoveNotification(
        file_system_info_.mount_path().value(), false /* by_user */);
  }
}

void NotificationManager::ShowUnresponsiveNotification(
    int id,
    const NotificationCallback& callback) {
  callbacks_[id] = callback;

  if (callbacks_.size() == 1) {
    g_browser_process->message_center()->AddNotification(CreateNotification());
  } else {
    g_browser_process->message_center()->UpdateNotification(
        file_system_info_.mount_path().value(), CreateNotification());
  }
}

void NotificationManager::HideUnresponsiveNotification(int id) {
  callbacks_.erase(id);

  if (callbacks_.size()) {
    g_browser_process->message_center()->UpdateNotification(
        file_system_info_.mount_path().value(), CreateNotification());
    return;
  }

  g_browser_process->message_center()->RemoveNotification(
      file_system_info_.mount_path().value(), false /* by_user */);
}

void NotificationManager::OnButtonClick(int button_index) {
  OnNotificationResult(ABORT);
}

void NotificationManager::OnClose() {
  OnNotificationResult(CONTINUE);
}

void NotificationManager::OnAppImageUpdated(const std::string& id,
                                            const gfx::ImageSkia& image) {
  extension_icon_.reset(new gfx::Image(image));
  g_browser_process->message_center()->UpdateNotification(
      file_system_info_.mount_path().value(), CreateNotification());
}

std::unique_ptr<message_center::Notification>
NotificationManager::CreateNotification() {
  if (!extension_icon_.get())
    icon_loader_->FetchImage(file_system_info_.extension_id());

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_PROVIDER_UNRESPONSIVE_ABORT_BUTTON)));

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      file_system_info_.mount_path().value());
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile_).GetUserEmail();

  std::unique_ptr<message_center::Notification> notification(
      new message_center::Notification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          file_system_info_.mount_path().value(),
          base::UTF8ToUTF16(file_system_info_.display_name()),
          l10n_util::GetStringUTF16(
              callbacks_.size() == 1
                  ? IDS_FILE_SYSTEM_PROVIDER_UNRESPONSIVE_WARNING
                  : IDS_FILE_SYSTEM_PROVIDER_MANY_UNRESPONSIVE_WARNING),
          extension_icon_.get() ? *extension_icon_.get() : gfx::Image(),
          base::string16(),  // display_source
          GURL(), notifier_id, rich_notification_data,
          new ProviderNotificationDelegate(this)));

  notification->SetSystemPriority();
  return notification;
}

void NotificationManager::OnNotificationResult(NotificationResult result) {
  CallbackMap::iterator it = callbacks_.begin();
  while (it != callbacks_.end()) {
    CallbackMap::iterator current_it = it++;
    NotificationCallback callback = current_it->second;
    callbacks_.erase(current_it);
    callback.Run(result);
  }
}

}  // namespace file_system_provider
}  // namespace chromeos
