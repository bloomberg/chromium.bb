// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYSTEM_COMPONENT_NOTIFIER_CONTROLLER_CHROMEOS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYSTEM_COMPONENT_NOTIFIER_CONTROLLER_CHROMEOS_H_

#include "chrome/browser/notifications/notifier_controller.h"

// TODO(estade): remove this class. See crbug.com/766846
class SystemComponentNotifierControllerChromeOS : public NotifierController {
 public:
  explicit SystemComponentNotifierControllerChromeOS(Observer* observer);
  std::vector<std::unique_ptr<message_center::Notifier>> GetNotifierList(
      Profile* profile) override;
  void SetNotifierEnabled(Profile* profile,
                          const message_center::NotifierId& notifier_id,
                          bool enabled) override;

 private:
  // Lifetime of parent must be longer than the source.
  Observer* observer_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYSTEM_COMPONENT_NOTIFIER_CONTROLLER_CHROMEOS_H_
