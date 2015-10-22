// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_info.h"

ShortcutInfo::ShortcutInfo(const GURL& shortcut_url)
    : url(shortcut_url),
      display(blink::WebDisplayModeBrowser),
      orientation(blink::WebScreenOrientationLockDefault),
      source(SOURCE_ADD_TO_HOMESCREEN),
      theme_color(content::Manifest::kInvalidOrMissingColor),
      background_color(content::Manifest::kInvalidOrMissingColor),
      is_icon_generated(false) {
}

ShortcutInfo::~ShortcutInfo() {
}

void ShortcutInfo::UpdateFromManifest(const content::Manifest& manifest) {
  if (!manifest.short_name.is_null())
    short_name = manifest.short_name.string();
  if (!manifest.name.is_null())
    name = manifest.name.string();
  if (manifest.short_name.is_null() != manifest.name.is_null()) {
    if (manifest.short_name.is_null())
      short_name = name;
    else
      name = short_name;
  }
  user_title = short_name;

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    url = manifest.start_url;

  // Set the display based on the manifest value, if any.
  if (manifest.display != blink::WebDisplayModeUndefined)
    display = manifest.display;

  // 'fullscreen' and 'minimal-ui' are not yet supported, fallback to the right
  // mode in those cases.
  if (manifest.display == blink::WebDisplayModeFullscreen)
    display = blink::WebDisplayModeStandalone;
  if (manifest.display == blink::WebDisplayModeMinimalUi)
    display = blink::WebDisplayModeBrowser;

  // Set the orientation based on the manifest value, if any.
  if (manifest.orientation != blink::WebScreenOrientationLockDefault) {
    // Ignore the orientation if the display mode is different from
    // 'standalone'.
    // TODO(mlamouri): send a message to the developer console about this.
    if (display == blink::WebDisplayModeStandalone)
      orientation = manifest.orientation;
  }

  // Set the theme color based on the manifest value, if any.
  if (manifest.theme_color != content::Manifest::kInvalidOrMissingColor)
    theme_color = manifest.theme_color;

  // Set the background color based on the manifest value, if any.
  if (manifest.background_color != content::Manifest::kInvalidOrMissingColor)
    background_color = manifest.background_color;
}

void ShortcutInfo::UpdateSource(const Source new_source) {
  source = new_source;
}
