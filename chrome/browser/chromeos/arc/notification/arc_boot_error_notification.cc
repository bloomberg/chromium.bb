// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/notification/arc_boot_error_notification.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/message_center/public/cpp/message_center_switches.h"

namespace arc {

namespace {

const char kLowDiskSpaceId[] = "arc_low_disk";
const char kNotifierId[] = "arc_boot_error";
const char kStoragePage[] = "storage";

class LowDiskSpaceErrorNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  explicit LowDiskSpaceErrorNotificationDelegate(
      content::BrowserContext* context)
      : context_(context) {}

  // message_center::NotificationDelegate
  void ButtonClick(int button_index) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    chrome::ShowSettingsSubPageForProfile(Profile::FromBrowserContext(context_),
                                          kStoragePage);
  }

 private:
  ~LowDiskSpaceErrorNotificationDelegate() override = default;

  // Passed from ArcBootErrorNotification, so owned by ProfileManager.
  // Thus, touching this on UI thread while the message loop is running
  // should be safe.
  content::BrowserContext* const context_;
  DISALLOW_COPY_AND_ASSIGN(LowDiskSpaceErrorNotificationDelegate);
};

void ShowLowDiskSpaceErrorNotification(content::BrowserContext* context) {
  // We suppress the low-disk notification when there are multiple users on an
  // enterprise managed device. crbug.com/656788.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->IsEnterpriseManaged() &&
      user_manager::UserManager::Get()->GetUsers().size() > 1) {
    LOG(WARNING) << "ARC booting is aborted due to low disk space, but the "
                 << "notification was suppressed on a managed device.";
    return;
  }
  message_center::ButtonInfo storage_settings(
      l10n_util::GetStringUTF16(IDS_LOW_DISK_NOTIFICATION_BUTTON));
  storage_settings.icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_STORAGE_MANAGER_BUTTON);
  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.push_back(storage_settings);

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT, kNotifierId);
  const AccountId& account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  notifier_id.profile_id = account_id.GetUserEmail();

  std::unique_ptr<message_center::Notification> notification;
  if (message_center::IsNewStyleNotificationEnabled()) {
    notification = message_center::Notification::CreateSystemNotification(
        message_center::NOTIFICATION_TYPE_SIMPLE, kLowDiskSpaceId,
        l10n_util::GetStringUTF16(
            IDS_ARC_CRITICALLY_LOW_DISK_NOTIFICATION_TITLE),
        l10n_util::GetStringUTF16(
            IDS_ARC_CRITICALLY_LOW_DISK_NOTIFICATION_MESSAGE),
        gfx::Image(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_DISK_SPACE_NOTIFICATION_CRITICAL)),
        l10n_util::GetStringUTF16(IDS_ARC_NOTIFICATION_DISPLAY_SOURCE), GURL(),
        notifier_id, optional_fields,
        new LowDiskSpaceErrorNotificationDelegate(context),
        kNotificationStorageFullIcon,
        message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
  } else {
    notification = std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, kLowDiskSpaceId,
        l10n_util::GetStringUTF16(
            IDS_ARC_CRITICALLY_LOW_DISK_NOTIFICATION_TITLE),
        l10n_util::GetStringUTF16(
            IDS_ARC_CRITICALLY_LOW_DISK_NOTIFICATION_MESSAGE),
        gfx::Image(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_DISK_SPACE_NOTIFICATION_CRITICAL)),
        l10n_util::GetStringUTF16(IDS_ARC_NOTIFICATION_DISPLAY_SOURCE), GURL(),
        notifier_id, optional_fields,
        new LowDiskSpaceErrorNotificationDelegate(context));
  }
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

// Singleton factory for ArcBootErrorNotificationFactory.
class ArcBootErrorNotificationFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcBootErrorNotification,
          ArcBootErrorNotificationFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcBootErrorNotificationFactory";

  static ArcBootErrorNotificationFactory* GetInstance() {
    return base::Singleton<ArcBootErrorNotificationFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcBootErrorNotificationFactory>;
  ArcBootErrorNotificationFactory() = default;
  ~ArcBootErrorNotificationFactory() override = default;
};

}  // namespace

// static
ArcBootErrorNotification* ArcBootErrorNotification::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcBootErrorNotificationFactory::GetForBrowserContext(context);
}

ArcBootErrorNotification::ArcBootErrorNotification(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : context_(context) {
  ArcSessionManager::Get()->AddObserver(this);
}

ArcBootErrorNotification::~ArcBootErrorNotification() {
  ArcSessionManager::Get()->RemoveObserver(this);
}

void ArcBootErrorNotification::OnArcSessionStopped(ArcStopReason reason) {
  if (reason == ArcStopReason::LOW_DISK_SPACE)
    ShowLowDiskSpaceErrorNotification(context_);
}

}  // namespace arc
