// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/pending_app_manager.h"

#include <memory>
#include <utility>

namespace web_app {

PendingAppManager::AppInfo::AppInfo(GURL url, LaunchContainer launch_container)
    : url(std::move(url)), launch_container(launch_container) {}

PendingAppManager::AppInfo::AppInfo(PendingAppManager::AppInfo&& other) =
    default;

PendingAppManager::AppInfo::~AppInfo() = default;

std::unique_ptr<PendingAppManager::AppInfo> PendingAppManager::AppInfo::Clone()
    const {
  auto other = std::make_unique<AppInfo>(url, launch_container);
  DCHECK_EQ(*this, *other);
  return other;
}

bool PendingAppManager::AppInfo::operator==(
    const PendingAppManager::AppInfo& other) const {
  return std::tie(url, launch_container) ==
         std::tie(other.url, other.launch_container);
}

PendingAppManager::PendingAppManager() = default;

PendingAppManager::~PendingAppManager() = default;

std::ostream& operator<<(std::ostream& out,
                         const PendingAppManager::AppInfo& app_info) {
  return out << "url: " << app_info.url << "\n launch_container: "
             << static_cast<int32_t>(app_info.launch_container);
}

}  // namespace web_app
