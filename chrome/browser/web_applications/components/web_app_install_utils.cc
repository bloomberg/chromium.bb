// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_install_utils.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

namespace {

// We restrict the number of icons to limit disk usage per installed PWA. This
// value can change overtime as new features are added.
constexpr int kMaxIcons = 20;
constexpr SquareSizePx kMaxIconSize = 1024;

// Get a list of non-empty square icons from |icons_map|.
void FilterSquareIconsFromMap(const IconsMap& icons_map,
                              std::vector<SkBitmap>* square_icons) {
  for (const auto& url_icon : icons_map) {
    for (const SkBitmap& icon : url_icon.second) {
      if (!icon.empty() && icon.width() == icon.height())
        square_icons->push_back(icon);
    }
  }
}

// Get a list of non-empty square app icons from |icons_map|. We will disregard
// shortcut icons here.
void FilterSquareIconsFromMapDisregardShortcutIcons(
    const std::vector<WebApplicationIconInfo>& icon_infos,
    const IconsMap& icons_map,
    std::vector<SkBitmap>* square_icons) {
  if (icon_infos.empty()) {
    FilterSquareIconsFromMap(icons_map, square_icons);
    return;
  }

  for (const auto& url_icon : icons_map) {
    for (const auto& info : icon_infos) {
      if (info.url == url_icon.first) {
        for (const SkBitmap& icon : url_icon.second) {
          if (!icon.empty() && icon.width() == icon.height())
            square_icons->push_back(icon);
        }
      }
    }
  }
}

// Get all non-empty square icons from |icons_map|.
void FilterSquareIconsFromBitmaps(
    const std::map<SquareSizePx, SkBitmap> bitmaps,
    std::vector<SkBitmap>* square_icons) {
  for (const std::pair<const SquareSizePx, SkBitmap>& icon : bitmaps) {
    DCHECK_EQ(icon.first, icon.second.width());
    DCHECK_EQ(icon.first, icon.second.height());
    if (!icon.second.empty())
      square_icons->push_back(icon.second);
  }
}

// Populate |web_app_info|'s shortcut_infos vector using the blink::Manifest's
// shortcuts vector.
std::vector<WebApplicationShortcutInfo> UpdateShortcutInfosFromManifest(
    const std::vector<blink::Manifest::ShortcutItem>& shortcuts) {
  std::vector<WebApplicationShortcutInfo> web_app_shortcut_infos;
  int num_shortcut_icons = 0;
  for (const auto& shortcut : shortcuts) {
    WebApplicationShortcutInfo shortcut_info;
    shortcut_info.name = shortcut.name;
    shortcut_info.url = shortcut.url;

    std::vector<WebApplicationIconInfo> shortcut_icons;
    for (const auto& icon : shortcut.icons) {
      WebApplicationIconInfo info;

      // Filter out non-square or too large icons.
      auto valid_size_it = std::find_if(
          icon.sizes.begin(), icon.sizes.end(), [](const gfx::Size& size) {
            return size.width() == size.height() &&
                   size.width() <= kMaxIconSize;
          });
      if (valid_size_it == icon.sizes.end())
        continue;
      // TODO(https://crbug.com/1071308): Take the declared icon density and
      // sizes into account.
      info.square_size_px = valid_size_it->width();

      DCHECK_LE(num_shortcut_icons, kMaxIcons);
      if (num_shortcut_icons < kMaxIcons) {
        info.url = icon.src;
        shortcut_icons.push_back(std::move(info));
        ++num_shortcut_icons;
      }
      if (num_shortcut_icons == kMaxIcons)
        break;
    }

    // If any icons are specified in the manifest, they take precedence over
    // any we picked up from web_app_info.
    if (!shortcut_icons.empty())
      shortcut_info.shortcut_icon_infos = std::move(shortcut_icons);
    web_app_shortcut_infos.push_back(std::move(shortcut_info));
  }

  return web_app_shortcut_infos;
}

}  // namespace

void UpdateWebAppInfoFromManifest(const blink::Manifest& manifest,
                                  WebApplicationInfo* web_app_info) {
  if (!manifest.short_name.is_null())
    web_app_info->title = manifest.short_name.string();

  // Give the full length name priority.
  if (!manifest.name.is_null())
    web_app_info->title = manifest.name.string();

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    web_app_info->app_url = manifest.start_url;

  if (manifest.scope.is_valid())
    web_app_info->scope = manifest.scope;

  if (manifest.theme_color)
    web_app_info->theme_color = *manifest.theme_color;

  if (manifest.display != DisplayMode::kUndefined)
    web_app_info->display_mode = manifest.display;

  // Create the WebApplicationInfo icons list *outside* of |web_app_info|, so
  // that we can decide later whether or not to replace the existing icons array
  // (conditionally on whether there were any that didn't have purpose ANY).
  std::vector<WebApplicationIconInfo> web_app_icons;
  for (const auto& icon : manifest.icons) {
    // An icon's purpose vector should never be empty (the manifest parser
    // should have added ANY if there was no purpose specified in the manifest).
    DCHECK(!icon.purpose.empty());

    if (!base::Contains(icon.purpose,
                        blink::Manifest::ImageResource::Purpose::ANY)) {
      continue;
    }

    WebApplicationIconInfo info;

    if (!icon.sizes.empty()) {
      // Filter out non-square or too large icons.
      auto valid_size = std::find_if(icon.sizes.begin(), icon.sizes.end(),
                                     [](const gfx::Size& size) {
                                       return size.width() == size.height() &&
                                              size.width() <= kMaxIconSize;
                                     });
      if (valid_size == icon.sizes.end())
        continue;
      // TODO(https://crbug.com/1071308): Take the declared icon density and
      // sizes into account.
      info.square_size_px = valid_size->width();
    }

    info.url = icon.src;
    web_app_icons.push_back(std::move(info));

    // Limit the number of icons we store on the user's machine.
    if (web_app_icons.size() == kMaxIcons)
      break;
  }
  // If any icons are specified in the manifest, they take precedence over any
  // we picked up from the web_app stuff.
  if (!web_app_icons.empty())
    web_app_info->icon_infos = std::move(web_app_icons);

  web_app_info->file_handlers = manifest.file_handlers;

  // If any shortcuts are specified in the manifest, they take precedence over
  // any we picked up from the web_app stuff.
  if (!manifest.shortcuts.empty() &&
      base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu)) {
    web_app_info->shortcut_infos =
        UpdateShortcutInfosFromManifest(manifest.shortcuts);
  }
}

std::vector<GURL> GetValidIconUrlsToDownload(
    const WebApplicationInfo& web_app_info) {
  std::vector<GURL> web_app_info_icon_urls;
  for (const WebApplicationIconInfo& info : web_app_info.icon_infos) {
    if (!info.url.is_valid())
      continue;
    web_app_info_icon_urls.push_back(info.url);
  }
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu)) {
    // Also add shortcut icon urls, so they can be downloaded.
    for (const auto& shortcut : web_app_info.shortcut_infos) {
      for (const auto& icon : shortcut.shortcut_icon_infos) {
        web_app_info_icon_urls.push_back(icon.url);
      }
    }
  }
  return web_app_info_icon_urls;
}

void PopulateShortcutItemIcons(WebApplicationInfo* web_app_info,
                               const IconsMap* icons_map) {
  for (auto& shortcut : web_app_info->shortcut_infos) {
    for (const auto& icon : shortcut.shortcut_icon_infos) {
      auto it = icons_map->find(icon.url);
      if (it != icons_map->end()) {
        std::set<SquareSizePx> sizes_to_generate;
        sizes_to_generate.emplace(icon.square_size_px);
        std::map<SquareSizePx, SkBitmap> resized_bitmaps(
            ConstrainBitmapsToSizes(it->second, sizes_to_generate));

        // Don't overwrite as a shortcut item could have multiple icon urls.
        shortcut.shortcut_icon_bitmaps.insert(resized_bitmaps.begin(),
                                              resized_bitmaps.end());
      }
    }
  }
}

void FilterAndResizeIconsGenerateMissing(WebApplicationInfo* web_app_info,
                                         const IconsMap* icons_map) {
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu) &&
      icons_map) {
    PopulateShortcutItemIcons(web_app_info, icons_map);
  }

  // Ensure that all top-level icons that are in web_app_info are present, by
  // generating icons for any sizes which have failed to download. This ensures
  // that the created manifest for the web app does not contain links to icons
  // which are not actually created and linked on disk.
  std::vector<SkBitmap> square_icons;
  if (icons_map) {
    if (base::FeatureList::IsEnabled(
            features::kDesktopPWAsAppIconShortcutsMenu)) {
      FilterSquareIconsFromMapDisregardShortcutIcons(web_app_info->icon_infos,
                                                     *icons_map, &square_icons);
    } else {
      FilterSquareIconsFromMap(*icons_map, &square_icons);
    }
  }
  FilterSquareIconsFromBitmaps(web_app_info->icon_bitmaps, &square_icons);

  base::char16 icon_letter =
      web_app_info->title.empty()
          ? GenerateIconLetterFromUrl(web_app_info->app_url)
          : GenerateIconLetterFromAppName(web_app_info->title);
  web_app_info->generated_icon_color = SK_ColorTRANSPARENT;
  // TODO(https://crbug.com/1029223): Don't resize before writing to disk, it's
  // not necessary and would simplify this code path to remove.
  std::map<SquareSizePx, SkBitmap> size_to_icon = ResizeIconsAndGenerateMissing(
      square_icons, SizesToGenerate(), icon_letter,
      &web_app_info->generated_icon_color);

  for (std::pair<const SquareSizePx, SkBitmap>& item : size_to_icon) {
    // Retain any bitmaps provided as input to the installation.
    if (web_app_info->icon_bitmaps.count(item.first) == 0) {
      web_app_info->icon_bitmaps[item.first] = std::move(item.second);
    }
  }
}

void RecordAppBanner(content::WebContents* contents, const GURL& app_url) {
  AppBannerSettingsHelper::RecordBannerEvent(
      contents, app_url, app_url.spec(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      base::Time::Now());
}

WebappInstallSource ConvertExternalInstallSourceToInstallSource(
    ExternalInstallSource external_install_source) {
  WebappInstallSource install_source;
  switch (external_install_source) {
    case ExternalInstallSource::kInternalDefault:
      install_source = WebappInstallSource::INTERNAL_DEFAULT;
      break;
    case ExternalInstallSource::kExternalDefault:
      install_source = WebappInstallSource::EXTERNAL_DEFAULT;
      break;
    case ExternalInstallSource::kExternalPolicy:
      install_source = WebappInstallSource::EXTERNAL_POLICY;
      break;
    case ExternalInstallSource::kSystemInstalled:
      install_source = WebappInstallSource::SYSTEM_DEFAULT;
      break;
    case ExternalInstallSource::kArc:
      install_source = WebappInstallSource::ARC;
      break;
  }

  return install_source;
}

void RecordExternalAppInstallResultCode(
    const char* histogram_name,
    std::map<GURL, InstallResultCode> install_results) {
  for (const auto& url_and_result : install_results)
    base::UmaHistogramEnumeration(histogram_name, url_and_result.second);
}

}  // namespace web_app
