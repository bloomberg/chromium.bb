// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_launch_id.h"

#include "base/logging.h"

namespace ash {

AppLaunchId::AppLaunchId(const std::string& app_id,
                         const std::string& launch_id)
    : app_id(app_id), launch_id(launch_id) {
  DCHECK(launch_id.empty() || !app_id.empty()) << "launch ids require app ids.";
}

AppLaunchId::AppLaunchId(const std::string& app_id) : app_id(app_id) {}

AppLaunchId::AppLaunchId() = default;

AppLaunchId::~AppLaunchId() = default;

AppLaunchId::AppLaunchId(const AppLaunchId& other) = default;

AppLaunchId::AppLaunchId(AppLaunchId&& other) = default;

AppLaunchId& AppLaunchId::operator=(const AppLaunchId& other) = default;

bool AppLaunchId::operator==(const AppLaunchId& other) const {
  return app_id == other.app_id && launch_id == other.launch_id;
}

bool AppLaunchId::operator!=(const AppLaunchId& other) const {
  return !(*this == other);
}

bool AppLaunchId::operator<(const AppLaunchId& other) const {
  return app_id < other.app_id ||
         (app_id == other.app_id && launch_id < other.launch_id);
}

bool AppLaunchId::IsNull() const {
  return app_id.empty() && launch_id.empty();
}

}  // namespace ash
