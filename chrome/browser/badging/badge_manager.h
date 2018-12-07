// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_MANAGER_H_
#define CHROME_BROWSER_BADGING_BADGE_MANAGER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"

class KeyedService;

namespace badging {

// Maintains a record of badge contents and dispatches badge changes to a
// delegate.
class BadgeManager : public KeyedService {
 public:
  BadgeManager();
  ~BadgeManager() override;

  // Records badge contents for an app and notifies the delegate if the badge
  // contents have changed.
  void UpdateBadge(const extensions::ExtensionId&, base::Optional<int>);

  // Clears badge contents for an app (if existing) and notifies the delegate.
  void ClearBadge(const extensions::ExtensionId&);

  void SetDelegate(BadgeManagerDelegate*);

 private:
  BadgeManagerDelegate* delegate_ = nullptr;

  // Maps extension id to badge contents.
  std::map<extensions::ExtensionId, base::Optional<int>> badged_apps_;

  DISALLOW_COPY_AND_ASSIGN(BadgeManager);
};

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_BADGE_MANAGER_H_
