// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_ARC_APPLICATION_NOTIFIER_SOURCE_CHROMEOS_H_
#define CHROME_BROWSER_NOTIFICATIONS_ARC_APPLICATION_NOTIFIER_SOURCE_CHROMEOS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/notifications/notifier_source.h"

namespace content {
class BrowserContext;
}

namespace message_center {
struct Notifier;
}

namespace arc {

// TODO(hirono): Observe enabled flag change and notify it to message center.
class ArcApplicationNotifierSourceChromeOS : public NotifierSource {
 public:
  explicit ArcApplicationNotifierSourceChromeOS(Observer* observer);

  // TODO(hirono): Rewrite the function with new API to fetch package list.
  std::vector<std::unique_ptr<message_center::Notifier>> GetNotifierList(
      Profile* profile) override;
  void SetNotifierEnabled(Profile* profile,
                          const message_center::Notifier& notifier,
                          bool enabled) override;
  message_center::NotifierId::NotifierType GetNotifierType() override;

 private:
  Observer* observer_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_NOTIFICATIONS_ARC_APPLICATION_NOTIFIER_SOURCE_CHROMEOS_H_
