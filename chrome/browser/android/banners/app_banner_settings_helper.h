// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_SETTINGS_HELPER_H_
#define CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_SETTINGS_HELPER_H_

#include <string>

#include "base/macros.h"

namespace content {
class WebContents;
}  // namesapce content

class GURL;

// Utility class for reading and updating ContentSettings for app banners.
class AppBannerSettingsHelper {
 public:
  // Checks if a URL is allowed to show a banner for the given package.
  static bool IsAllowed(content::WebContents* web_contents,
                        const GURL& origin_url,
                        const std::string& package_name);

  // Blocks a URL from showing a banner for the given package.
  static void Block(content::WebContents* web_contents,
                    const GURL& origin_url,
                    const std::string& package_name);

 private:
  DISALLOW_COPY_AND_ASSIGN(AppBannerSettingsHelper);
};

#endif  // CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_SETTINGS_HELPER_H_
