// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_info.h"

ShortcutInfo::ShortcutInfo(const GURL& shortcut_url)
    : url(shortcut_url),
      display(blink::kWebDisplayModeBrowser),
      orientation(blink::kWebScreenOrientationLockDefault),
      source(SOURCE_ADD_TO_HOMESCREEN_SHORTCUT),
      theme_color(content::Manifest::kInvalidOrMissingColor),
      background_color(content::Manifest::kInvalidOrMissingColor),
      ideal_splash_image_size_in_px(0),
      minimum_splash_image_size_in_px(0) {}

ShortcutInfo::ShortcutInfo(const ShortcutInfo& other) = default;

ShortcutInfo::~ShortcutInfo() {
}

void ShortcutInfo::UpdateFromManifest(const content::Manifest& manifest) {
  if (!manifest.short_name.string().empty() ||
      !manifest.name.string().empty()) {
    short_name = manifest.short_name.string();
    name = manifest.name.string();
    if (short_name.empty())
      short_name = name;
    else if (name.empty())
      name = short_name;
  }
  user_title = short_name;

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    url = manifest.start_url;

  if (manifest.scope.is_valid())
    scope = manifest.scope;

  // Set the display based on the manifest value, if any.
  if (manifest.display != blink::kWebDisplayModeUndefined)
    display = manifest.display;

  // 'minimal-ui' is not yet supported (see crbug.com/604390). Otherwise, set
  // the source to be standalone if appropriate.
  if (manifest.display == blink::kWebDisplayModeMinimalUi) {
    display = blink::kWebDisplayModeBrowser;
  } else if (display == blink::kWebDisplayModeStandalone ||
             display == blink::kWebDisplayModeFullscreen) {
    source = SOURCE_ADD_TO_HOMESCREEN_STANDALONE;
  }

  // Set the orientation based on the manifest value, if any.
  if (manifest.orientation != blink::kWebScreenOrientationLockDefault) {
    // Ignore the orientation if the display mode is different from
    // 'standalone' or 'fullscreen'.
    // TODO(mlamouri): send a message to the developer console about this.
    if (display == blink::kWebDisplayModeStandalone ||
        display == blink::kWebDisplayModeFullscreen) {
      orientation = manifest.orientation;
    }
  }

  // Set the theme color based on the manifest value, if any.
  if (manifest.theme_color != content::Manifest::kInvalidOrMissingColor)
    theme_color = manifest.theme_color;

  // Set the background color based on the manifest value, if any.
  if (manifest.background_color != content::Manifest::kInvalidOrMissingColor)
    background_color = manifest.background_color;

  // Set the icon urls based on the icons in the manifest, if any.
  icon_urls.clear();
  for (const content::Manifest::Icon& icon : manifest.icons)
    icon_urls.push_back(icon.src.spec());
}

void ShortcutInfo::UpdateSource(const Source new_source) {
  source = new_source;
}
