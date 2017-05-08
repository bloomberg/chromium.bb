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
#include "ui/app_list/app_list_constants.h"
#include "ui/base/layout.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
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
      activity == app_info_->activity && intent == app_info_->intent_uri) {
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

void ArcKioskAppService::OnIconUpdated(ArcAppIcon* icon) {
  DCHECK_EQ(icon, app_icon_.get());
  if (icon->image_skia().isNull()) {
    app_icon_.release();
    return;
  }
  app_manager_->UpdateNameAndIcon(app_id_, app_info_->name,
                                  app_icon_->image_skia());
}

ArcKioskAppService::ArcKioskAppService(Profile* profile) : profile_(profile) {
  ArcAppListPrefs::Get(profile_)->AddObserver(this);
  app_manager_ = ArcKioskAppManager::Get();
  DCHECK(app_manager_);
  app_manager_->AddObserver(this);
  pref_change_registrar_.reset(new PrefChangeRegistrar());
  pref_change_registrar_->Init(profile_->GetPrefs());
  // Kiosk app can be started only when policy compliance is reported.
  pref_change_registrar_->Add(
      prefs::kArcPolicyComplianceReported,
      base::Bind(&ArcKioskAppService::PreconditionsChanged,
                 base::Unretained(this)));
  notification_blocker_.reset(new ArcKioskNotificationBlocker());
  PreconditionsChanged();
}

ArcKioskAppService::~ArcKioskAppService() {
  maintenance_timeout_timer_.Stop();
}

void ArcKioskAppService::RequestNameAndIconUpdate() {
  // Request only once when app_icon_ is not initialized.
  if (!app_info_ || !app_info_->ready || app_icon_)
    return;
  app_icon_ = base::MakeUnique<ArcAppIcon>(profile_, app_id_,
                                           app_list::kGridIconDimension, this);
  app_icon_->image_skia().GetRepresentation(ui::GetSupportedScaleFactor(
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor()));
  // Apply default image now and in case icon is updated then OnIconUpdated()
  // will be called additionally.
  OnIconUpdated(app_icon_.get());
}

void ArcKioskAppService::PreconditionsChanged() {
  VLOG(2) << "Preconditions for kiosk app changed";
  app_id_ = GetAppId();
  if (app_id_.empty()) {
    VLOG(2) << "Kiosk app is not available";
    return;
  }
  app_info_ = ArcAppListPrefs::Get(profile_)->GetApp(app_id_);
  VLOG_IF(2, app_info_ && app_info_->ready) << "Kiosk app is ready";
  VLOG(2) << "Maintenance session is "
          << (maintenance_session_running_ ? "running" : "not running");
  VLOG(2) << "Policy compliance is "
          << (profile_->GetPrefs()->GetBoolean(
                  prefs::kArcPolicyComplianceReported)
                  ? "reported"
                  : "not yet reported");
  VLOG(2) << "Kiosk app with id: " << app_id_ << " is "
          << (app_launcher_ ? "already launched" : "not yet launched");
  if (app_info_ && app_info_->ready && !maintenance_session_running_ &&
      profile_->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported)) {
    if (!app_launcher_) {
      VLOG(2) << "Starting kiosk app";
      app_launcher_ = base::MakeUnique<ArcKioskAppLauncher>(
          profile_, ArcAppListPrefs::Get(profile_), app_id_, this);
    }
  } else if (task_id_ != -1) {
    VLOG(2) << "Kiosk app should be closed";
    arc::CloseTask(task_id_);
  }
  RequestNameAndIconUpdate();
}

std::string ArcKioskAppService::GetAppId() {
  AccountId account_id = multi_user_util::GetAccountIdFromProfile(profile_);
  const ArcKioskAppData* app = app_manager_->GetAppByAccountId(account_id);
  if (!app)
    return std::string();
  std::unordered_set<std::string> app_ids =
      ArcAppListPrefs::Get(profile_)->GetAppsForPackage(app->package_name());
  if (app_ids.empty())
    return std::string();
  // If |activity| and |intent| are not specified, return any app from the
  // package.
  if (app->activity().empty() && app->intent().empty())
    return *app_ids.begin();
  // Check that the app is registered for given package.
  return app_ids.count(app->app_id()) ? app->app_id() : std::string();
}

}  // namespace chromeos
