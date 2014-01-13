// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/per_app_settings_service.h"

PerAppSettingsService::PerAppSettingsService() {
}

PerAppSettingsService::~PerAppSettingsService() {
}

void PerAppSettingsService::SetDesktopLastLaunchedFrom(
    const std::string& app_id, chrome::HostDesktopType host_desktop) {
  default_desktops_[app_id] = host_desktop;
}

chrome::HostDesktopType PerAppSettingsService::GetDesktopLastLaunchedFrom(
    const std::string& app_id) const {
  DesktopMap::const_iterator it = default_desktops_.find(app_id);
  if (it == default_desktops_.end())
    NOTREACHED();
  return it->second;
}

bool PerAppSettingsService::HasDesktopLastLaunchedFrom(
    const std::string& app_id) const {
  return default_desktops_.find(app_id) != default_desktops_.end();
}
