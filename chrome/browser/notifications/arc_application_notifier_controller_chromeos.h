// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_ARC_APPLICATION_NOTIFIER_CONTROLLER_CHROMEOS_H_
#define CHROME_BROWSER_NOTIFICATIONS_ARC_APPLICATION_NOTIFIER_CONTROLLER_CHROMEOS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/notifications/notifier_controller.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

class Profile;

namespace arc {

// TODO(hirono): Observe enabled flag change and notify it to message center.
class ArcApplicationNotifierControllerChromeOS
    : public NotifierController,
      public ArcAppIcon::Observer,
      public ArcAppListPrefs::Observer {
 public:
  explicit ArcApplicationNotifierControllerChromeOS(
      NotifierController::Observer* observer);

  ~ArcApplicationNotifierControllerChromeOS() override;

  // TODO(hirono): Rewrite the function with new API to fetch package list.
  std::vector<ash::mojom::NotifierUiDataPtr> GetNotifierList(
      Profile* profile) override;
  void SetNotifierEnabled(Profile* profile,
                          const message_center::NotifierId& notifier_id,
                          bool enabled) override;

 private:
  // Overridden from ArcAppIcon::Observer.
  void OnIconUpdated(ArcAppIcon* icon) override;
  void StopObserving();

  // Overriden from ArcAppListPrefs::Observer.
  void OnNotificationsEnabledChanged(const std::string& package_name,
                                     bool enabled) override;

  NotifierController::Observer* observer_;
  std::vector<std::unique_ptr<ArcAppIcon>> icons_;
  std::map<std::string, std::string> package_to_app_ids_;
  Profile* last_profile_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_NOTIFICATIONS_ARC_APPLICATION_NOTIFIER_CONTROLLER_CHROMEOS_H_
