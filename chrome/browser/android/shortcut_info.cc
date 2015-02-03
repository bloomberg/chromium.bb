// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_info.h"

ShortcutInfo::ShortcutInfo()
    : display(content::Manifest::DISPLAY_MODE_BROWSER),
      orientation(blink::WebScreenOrientationLockDefault) {
}

ShortcutInfo::ShortcutInfo(const GURL& shortcut_url)
    : url(shortcut_url),
      display(content::Manifest::DISPLAY_MODE_BROWSER),
      orientation(blink::WebScreenOrientationLockDefault) {
}

void ShortcutInfo::UpdateFromManifest(const content::Manifest& manifest) {
  if (!manifest.short_name.is_null())
    title = manifest.short_name.string();
  else if (!manifest.name.is_null())
    title = manifest.name.string();

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    url = manifest.start_url;

  // Set the display based on the manifest value, if any.
  if (manifest.display != content::Manifest::DISPLAY_MODE_UNSPECIFIED)
    display = manifest.display;

  // 'fullscreen' and 'minimal-ui' are not yet supported, fallback to the right
  // mode in those cases.
  if (manifest.display == content::Manifest::DISPLAY_MODE_FULLSCREEN)
    display = content::Manifest::DISPLAY_MODE_STANDALONE;
  if (manifest.display == content::Manifest::DISPLAY_MODE_MINIMAL_UI)
    display = content::Manifest::DISPLAY_MODE_BROWSER;

  // Set the orientation based on the manifest value, if any.
  if (manifest.orientation != blink::WebScreenOrientationLockDefault) {
    // Ignore the orientation if the display mode is different from
    // 'standalone'.
    // TODO(mlamouri): send a message to the developer console about this.
    if (display == content::Manifest::DISPLAY_MODE_STANDALONE)
      orientation = manifest.orientation;
  }
}
