// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/pending_app_manager.h"

#include <memory>
#include <utility>

namespace web_app {

PendingAppManager::AppInfo::AppInfo(GURL url,
                                    LaunchContainer launch_container,
                                    InstallSource install_source,
                                    bool create_shortcuts)
    : url(std::move(url)),
      launch_container(launch_container),
      install_source(install_source),
      create_shortcuts(create_shortcuts) {}

PendingAppManager::AppInfo::AppInfo(PendingAppManager::AppInfo&& other) =
    default;

PendingAppManager::AppInfo::~AppInfo() = default;

std::unique_ptr<PendingAppManager::AppInfo> PendingAppManager::AppInfo::Clone()
    const {
  std::unique_ptr<AppInfo> other(
      new AppInfo(url, launch_container, install_source, create_shortcuts));
  DCHECK_EQ(*this, *other);
  return other;
}

bool PendingAppManager::AppInfo::operator==(
    const PendingAppManager::AppInfo& other) const {
  return std::tie(url, launch_container, install_source, create_shortcuts) ==
         std::tie(other.url, other.launch_container, other.install_source,
                  other.create_shortcuts);
}

PendingAppManager::PendingAppManager() = default;

PendingAppManager::~PendingAppManager() = default;

std::ostream& operator<<(std::ostream& out,
                         const PendingAppManager::AppInfo& app_info) {
  return out << "url: " << app_info.url << "\n launch_container: "
             << static_cast<int32_t>(app_info.launch_container)
             << "\n install_source: "
             << static_cast<int32_t>(app_info.install_source)
             << "\n create_shortcuts: " << app_info.create_shortcuts;
}

}  // namespace web_app
