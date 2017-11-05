// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SHORTCUT_INFO_H_
#define CHROME_BROWSER_ANDROID_SHORTCUT_INFO_H_

#include <stdint.h>

#include <vector>

#include "base/strings/string16.h"
#include "content/public/common/manifest.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "url/gurl.h"

// Information needed to create a shortcut via ShortcutHelper.
struct ShortcutInfo {

  // This enum is used to back a UMA histogram, and must be treated as
  // append-only.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: ShortcutSource
  enum Source {
    SOURCE_UNKNOWN = 0,
    SOURCE_ADD_TO_HOMESCREEN_DEPRECATED = 1,

    // Used for legacy PWAs added via the banner.
    SOURCE_APP_BANNER = 2,
    SOURCE_BOOKMARK_NAVIGATOR_WIDGET = 3,
    SOURCE_BOOKMARK_SHORTCUT_WIDGET = 4,

    // Used for legacy and WebAPKs launched from a notification.
    SOURCE_NOTIFICATION = 5,

    // Used for WebAPKs added via the menu item.
    SOURCE_ADD_TO_HOMESCREEN_PWA = 6,

    // Used for legacy PWAs added via the menu item.
    SOURCE_ADD_TO_HOMESCREEN_STANDALONE = 7,

    // Used for bookmark-type shortcuts that launch the tabbed browser.
    SOURCE_ADD_TO_HOMESCREEN_SHORTCUT = 8,

    // Used for WebAPKs launched via an external intent.
    SOURCE_EXTERNAL_INTENT = 9,

    // Used for WebAPK PWAs added via the banner.
    SOURCE_APP_BANNER_WEBAPK = 10,

    // Used for WebAPK PWAs whose install source info was lost.
    SOURCE_WEBAPK_UNKNOWN = 11,

    // Used for Trusted Web Activities launched from third party Android apps.
    SOURCE_TRUSTED_WEB_ACTIVITY = 12,

    // Used for WebAPK intents received as a result of share events.
    SOURCE_WEBAPK_SHARE_TARGET = 13,

    SOURCE_COUNT = 14
  };

  explicit ShortcutInfo(const GURL& shortcut_url);
  ShortcutInfo(const ShortcutInfo& other);
  ~ShortcutInfo();

  // Updates the info based on the given |manifest|.
  void UpdateFromManifest(const content::Manifest& manifest);

  // Updates the source of the shortcut.
  void UpdateSource(const Source source);

  GURL manifest_url;
  GURL url;
  GURL scope;
  base::string16 user_title;
  base::string16 name;
  base::string16 short_name;
  blink::WebDisplayMode display;
  blink::WebScreenOrientationLockType orientation;
  Source source;
  int64_t theme_color;
  int64_t background_color;
  GURL splash_screen_url;
  int ideal_splash_image_size_in_px;
  int minimum_splash_image_size_in_px;
  GURL splash_image_url;
  GURL best_primary_icon_url;
  GURL best_badge_icon_url;
  std::vector<std::string> icon_urls;
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_INFO_H_
