// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/install_options.h"

#include <ostream>
#include <tuple>

namespace web_app {

InstallOptions::InstallOptions(const GURL& url,
                               LaunchContainer launch_container,
                               InstallSource install_source)
    : url(url),
      launch_container(launch_container),
      install_source(install_source) {}

InstallOptions::~InstallOptions() = default;

InstallOptions::InstallOptions(const InstallOptions& other) = default;

InstallOptions::InstallOptions(InstallOptions&& other) = default;

InstallOptions& InstallOptions::operator=(const InstallOptions& other) =
    default;

bool InstallOptions::operator==(const InstallOptions& other) const {
  return std::tie(url, launch_container, install_source, create_shortcuts,
                  override_previous_user_uninstall, bypass_service_worker_check,
                  require_manifest, always_update) ==
         std::tie(other.url, other.launch_container, other.install_source,
                  other.create_shortcuts,
                  other.override_previous_user_uninstall,
                  other.bypass_service_worker_check, other.require_manifest,
                  other.always_update);
}

std::ostream& operator<<(std::ostream& out,
                         const InstallOptions& install_options) {
  return out << "url: " << install_options.url << "\n launch_container: "
             << static_cast<int32_t>(install_options.launch_container)
             << "\n install_source: "
             << static_cast<int32_t>(install_options.install_source)
             << "\n create_shortcuts: " << install_options.create_shortcuts
             << "\n override_previous_user_uninstall: "
             << install_options.override_previous_user_uninstall
             << "\n bypass_service_worker_check: "
             << install_options.bypass_service_worker_check
             << "\n require_manifest: " << install_options.require_manifest;
}

}  // namespace web_app
