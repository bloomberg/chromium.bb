// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager.h"

#include "base/logging.h"
#include "base/optional.h"

namespace badging {

BadgeManager::BadgeManager() = default;

BadgeManager::~BadgeManager() = default;

void BadgeManager::UpdateBadge(const extensions::ExtensionId& extension_id,
                               base::Optional<int> contents) {
  auto it = badged_apps_.find(extension_id);
  if (it != badged_apps_.end() && it->second == contents)
    return;

  badged_apps_[extension_id] = contents;

  if (!delegate_)
    return;

  delegate_->OnBadgeSet(extension_id, contents);
}

void BadgeManager::ClearBadge(const extensions::ExtensionId& extension_id) {
  auto removed = badged_apps_.erase(extension_id);
  if (!removed || !delegate_)
    return;

  delegate_->OnBadgeCleared(extension_id);
}

void BadgeManager::SetDelegate(BadgeManagerDelegate* badge_manager_delegate) {
  DCHECK(!delegate_);
  delegate_ = badge_manager_delegate;
}

}  // namespace badging
