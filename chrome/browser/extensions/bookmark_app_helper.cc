// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/favicon_downloader.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/url_pattern.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image.h"

namespace extensions {

// static
std::map<int, SkBitmap> BookmarkAppHelper::ConstrainBitmapsToSizes(
    const std::vector<SkBitmap>& bitmaps,
    const std::set<int>& sizes) {
  std::map<int, SkBitmap> output_bitmaps;
  std::map<int, SkBitmap> ordered_bitmaps;
  for (std::vector<SkBitmap>::const_iterator it = bitmaps.begin();
       it != bitmaps.end();
       ++it) {
    DCHECK(it->width() == it->height());
    ordered_bitmaps[it->width()] = *it;
  }

  std::set<int>::const_iterator sizes_it = sizes.begin();
  std::map<int, SkBitmap>::const_iterator bitmaps_it = ordered_bitmaps.begin();
  while (sizes_it != sizes.end() && bitmaps_it != ordered_bitmaps.end()) {
    int size = *sizes_it;
    // Find the closest not-smaller bitmap.
    bitmaps_it = ordered_bitmaps.lower_bound(size);
    ++sizes_it;
    // Ensure the bitmap is valid and smaller than the next allowed size.
    if (bitmaps_it != ordered_bitmaps.end() &&
        (sizes_it == sizes.end() || bitmaps_it->second.width() < *sizes_it)) {
      // Resize the bitmap if it does not exactly match the desired size.
      output_bitmaps[size] = bitmaps_it->second.width() == size
                                 ? bitmaps_it->second
                                 : skia::ImageOperations::Resize(
                                       bitmaps_it->second,
                                       skia::ImageOperations::RESIZE_LANCZOS3,
                                       size,
                                       size);
    }
  }
  return output_bitmaps;
}

// static
void BookmarkAppHelper::GenerateContainerIcon(std::map<int, SkBitmap>* bitmaps,
                                              int output_size) {
  std::map<int, SkBitmap>::const_iterator it =
      bitmaps->lower_bound(output_size);
  // Do nothing if there is no icon smaller than the desired size or there is
  // already an icon of |output_size|.
  if (it == bitmaps->begin() || bitmaps->count(output_size))
    return;

  --it;
  // This is the biggest icon smaller than |output_size|.
  const SkBitmap& base_icon = it->second;

  const size_t kBorderRadius = 5;
  const size_t kColorStripHeight = 3;
  const SkColor kBorderColor = 0xFFD5D5D5;
  const SkColor kBackgroundColor = 0xFFFFFFFF;

  // Create a separate canvas for the color strip.
  scoped_ptr<SkCanvas> color_strip_canvas(
      skia::CreateBitmapCanvas(output_size, output_size, false));
  DCHECK(color_strip_canvas);

  // Draw a rounded rect of the |base_icon|'s dominant color.
  SkPaint color_strip_paint;
  color_utils::GridSampler sampler;
  color_strip_paint.setFlags(SkPaint::kAntiAlias_Flag);
  color_strip_paint.setColor(color_utils::CalculateKMeanColorOfPNG(
      gfx::Image::CreateFrom1xBitmap(base_icon).As1xPNGBytes(),
      100,
      665,
      &sampler));
  color_strip_canvas->drawRoundRect(SkRect::MakeWH(output_size, output_size),
                                    kBorderRadius,
                                    kBorderRadius,
                                    color_strip_paint);

  // Erase the top of the rounded rect to leave a color strip.
  SkPaint clear_paint;
  clear_paint.setColor(SK_ColorTRANSPARENT);
  clear_paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  color_strip_canvas->drawRect(
      SkRect::MakeWH(output_size, output_size - kColorStripHeight),
      clear_paint);

  // Draw each element to an output canvas.
  scoped_ptr<SkCanvas> canvas(
      skia::CreateBitmapCanvas(output_size, output_size, false));
  DCHECK(canvas);

  // Draw the background.
  SkPaint background_paint;
  background_paint.setColor(kBackgroundColor);
  background_paint.setFlags(SkPaint::kAntiAlias_Flag);
  canvas->drawRoundRect(SkRect::MakeWH(output_size, output_size),
                        kBorderRadius,
                        kBorderRadius,
                        background_paint);

  // Draw the color strip.
  canvas->drawBitmap(
      color_strip_canvas->getDevice()->accessBitmap(false), 0, 0);

  // Draw the border.
  SkPaint border_paint;
  border_paint.setColor(kBorderColor);
  border_paint.setStyle(SkPaint::kStroke_Style);
  border_paint.setFlags(SkPaint::kAntiAlias_Flag);
  canvas->drawRoundRect(SkRect::MakeWH(output_size, output_size),
                        kBorderRadius,
                        kBorderRadius,
                        border_paint);

  // Draw the centered base icon to the output canvas.
  canvas->drawBitmap(base_icon,
                     (output_size - base_icon.width()) / 2,
                     (output_size - base_icon.height()) / 2);

  const SkBitmap& generated_icon = canvas->getDevice()->accessBitmap(false);
  generated_icon.deepCopyTo(&(*bitmaps)[output_size]);
}

BookmarkAppHelper::BookmarkAppHelper(ExtensionService* service,
                                     WebApplicationInfo web_app_info,
                                     content::WebContents* contents)
    : web_app_info_(web_app_info),
      crx_installer_(extensions::CrxInstaller::CreateSilent(service)) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                 content::Source<CrxInstaller>(crx_installer_.get()));

  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
                 content::Source<CrxInstaller>(crx_installer_.get()));

  crx_installer_->set_error_on_unsupported_requirements(true);

  // Add urls from the WebApplicationInfo.
  std::vector<GURL> web_app_info_icon_urls;
  for (std::vector<WebApplicationInfo::IconInfo>::const_iterator it =
           web_app_info_.icons.begin();
       it != web_app_info_.icons.end();
       ++it) {
    if (it->url.is_valid())
      web_app_info_icon_urls.push_back(it->url);
  }

  favicon_downloader_.reset(
      new FaviconDownloader(contents,
                            web_app_info_icon_urls,
                            base::Bind(&BookmarkAppHelper::OnIconsDownloaded,
                                       base::Unretained(this))));
}

BookmarkAppHelper::~BookmarkAppHelper() {}

void BookmarkAppHelper::Create(const CreateBookmarkAppCallback& callback) {
  callback_ = callback;
  favicon_downloader_->Start();
}

void BookmarkAppHelper::OnIconsDownloaded(
    bool success,
    const std::map<GURL, std::vector<SkBitmap> >& bitmaps) {
  // The tab has navigated away during the icon download. Cancel the bookmark
  // app creation.
  if (!success) {
    favicon_downloader_.reset();
    callback_.Run(NULL, web_app_info_);
    return;
  }

  // Add the downloaded icons. Extensions only allow certain icon sizes. First
  // populate icons that match the allowed sizes exactly and then downscale
  // remaining icons to the closest allowed size that doesn't yet have an icon.
  std::set<int> allowed_sizes(extension_misc::kExtensionIconSizes,
                              extension_misc::kExtensionIconSizes +
                                  extension_misc::kNumExtensionIconSizes);
  std::vector<SkBitmap> downloaded_icons;
  for (FaviconDownloader::FaviconMap::const_iterator map_it = bitmaps.begin();
       map_it != bitmaps.end();
       ++map_it) {
    for (std::vector<SkBitmap>::const_iterator bitmap_it =
             map_it->second.begin();
         bitmap_it != map_it->second.end();
         ++bitmap_it) {
      if (bitmap_it->empty() || bitmap_it->width() != bitmap_it->height())
        continue;

      downloaded_icons.push_back(*bitmap_it);
    }
  }

  // If there are icons that don't match the accepted icon sizes, find the
  // closest bigger icon to the accepted sizes and resize the icon to it. An
  // icon will be resized and used for at most one size.
  std::map<int, SkBitmap> resized_bitmaps(
      ConstrainBitmapsToSizes(downloaded_icons, allowed_sizes));

  // Generate container icons from smaller icons.
  const int kIconSizesToGenerate[] = {extension_misc::EXTENSION_ICON_SMALL,
                                      extension_misc::EXTENSION_ICON_MEDIUM, };
  const std::set<int> generate_sizes(
      kIconSizesToGenerate,
      kIconSizesToGenerate + arraysize(kIconSizesToGenerate));

  // Only generate icons if larger icons don't exist. This means the app
  // launcher and the taskbar will do their best downsizing large icons and
  // these container icons are only generated as a last resort against upscaling
  // a smaller icon.
  if (resized_bitmaps.lower_bound(*generate_sizes.rbegin()) ==
      resized_bitmaps.end()) {
    // Generate these from biggest to smallest so we don't end up with
    // concentric container icons.
    for (std::set<int>::const_reverse_iterator it = generate_sizes.rbegin();
         it != generate_sizes.rend();
         ++it) {
      GenerateContainerIcon(&resized_bitmaps, *it);
    }
  }

  // Populate the icon data into the WebApplicationInfo we are using to
  // install the bookmark app.
  for (std::map<int, SkBitmap>::const_iterator resized_bitmaps_it =
           resized_bitmaps.begin();
       resized_bitmaps_it != resized_bitmaps.end();
       ++resized_bitmaps_it) {
    WebApplicationInfo::IconInfo icon_info;
    icon_info.data = resized_bitmaps_it->second;
    icon_info.width = icon_info.data.width();
    icon_info.height = icon_info.data.height();
    web_app_info_.icons.push_back(icon_info);
  }

  // Install the app.
  crx_installer_->InstallWebApp(web_app_info_);
  favicon_downloader_.reset();
}

void BookmarkAppHelper::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_CRX_INSTALLER_DONE: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      DCHECK(extension);
      DCHECK_EQ(AppLaunchInfo::GetLaunchWebURL(extension),
                web_app_info_.app_url);
      callback_.Run(extension, web_app_info_);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR:
      callback_.Run(NULL, web_app_info_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void CreateOrUpdateBookmarkApp(ExtensionService* service,
                               WebApplicationInfo& web_app_info) {
  scoped_refptr<extensions::CrxInstaller> installer(
      extensions::CrxInstaller::CreateSilent(service));
  installer->set_error_on_unsupported_requirements(true);
  installer->InstallWebApp(web_app_info);
}

bool IsValidBookmarkAppUrl(const GURL& url) {
  URLPattern origin_only_pattern(Extension::kValidWebExtentSchemes);
  origin_only_pattern.SetMatchAllURLs(true);
  return url.is_valid() && origin_only_pattern.MatchesURL(url);
}

}  // namespace extensions
