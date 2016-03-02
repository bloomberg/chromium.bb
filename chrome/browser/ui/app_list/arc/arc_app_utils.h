// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_UTILS_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_UTILS_H_

#include <string>

namespace content {
class BrowserContext;
}

namespace arc {

bool LaunchApp(content::BrowserContext* context, const std::string& app_id);

}  // namespace arc

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_UTILS_H_
