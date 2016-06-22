// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/arc_application_notifier_source_chromeos.h"

#include <set>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "ui/base/layout.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/message_center/notifier_settings.h"

namespace arc {

namespace {
constexpr int kSetNotificationsEnabledMinVersion = 6;
constexpr int kArcAppIconSizeInDp = 48;
}  // namespace

ArcApplicationNotifierSourceChromeOS::ArcApplicationNotifierSourceChromeOS(
    NotifierSource::Observer* observer)
    : observer_(observer) {}

ArcApplicationNotifierSourceChromeOS::~ArcApplicationNotifierSourceChromeOS() {}

std::vector<std::unique_ptr<message_center::Notifier>>
ArcApplicationNotifierSourceChromeOS::GetNotifierList(Profile* profile) {
  const ArcAppListPrefs* const app_list = ArcAppListPrefs::Get(profile);
  const std::vector<std::string>& app_ids = app_list->GetAppIds();
  std::set<std::string> added_packages;
  std::vector<std::unique_ptr<message_center::Notifier>> results;

  icons_.clear();
  for (const std::string& app_id : app_ids) {
    const auto app = app_list->GetApp(app_id);
    if (!app)
      continue;
    // Handle packages having multiple launcher activities.
    if (added_packages.count(app->package_name))
      continue;

    // Load icons for notifier.
    std::unique_ptr<ArcAppIcon> icon(
        new ArcAppIcon(profile, app_id,
                       // ARC icon is available only for 48x48 dips.
                       kArcAppIconSizeInDp,
                       // The life time of icon must shorter than |this|.
                       this));
    icon->LoadForScaleFactor(
        ui::GetSupportedScaleFactor(display::Screen::GetScreen()
                                        ->GetPrimaryDisplay()
                                        .device_scale_factor()));

    // Add notifiers.
    added_packages.insert(app->package_name);
    message_center::NotifierId notifier_id(
        message_center::NotifierId::ARC_APPLICATION, app_id);
    std::unique_ptr<message_center::Notifier> notifier(
        new message_center::Notifier(notifier_id, base::UTF8ToUTF16(app->name),
                                     app->notifications_enabled));
    notifier->icon = icon->image();
    icons_.push_back(std::move(icon));
    results.push_back(std::move(notifier));
  }

  return results;
}

void ArcApplicationNotifierSourceChromeOS::SetNotifierEnabled(
    Profile* profile,
    const message_center::Notifier& notifier,
    bool enabled) {
  auto* const service = arc::ArcBridgeService::Get();
  if (service) {
    if (service->app_version() >= kSetNotificationsEnabledMinVersion) {
      service->app_instance()->SetNotificationsEnabled(notifier.notifier_id.id,
                                                       enabled);
      observer_->OnNotifierEnabledChanged(notifier.notifier_id, enabled);
    }
  }
}

void ArcApplicationNotifierSourceChromeOS::OnNotifierSettingsClosing() {
  icons_.clear();
}

message_center::NotifierId::NotifierType
ArcApplicationNotifierSourceChromeOS::GetNotifierType() {
  return message_center::NotifierId::ARC_APPLICATION;
}

void ArcApplicationNotifierSourceChromeOS::OnIconUpdated(ArcAppIcon* icon) {
  observer_->OnIconImageUpdated(
      message_center::NotifierId(message_center::NotifierId::ARC_APPLICATION,
                                 icon->app_id()),
      icon->image());
}

}  // namespace arc
