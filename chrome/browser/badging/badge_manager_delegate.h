// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_H_

#include <string>

#include "base/optional.h"

namespace badging {

// BadgeManagerDelegate is responsible for dispatching badge events that should
// be handled and reflected in the UI.
class BadgeManagerDelegate {
 public:
  virtual ~BadgeManagerDelegate() {}

  // Called when an app's badge has changed.
  virtual void OnBadgeSet(const std::string& app_id,
                          base::Optional<int> contents) = 0;

  // Called when a app's badge has been cleared.
  virtual void OnBadgeCleared(const std::string& app_id) = 0;
};

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_H_
