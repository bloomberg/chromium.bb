// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYSTEM_COMPONENT_NOTIFIER_SOURCE_CHROMEOS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYSTEM_COMPONENT_NOTIFIER_SOURCE_CHROMEOS_H_

#include "chrome/browser/notifications/notifier_source.h"

class SystemComponentNotifierSourceChromeOS : public NotifierSource {
 public:
  explicit SystemComponentNotifierSourceChromeOS(Observer* observer);
  std::vector<std::unique_ptr<message_center::Notifier>> GetNotifierList(
      Profile* profile) override;
  void SetNotifierEnabled(Profile* profile,
                          const message_center::Notifier& notifier,
                          bool enabled) override;
  message_center::NotifierId::NotifierType GetNotifierType() override;

 private:
  // Lifetime of parent must be longer than the source.
  Observer* observer_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYSTEM_COMPONENT_NOTIFIER_SOURCE_CHROMEOS_H_
