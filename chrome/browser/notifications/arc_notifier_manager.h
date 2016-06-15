// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_ARC_NOTIFIER_MANAGER_H_
#define CHROME_BROWSER_NOTIFICATIONS_ARC_NOTIFIER_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"

namespace content {
class BrowserContext;
}

namespace message_center {
struct Notifier;
}

namespace arc {

class ArcNotifierManager {
 public:
  ArcNotifierManager() {}

  // TODO(hirono): Rewrite the function with new API to fetch package list.
  std::vector<std::unique_ptr<message_center::Notifier>> GetNotifiers(
      content::BrowserContext* profile);
  void SetNotifierEnabled(const std::string& package, bool enabled);

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcNotifierManager);
};

}  // namespace arc

#endif  // CHROME_BROWSER_NOTIFICATIONS_ARC_NOTIFIER_MANAGER_H_
