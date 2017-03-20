// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_launch_id.h"

#include "base/logging.h"

namespace ash {

AppLaunchId::AppLaunchId(const std::string& app_id,
                         const std::string& launch_id)
    : app_id_(app_id), launch_id_(launch_id) {
  DCHECK(!app_id.empty());
}

AppLaunchId::AppLaunchId(const std::string& app_id) : app_id_(app_id) {
  DCHECK(!app_id.empty());
}

AppLaunchId::AppLaunchId() {}

AppLaunchId::~AppLaunchId() {}

}  // namespace ash
