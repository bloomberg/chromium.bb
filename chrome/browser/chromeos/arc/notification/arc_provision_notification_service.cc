// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/notification/arc_provision_notification_service.h"

#include <utility>

#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"
#include "url/gurl.h"

namespace arc {

namespace {

constexpr char kManagedProvisionNotificationId[] = "arc_managed_provision";
constexpr char kManagedProvisionNotifierId[] = "arc_managed_provision";

class DelegateImpl : public ArcProvisionNotificationService::Delegate {
 public:
  void ShowManagedProvisionNotification() override;
  void RemoveManagedProvisionNotification() override;
};

void DelegateImpl::ShowManagedProvisionNotification() {
  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      kManagedProvisionNotifierId);
  const AccountId& account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  notifier_id.profile_id = account_id.GetUserEmail();
  message_center::RichNotificationData optional_fields;
  optional_fields.never_timeout = true;

  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kManagedProvisionNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ARC_MANAGED_PROVISION_NOTIFICATION_TITLE),
          l10n_util::GetStringFUTF16(
              IDS_ARC_MANAGED_PROVISION_NOTIFICATION_MESSAGE,
              ui::GetChromeOSDeviceName()),
          gfx::Image(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              IDR_ARC_PLAY_STORE_OPTIN_IN_PROGRESS_NOTIFICATION)),
          l10n_util::GetStringUTF16(IDS_ARC_NOTIFICATION_DISPLAY_SOURCE),
          GURL(), notifier_id, optional_fields, nullptr));
}

void DelegateImpl::RemoveManagedProvisionNotification() {
  message_center::MessageCenter::Get()->RemoveNotification(
      kManagedProvisionNotificationId, false);
}

// Singleton factory for ArcProvisionNotificationService.
class ArcProvisionNotificationServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcProvisionNotificationService,
          ArcProvisionNotificationServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcProvisionNotificationServiceFactory";

  static ArcProvisionNotificationServiceFactory* GetInstance() {
    return base::Singleton<ArcProvisionNotificationServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcProvisionNotificationServiceFactory>;
  ArcProvisionNotificationServiceFactory() = default;
  ~ArcProvisionNotificationServiceFactory() override = default;
};

}  // namespace

ArcProvisionNotificationService::Delegate::Delegate() = default;

ArcProvisionNotificationService::Delegate::~Delegate() = default;

// static
ArcProvisionNotificationService*
ArcProvisionNotificationService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcProvisionNotificationServiceFactory::GetForBrowserContext(context);
}

ArcProvisionNotificationService::ArcProvisionNotificationService(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : ArcProvisionNotificationService(context,
                                      std::make_unique<DelegateImpl>()) {}

ArcProvisionNotificationService::~ArcProvisionNotificationService() {
  // Make sure no notification is left being shown.
  delegate_->RemoveManagedProvisionNotification();

  ArcSessionManager::Get()->RemoveObserver(this);
}

// static
std::unique_ptr<ArcProvisionNotificationService>
ArcProvisionNotificationService::CreateForTesting(
    content::BrowserContext* context,
    std::unique_ptr<Delegate> delegate) {
  return base::WrapUnique<ArcProvisionNotificationService>(
      new ArcProvisionNotificationService(context, std::move(delegate)));
}

ArcProvisionNotificationService::ArcProvisionNotificationService(
    content::BrowserContext* context,
    std::unique_ptr<Delegate> delegate)
    : context_(context), delegate_(std::move(delegate)) {
  ArcSessionManager::Get()->AddObserver(this);
}

void ArcProvisionNotificationService::OnArcPlayStoreEnabledChanged(
    bool enabled) {
  if (!enabled) {
    // Make sure no notification is shown after ARC gets disabled.
    delegate_->RemoveManagedProvisionNotification();
  }
}

void ArcProvisionNotificationService::OnArcOptInManagementCheckStarted() {
  // This observer is notified at an early phase of the opt-in flow, so start
  // showing the notification if the opt-in flow happens silently (due to the
  // managed prefs), or ensure that no notification is shown otherwise.
  const Profile* const profile = Profile::FromBrowserContext(context_);
  if (IsArcPlayStoreEnabledPreferenceManagedForProfile(profile) &&
      AreArcAllOptInPreferencesIgnorableForProfile(profile)) {
    delegate_->ShowManagedProvisionNotification();
  } else {
    delegate_->RemoveManagedProvisionNotification();
  }
}

void ArcProvisionNotificationService::OnArcInitialStart() {
  // The opt-in flow finished successfully, so remove the notification.
  delegate_->RemoveManagedProvisionNotification();
}

void ArcProvisionNotificationService::OnArcSessionStopped(
    ArcStopReason stop_reason) {
  // One of the reasons of ARC being stopped is a failure of the opt-in flow.
  // Therefore remove the notification if it is shown.
  delegate_->RemoveManagedProvisionNotification();
}

void ArcProvisionNotificationService::OnArcErrorShowRequested(
    ArcSupportHost::Error error) {
  // If an error happens during the opt-in flow, then the provision fails, and
  // the notification should be therefore removed if it is shown. Do this here
  // unconditionally as there should be no notification displayed otherwise
  // anyway.
  delegate_->RemoveManagedProvisionNotification();
}

}  // namespace arc
