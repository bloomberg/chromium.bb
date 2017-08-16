// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/arc_application_notifier_source_chromeos.h"

#include <set>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/layout.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/message_center/notifier_settings.h"

namespace arc {

namespace {
constexpr int kArcAppIconSizeInDp = 48;
}  // namespace

ArcApplicationNotifierSourceChromeOS::ArcApplicationNotifierSourceChromeOS(
    NotifierSource::Observer* observer)
    : observer_(observer), last_profile_(nullptr) {}

ArcApplicationNotifierSourceChromeOS::~ArcApplicationNotifierSourceChromeOS() {
  StopObserving();
}

std::vector<std::unique_ptr<message_center::Notifier>>
ArcApplicationNotifierSourceChromeOS::GetNotifierList(Profile* profile) {
  package_to_app_ids_.clear();
  icons_.clear();
  StopObserving();

  ArcAppListPrefs* const app_list = ArcAppListPrefs::Get(profile);
  if (!app_list) {
    // It can be null in unit tests.
    return std::vector<std::unique_ptr<message_center::Notifier>>();
  }
  const std::vector<std::string>& app_ids = app_list->GetAppIds();
  std::vector<std::unique_ptr<message_center::Notifier>> results;

  last_profile_ = profile;
  app_list->AddObserver(this);

  for (const std::string& app_id : app_ids) {
    const auto app = app_list->GetApp(app_id);
    // Handle packages having multiple launcher activities.
    if (!app || package_to_app_ids_.count(app->package_name))
      continue;

    const auto package = app_list->GetPackage(app->package_name);
    if (!package || package->system)
      continue;

    // Load icons for notifier.
    std::unique_ptr<ArcAppIcon> icon(
        new ArcAppIcon(profile, app_id,
                       // ARC icon is available only for 48x48 dips.
                       kArcAppIconSizeInDp,
                       // The life time of icon must shorter than |this|.
                       this));
    // Apply icon now to set the default image.
    OnIconUpdated(icon.get());

    // Add notifiers.
    package_to_app_ids_.insert(std::make_pair(app->package_name, app_id));
    message_center::NotifierId notifier_id(
        message_center::NotifierId::ARC_APPLICATION, app_id);
    std::unique_ptr<message_center::Notifier> notifier(
        new message_center::Notifier(notifier_id, base::UTF8ToUTF16(app->name),
                                     app->notifications_enabled));
    notifier->icon = gfx::Image(icon->image_skia());
    icons_.push_back(std::move(icon));
    results.push_back(std::move(notifier));
  }

  return results;
}

void ArcApplicationNotifierSourceChromeOS::SetNotifierEnabled(
    Profile* profile,
    const message_center::Notifier& notifier,
    bool enabled) {
  ArcAppListPrefs* const app_list = ArcAppListPrefs::Get(profile);
  app_list->SetNotificationsEnabled(notifier.notifier_id.id, enabled);
  // OnNotifierEnabledChanged will be invoked via ArcAppListPrefs::Observer.
}

void ArcApplicationNotifierSourceChromeOS::OnNotificationsEnabledChanged(
    const std::string& package_name,
    bool enabled) {
  auto it = package_to_app_ids_.find(package_name);
  if (it == package_to_app_ids_.end())
    return;
  observer_->OnNotifierEnabledChanged(
      message_center::NotifierId(message_center::NotifierId::ARC_APPLICATION,
                                 it->second),
      enabled);
}

void ArcApplicationNotifierSourceChromeOS::OnNotifierSettingsClosing() {
  icons_.clear();
  package_to_app_ids_.clear();
  StopObserving();
}

message_center::NotifierId::NotifierType
ArcApplicationNotifierSourceChromeOS::GetNotifierType() {
  return message_center::NotifierId::ARC_APPLICATION;
}

void ArcApplicationNotifierSourceChromeOS::OnIconUpdated(ArcAppIcon* icon) {
  observer_->OnIconImageUpdated(
      message_center::NotifierId(message_center::NotifierId::ARC_APPLICATION,
                                 icon->app_id()),
      gfx::Image(icon->image_skia()));
}

void ArcApplicationNotifierSourceChromeOS::StopObserving() {
  if (!last_profile_)
    return;
  ArcAppListPrefs* const app_list = ArcAppListPrefs::Get(last_profile_);
  app_list->RemoveObserver(this);
  last_profile_ = nullptr;
}

}  // namespace arc
