// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SHORTCUT_INFO_H_
#define CHROME_BROWSER_ANDROID_SHORTCUT_INFO_H_

#include "base/strings/string16.h"
#include "content/public/common/manifest.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"
#include "url/gurl.h"

// Information needed to create a shortcut via ShortcutHelper.
struct ShortcutInfo {
  ShortcutInfo();
  explicit ShortcutInfo(const GURL& shortcut_url);

  // Updates the info based on the given |manifest|.
  void UpdateFromManifest(const content::Manifest& manifest);

  GURL url;
  base::string16 title;
  content::Manifest::DisplayMode display;
  blink::WebScreenOrientationLockType orientation;
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_INFO_H_
