// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/extension_notifier_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/notifications.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/message_center/notifier_settings.h"

ExtensionNotifierController::ExtensionNotifierController(Observer* observer)
    : observer_(observer) {}

ExtensionNotifierController::~ExtensionNotifierController() {}

std::vector<std::unique_ptr<message_center::Notifier>>
ExtensionNotifierController::GetNotifierList(Profile* profile) {
  std::vector<std::unique_ptr<message_center::Notifier>> notifiers;
  const extensions::ExtensionSet& extension_set =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  // The extension icon size has to be 32x32 at least to load bigger icons if
  // the icon doesn't exist for the specified size, and in that case it falls
  // back to the default icon. The fetched icon will be resized in the
  // settings dialog. See chrome/browser/extensions/extension_icon_image.cc
  // and crbug.com/222931
  app_icon_loader_.reset(new extensions::ChromeAppIconLoader(
      profile, extension_misc::EXTENSION_ICON_SMALL, this));
  for (extensions::ExtensionSet::const_iterator iter = extension_set.begin();
       iter != extension_set.end(); ++iter) {
    const extensions::Extension* extension = iter->get();
    if (!extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kNotifications)) {
      continue;
    }

    // Hosted apps are no longer able to affect the notifications permission
    // state for web notifications.
    // TODO(dewittj): Deprecate the 'notifications' permission for hosted
    // apps.
    if (extension->is_hosted_app())
      continue;

    message_center::NotifierId notifier_id(
        message_center::NotifierId::APPLICATION, extension->id());
    NotifierStateTracker* const notifier_state_tracker =
        NotifierStateTrackerFactory::GetForProfile(profile);
    notifiers.emplace_back(new message_center::Notifier(
        notifier_id, base::UTF8ToUTF16(extension->name()),
        notifier_state_tracker->IsNotifierEnabled(notifier_id)));
    app_icon_loader_->FetchImage(extension->id());
  }

  return notifiers;
}

void ExtensionNotifierController::SetNotifierEnabled(
    Profile* profile,
    const message_center::NotifierId& notifier_id,
    bool enabled) {
  NotifierStateTrackerFactory::GetForProfile(profile)->SetNotifierEnabled(
      notifier_id, enabled);
  observer_->OnNotifierEnabledChanged(notifier_id, enabled);
}

bool ExtensionNotifierController::HasAdvancedSettings(
    Profile* profile,
    const message_center::NotifierId& notifier_id) const {
  const std::string& extension_id = notifier_id.id;

  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);

  return event_router->ExtensionHasEventListener(
      extension_id, extensions::api::notifications::OnShowSettings::kEventName);
}

void ExtensionNotifierController::OnNotifierAdvancedSettingsRequested(
    Profile* profile,
    const message_center::NotifierId& notifier_id,
    const std::string* notification_id) {
  const std::string& extension_id = notifier_id.id;

  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);
  std::unique_ptr<base::ListValue> args(new base::ListValue());

  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::NOTIFICATIONS_ON_SHOW_SETTINGS,
      extensions::api::notifications::OnShowSettings::kEventName,
      std::move(args)));
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

void ExtensionNotifierController::OnAppImageUpdated(
    const std::string& id,
    const gfx::ImageSkia& image) {
  observer_->OnIconImageUpdated(
      message_center::NotifierId(message_center::NotifierId::APPLICATION, id),
      gfx::Image(image));
}
