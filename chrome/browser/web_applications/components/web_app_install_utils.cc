// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_install_utils.h"

#include "chrome/common/web_application_info.h"
#include "third_party/blink/public/common/manifest/manifest.h"

namespace web_app {

void UpdateWebAppInfoFromManifest(const blink::Manifest& manifest,
                                  WebApplicationInfo* web_app_info,
                                  ForInstallableSite for_installable_site) {
  if (!manifest.short_name.is_null())
    web_app_info->title = manifest.short_name.string();

  // Give the full length name priority.
  if (!manifest.name.is_null())
    web_app_info->title = manifest.name.string();

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    web_app_info->app_url = manifest.start_url;

  if (for_installable_site == ForInstallableSite::kYes) {
    // If there is no scope present, use 'start_url' without the filename as the
    // scope. This does not match the spec but it matches what we do on Android.
    // See: https://github.com/w3c/manifest/issues/550
    if (!manifest.scope.is_empty())
      web_app_info->scope = manifest.scope;
    else if (manifest.start_url.is_valid())
      web_app_info->scope = manifest.start_url.Resolve(".");
  }

  if (manifest.theme_color)
    web_app_info->theme_color = *manifest.theme_color;

  // If any icons are specified in the manifest, they take precedence over any
  // we picked up from the web_app stuff.
  if (!manifest.icons.empty()) {
    web_app_info->icons.clear();
    for (const auto& icon : manifest.icons) {
      // TODO(benwells): Take the declared icon density and sizes into account.
      WebApplicationInfo::IconInfo info;
      info.url = icon.src;
      web_app_info->icons.push_back(info);
    }
  }
}

}  // namespace web_app
