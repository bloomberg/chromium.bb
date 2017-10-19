// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_CHROMEOS_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_CHROMEOS_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notifier_controller.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "ui/message_center/notifier_settings.h"

class NotifierController;

// The class to bridge between the settings UI of notifiers and the preference
// storage.
class MessageCenterSettingsControllerChromeOs
    : public message_center::NotifierSettingsProvider,
      public NotifierController::Observer {
 public:
  MessageCenterSettingsControllerChromeOs();
  ~MessageCenterSettingsControllerChromeOs() override;

  // message_center::NotifierSettingsProvider
  void AddObserver(message_center::NotifierSettingsObserver* observer) override;
  void RemoveObserver(
      message_center::NotifierSettingsObserver* observer) override;
  void GetNotifierList(std::vector<std::unique_ptr<message_center::Notifier>>*
                           notifiers) override;
  void SetNotifierEnabled(const message_center::NotifierId& notifier_id,
                          bool enabled) override;
  void OnNotifierSettingsClosing() override;
  bool NotifierHasAdvancedSettings(
      const message_center::NotifierId& notifier_id) const override;
  void OnNotifierAdvancedSettingsRequested(
      const message_center::NotifierId& notifier_id,
      const std::string* notification_id) override;

 private:
  // NotifierController::Observer
  void OnIconImageUpdated(const message_center::NotifierId&,
                          const gfx::Image&) override;
  void OnNotifierEnabledChanged(const message_center::NotifierId&,
                                bool) override;

  Profile* GetProfile() const;

  // The views displaying notifier settings.
  base::ObserverList<message_center::NotifierSettingsObserver> observers_;

  // Notifier source for each notifier type.
  std::map<message_center::NotifierId::NotifierType,
           std::unique_ptr<NotifierController>>
      sources_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterSettingsControllerChromeOs);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_CHROMEOS_H_
