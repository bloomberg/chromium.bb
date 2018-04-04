// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/chrome_ash_message_center_client.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "base/i18n/string_compare.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/arc_application_notifier_controller_chromeos.h"
#include "chrome/browser/notifications/extension_notifier_controller.h"
#include "chrome/browser/notifications/web_page_notifier_controller.h"
#include "components/user_manager/user_manager.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using ash::mojom::NotifierUiDataPtr;
using message_center::NotifierId;

namespace {

// All notifier actions are performed on the notifiers for the currently active
// profile, so this just returns the active profile.
Profile* GetProfileForNotifiers() {
  return chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetActiveUser());
}

class NotifierComparator {
 public:
  explicit NotifierComparator(icu::Collator* collator) : collator_(collator) {}

  bool operator()(const NotifierUiDataPtr& n1, const NotifierUiDataPtr& n2) {
    if (n1->notifier_id.type != n2->notifier_id.type)
      return n1->notifier_id.type < n2->notifier_id.type;

    if (collator_) {
      return base::i18n::CompareString16WithCollator(*collator_, n1->name,
                                                     n2->name) == UCOL_LESS;
    }
    return n1->name < n2->name;
  }

 private:
  icu::Collator* collator_;
};

}  // namespace

ChromeAshMessageCenterClient::ChromeAshMessageCenterClient(
    NotificationPlatformBridgeDelegate* delegate)
    : delegate_(delegate), binding_(this) {
  // May be null in unit tests.
  auto* connection = content::ServiceManagerConnection::GetForProcess();
  if (connection) {
    connection->GetConnector()->BindInterface(ash::mojom::kServiceName,
                                              &controller_);

    // Register this object as the client interface implementation.
    ash::mojom::AshMessageCenterClientAssociatedPtrInfo ptr_info;
    binding_.Bind(mojo::MakeRequest(&ptr_info));
    controller_->SetClient(std::move(ptr_info));
  }

  sources_.insert(std::make_pair(NotifierId::APPLICATION,
                                 std::unique_ptr<NotifierController>(
                                     new ExtensionNotifierController(this))));

  sources_.insert(std::make_pair(NotifierId::WEB_PAGE,
                                 std::unique_ptr<NotifierController>(
                                     new WebPageNotifierController(this))));

  sources_.insert(std::make_pair(
      NotifierId::ARC_APPLICATION,
      std::unique_ptr<NotifierController>(
          new arc::ArcApplicationNotifierControllerChromeOS(this))));
}

ChromeAshMessageCenterClient::~ChromeAshMessageCenterClient() {}

// The unused variables here will not be a part of the future
// NotificationPlatformBridge interface.
void ChromeAshMessageCenterClient::Display(
    NotificationHandler::Type /*notification_type*/,
    const std::string& /*profile_id*/,
    bool /*is_incognito*/,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  controller_->ShowClientNotification(notification);
}

// The unused variable here will not be a part of the future
// NotificationPlatformBridge interface.
void ChromeAshMessageCenterClient::Close(const std::string& /*profile_id*/,
                                         const std::string& notification_id) {
  controller_->CloseClientNotification(notification_id);
}

// The unused variables here will not be a part of the future
// NotificationPlatformBridge interface.
void ChromeAshMessageCenterClient::GetDisplayed(
    const std::string& /*profile_id*/,
    bool /*incognito*/,
    GetDisplayedNotificationsCallback callback) const {
  // Right now, this is only used to get web notifications that were created by
  // and have outlived a previous browser process. Ash itself doesn't outlive
  // the browser process, so there's no need to implement.
  std::move(callback).Run(std::make_unique<std::set<std::string>>(), false);
}

void ChromeAshMessageCenterClient::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  std::move(callback).Run(true);
}

void ChromeAshMessageCenterClient::HandleNotificationClosed(
    const std::string& id,
    bool by_user) {
  delegate_->HandleNotificationClosed(id, by_user);
}

void ChromeAshMessageCenterClient::HandleNotificationClicked(
    const std::string& id) {
  delegate_->HandleNotificationClicked(id);
}

void ChromeAshMessageCenterClient::HandleNotificationButtonClicked(
    const std::string& id,
    int button_index,
    const base::Optional<base::string16>& reply) {
  delegate_->HandleNotificationButtonClicked(id, button_index, reply);
}

void ChromeAshMessageCenterClient::HandleNotificationSettingsButtonClicked(
    const std::string& id) {
  delegate_->HandleNotificationSettingsButtonClicked(id);
}

void ChromeAshMessageCenterClient::DisableNotification(const std::string& id) {
  delegate_->DisableNotification(id);
}

void ChromeAshMessageCenterClient::SetNotifierEnabled(
    const NotifierId& notifier_id,
    bool enabled) {
  sources_[notifier_id.type]->SetNotifierEnabled(GetProfileForNotifiers(),
                                                 notifier_id, enabled);
}

void ChromeAshMessageCenterClient::GetNotifierList(
    GetNotifierListCallback callback) {
  std::vector<ash::mojom::NotifierUiDataPtr> notifiers;
  for (auto& source : sources_) {
    auto source_notifiers =
        source.second->GetNotifierList(GetProfileForNotifiers());
    for (auto& notifier : source_notifiers) {
      notifiers.push_back(std::move(notifier));
    }
  }

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  NotifierComparator comparator(U_SUCCESS(error) ? collator.get() : nullptr);
  std::sort(notifiers.begin(), notifiers.end(), comparator);

  std::move(callback).Run(std::move(notifiers));
}

void ChromeAshMessageCenterClient::OnIconImageUpdated(
    const NotifierId& notifier_id,
    const gfx::ImageSkia& image) {
  // |controller_| may be null in unit tests.
  if (!image.isNull() && controller_)
    controller_->UpdateNotifierIcon(notifier_id, image);
}

void ChromeAshMessageCenterClient::OnNotifierEnabledChanged(
    const NotifierId& notifier_id,
    bool enabled) {
  // May be null in unit tests.
  if (controller_)
    controller_->NotifierEnabledChanged(notifier_id, enabled);
}
