// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_UTILS_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_UTILS_H_

#include <string>

#include "base/callback.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class BrowserContext;
}

namespace arc {
using CanHandleResolutionCallback = base::Callback<void(bool)>;
}

namespace arc {

bool LaunchApp(content::BrowserContext* context, const std::string& app_id);

// Tests if the application can use the given target resolution.
// The callback will receive the information once known.
// A false will get returned if the result cannot be determined in which case
// the callback will not be called.
bool CanHandleResolution(content::BrowserContext* context,
    const std::string& app_id,
    const gfx::Rect& rect,
    const CanHandleResolutionCallback callback);

}  // namespace arc

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_UTILS_H_
