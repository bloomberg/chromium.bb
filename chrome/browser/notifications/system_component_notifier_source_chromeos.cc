// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/system_component_notifier_source_chromeos.h"

#include "ash/common/system/system_notifier.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/strings/grit/ui_strings.h"

SystemComponentNotifierSourceChromeOS::SystemComponentNotifierSourceChromeOS(
    Observer* observer)
    : observer_(observer) {}

std::vector<std::unique_ptr<message_center::Notifier>>
SystemComponentNotifierSourceChromeOS::GetNotifierList(Profile* profile) {
  std::vector<std::unique_ptr<message_center::Notifier>> notifiers;
  NotifierStateTracker* const notifier_state_tracker =
      NotifierStateTrackerFactory::GetForProfile(profile);

  // Screenshot notification feature is only for ChromeOS. See
  // crbug.com/238358
  const base::string16& screenshot_name =
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_NOTIFIER_SCREENSHOT_NAME);
  message_center::NotifierId screenshot_notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT,
      ash::system_notifier::kNotifierScreenshot);
  message_center::Notifier* const screenshot_notifier =
      new message_center::Notifier(
          screenshot_notifier_id, screenshot_name,
          notifier_state_tracker->IsNotifierEnabled(screenshot_notifier_id));
  screenshot_notifier->icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_SCREENSHOT_NOTIFICATION_ICON);
  notifiers.emplace_back(screenshot_notifier);
  return notifiers;
}

void SystemComponentNotifierSourceChromeOS::SetNotifierEnabled(
    Profile* profile,
    const message_center::Notifier& notifier,
    bool enabled) {
  NotifierStateTrackerFactory::GetForProfile(profile)->SetNotifierEnabled(
      notifier.notifier_id, enabled);
  observer_->OnNotifierEnabledChanged(notifier.notifier_id, enabled);
}

message_center::NotifierId::NotifierType
SystemComponentNotifierSourceChromeOS::GetNotifierType() {
  return message_center::NotifierId::SYSTEM_COMPONENT;
}
