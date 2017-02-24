// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_service.h>

#include "base/time/time.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_blocker.h"

namespace chromeos {

// Timeout maintenance session after 30 minutes.
constexpr base::TimeDelta kArcKioskMaintenanceSessionTimeout =
    base::TimeDelta::FromMinutes(30);

// Blocks all notifications for ARC Kiosk
class ArcKioskNotificationBlocker : public message_center::NotificationBlocker {
 public:
  ArcKioskNotificationBlocker()
      : message_center::NotificationBlocker(
            message_center::MessageCenter::Get()) {
    NotifyBlockingStateChanged();
  }

  ~ArcKioskNotificationBlocker() override {}

 private:
  bool ShouldShowNotification(
      const message_center::Notification& notification) const override {
    return false;
  }

  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(ArcKioskNotificationBlocker);
};

// static
ArcKioskAppService* ArcKioskAppService::Create(Profile* profile) {
  return new ArcKioskAppService(profile);
}

// static
ArcKioskAppService* ArcKioskAppService::Get(content::BrowserContext* context) {
  return ArcKioskAppServiceFactory::GetForBrowserContext(context);
}

void ArcKioskAppService::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void ArcKioskAppService::Shutdown() {
  ArcAppListPrefs::Get(profile_)->RemoveObserver(this);
  app_manager_->RemoveObserver(this);
}

void ArcKioskAppService::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (!app_id_.empty() && app_id != app_id_)
    return;
  PreconditionsChanged();
}

void ArcKioskAppService::OnAppReadyChanged(const std::string& id, bool ready) {
  if (!app_id_.empty() && id != app_id_)
    return;
  PreconditionsChanged();
}

void ArcKioskAppService::OnPackageListInitialRefreshed() {
  // The app could already be registered.
  PreconditionsChanged();
}

void ArcKioskAppService::OnArcKioskAppsChanged() {
  PreconditionsChanged();
}

void ArcKioskAppService::OnTaskCreated(int32_t task_id,
                                       const std::string& package_name,
                                       const std::string& activity,
                                       const std::string& intent) {
  // Store task id of the app to stop it later when needed.
  if (app_info_ && package_name == app_info_->package_name &&
      activity == app_info_->activity) {
    task_id_ = task_id;
    if (delegate_)
      delegate_->OnAppStarted();
  }
}

void ArcKioskAppService::OnTaskDestroyed(int32_t task_id) {
  if (task_id == task_id_) {
    app_launcher_.reset();
    task_id_ = -1;
    // Trying to restart app if it was somehow closed or crashed
    // as kiosk app should always be running during the session.
    PreconditionsChanged();
  }
}

void ArcKioskAppService::OnMaintenanceSessionCreated() {
  maintenance_session_running_ = true;
  PreconditionsChanged();
  // Safe to bind |this| as timer is auto-cancelled on destruction.
  maintenance_timeout_timer_.Start(
      FROM_HERE, kArcKioskMaintenanceSessionTimeout,
      base::Bind(&ArcKioskAppService::OnMaintenanceSessionFinished,
                 base::Unretained(this)));
}

void ArcKioskAppService::OnMaintenanceSessionFinished() {
  if (!maintenance_timeout_timer_.IsRunning())
    VLOG(1) << "Maintenance session timeout";
  maintenance_timeout_timer_.Stop();
  maintenance_session_running_ = false;
  PreconditionsChanged();
}

void ArcKioskAppService::OnAppWindowLaunched() {
  if (delegate_)
    delegate_->OnAppWindowLaunched();
}

ArcKioskAppService::ArcKioskAppService(Profile* profile) : profile_(profile) {
  ArcAppListPrefs::Get(profile_)->AddObserver(this);
  app_manager_ = ArcKioskAppManager::Get();
  DCHECK(app_manager_);
  app_manager_->AddObserver(this);
  pref_change_registrar_.reset(new PrefChangeRegistrar());
  pref_change_registrar_->Init(profile_->GetPrefs());
  notification_blocker_.reset(new ArcKioskNotificationBlocker());
  PreconditionsChanged();
}

ArcKioskAppService::~ArcKioskAppService() {
  maintenance_timeout_timer_.Stop();
}

void ArcKioskAppService::PreconditionsChanged() {
  app_id_ = GetAppId();
  if (app_id_.empty())
    return;
  app_info_ = ArcAppListPrefs::Get(profile_)->GetApp(app_id_);
  if (app_info_ && app_info_->ready && !maintenance_session_running_) {
    if (!app_launcher_)
      app_launcher_ = base::MakeUnique<ArcKioskAppLauncher>(
          profile_, ArcAppListPrefs::Get(profile_), app_id_, this);
  } else if (task_id_ != -1) {
    arc::CloseTask(task_id_);
  }
}

std::string ArcKioskAppService::GetAppId() {
  AccountId account_id = multi_user_util::GetAccountIdFromProfile(profile_);
  const ArcKioskAppManager::ArcKioskApp* app =
      app_manager_->GetAppByAccountId(account_id);
  if (!app)
    return std::string();
  std::unordered_set<std::string> app_ids =
      ArcAppListPrefs::Get(profile_)->GetAppsForPackage(
          app->app_info().package_name());
  if (app_ids.empty())
    return std::string();
  // TODO(poromov@): Choose appropriate app id to launch. See
  // http://crbug.com/665904
  return std::string(*app_ids.begin());
}

}  // namespace chromeos
