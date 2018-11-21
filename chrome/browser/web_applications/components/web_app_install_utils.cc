// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_install_utils.h"

#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/common/web_application_info.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

namespace {

void ReplaceWebAppIcons(std::map<int, BitmapAndSource> bitmap_map,
                        WebApplicationInfo* web_app_info) {
  web_app_info->icons.clear();

  // Populate the icon data into the WebApplicationInfo we are using to
  // install the bookmark app.
  for (const auto& pair : bitmap_map) {
    WebApplicationInfo::IconInfo icon_info;
    icon_info.data = pair.second.bitmap;
    icon_info.url = pair.second.source_url;
    icon_info.width = icon_info.data.width();
    icon_info.height = icon_info.data.height();
    web_app_info->icons.push_back(icon_info);
  }
}

}  // namespace

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

std::set<int> SizesToGenerate() {
  // Generate container icons from smaller icons.
  return std::set<int>({
      icon_size::k32, icon_size::k64, icon_size::k48, icon_size::k96,
      icon_size::k128, icon_size::k256,
  });
}

std::vector<GURL> GetValidIconUrlsToDownload(
    const InstallableData& data,
    const WebApplicationInfo& web_app_info) {
  // Add icon urls to download from the WebApplicationInfo.
  std::vector<GURL> web_app_info_icon_urls;
  for (auto& info : web_app_info.icons) {
    if (!info.url.is_valid())
      continue;

    // Skip downloading icon if we already have it from the InstallableManager.
    if (info.url == data.primary_icon_url && data.primary_icon)
      continue;

    web_app_info_icon_urls.push_back(info.url);
  }

  return web_app_info_icon_urls;
}

void MergeInstallableDataIcon(const InstallableData& data,
                              WebApplicationInfo* web_app_info) {
  if (data.primary_icon_url.is_valid()) {
    WebApplicationInfo::IconInfo primary_icon_info;
    const SkBitmap& icon = *data.primary_icon;
    primary_icon_info.url = data.primary_icon_url;
    primary_icon_info.data = icon;
    primary_icon_info.width = icon.width();
    primary_icon_info.height = icon.height();
    web_app_info->icons.push_back(primary_icon_info);
  }
}

std::vector<BitmapAndSource> FilterSquareIcons(
    const IconsMap& icons_map,
    const WebApplicationInfo& web_app_info) {
  std::vector<BitmapAndSource> downloaded_icons;
  for (const std::pair<GURL, std::vector<SkBitmap>>& url_bitmap : icons_map) {
    for (const SkBitmap& bitmap : url_bitmap.second) {
      if (bitmap.empty() || bitmap.width() != bitmap.height())
        continue;

      downloaded_icons.push_back(BitmapAndSource(url_bitmap.first, bitmap));
    }
  }

  // Add all existing icons from WebApplicationInfo.
  for (const WebApplicationInfo::IconInfo& icon_info : web_app_info.icons) {
    const SkBitmap& icon = icon_info.data;
    if (!icon.drawsNothing() && icon.width() == icon.height()) {
      downloaded_icons.push_back(BitmapAndSource(icon_info.url, icon));
    }
  }

  return downloaded_icons;
}

void ResizeDownloadedIconsGenerateMissing(
    std::vector<BitmapAndSource> downloaded_icons,
    WebApplicationInfo* web_app_info) {
  web_app_info->generated_icon_color = SK_ColorTRANSPARENT;
  std::map<int, BitmapAndSource> size_to_icons = ResizeIconsAndGenerateMissing(
      downloaded_icons, SizesToGenerate(), web_app_info->app_url,
      &web_app_info->generated_icon_color);

  ReplaceWebAppIcons(size_to_icons, web_app_info);
}

}  // namespace web_app
