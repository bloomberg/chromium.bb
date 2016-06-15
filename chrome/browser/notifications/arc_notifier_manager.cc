// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/arc_notifier_manager.h"

#include <set>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "ui/message_center/notifier_settings.h"

namespace arc {

namespace {
constexpr int kSetNotificationsEnabledMinVersion = 6;
}  // namespace

std::vector<std::unique_ptr<message_center::Notifier>>
ArcNotifierManager::GetNotifiers(content::BrowserContext* profile) {
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
    // TODO (hirono): Read Android settings to determine the notification is
    // enabled.
    results.emplace_back(new message_center::Notifier(
        notifier_id, base::ASCIIToUTF16(app->name), true));
  }

  return results;
}

void ArcNotifierManager::SetNotifierEnabled(const std::string& package,
                                            bool enabled) {
  auto* const service = arc::ArcBridgeService::Get();
  if (service) {
    if (service->app_version() >= kSetNotificationsEnabledMinVersion) {
      service->app_instance()->SetNotificationsEnabled(package, enabled);
    }
  }
}

}  // namespace arc
