// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_UTIL_H_

// Utility functions for launching an app.

#include <string>

#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace apps_util {

void Launch(const std::string& app_id,
            int32_t event_flags,
            apps::mojom::LaunchSource launch_source,
            int64_t display_id);

}  // namespace apps_util

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_LAUNCH_UTIL_H_
