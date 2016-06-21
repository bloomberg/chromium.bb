// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/arc_application_notifier_source_chromeos.h"

#include <set>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "ui/message_center/notifier_settings.h"

namespace arc {

namespace {
constexpr int kSetNotificationsEnabledMinVersion = 6;
}  // namespace

ArcApplicationNotifierSourceChromeOS::ArcApplicationNotifierSourceChromeOS(
    Observer* observer)
    : observer_(observer) {}

std::vector<std::unique_ptr<message_center::Notifier>>
ArcApplicationNotifierSourceChromeOS::GetNotifierList(Profile* profile) {
  const ArcAppListPrefs* const app_list = ArcAppListPrefs::Get(profile);
  const std::vector<std::string>& app_ids = app_list->GetAppIds();
  std::set<std::string> added_packages;
  std::vector<std::unique_ptr<message_center::Notifier>> results;

  for (const std::string& app_id : app_ids) {
    const auto app = app_list->GetApp(app_id);
    if (!app)
      continue;
    // Handle packages having multiple launcher activities.
    if (added_packages.count(app->package_name))
      continue;

    added_packages.insert(app->package_name);
    message_center::NotifierId notifier_id(
        message_center::NotifierId::ARC_APPLICATION, app->package_name);
    results.emplace_back(
        new message_center::Notifier(notifier_id, base::ASCIIToUTF16(app->name),
                                     app->notifications_enabled));
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

message_center::NotifierId::NotifierType
ArcApplicationNotifierSourceChromeOS::GetNotifierType() {
  return message_center::NotifierId::ARC_APPLICATION;
}

}  // namespace arc
