// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_WALLPAPER_THUMBNAIL_SOURCE2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_WALLPAPER_THUMBNAIL_SOURCE2_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"

namespace chromeos {
namespace options2 {

// Returns a string consisting of the prefix specified and the index of the
// image. For example: chrome://wallpaper/default_2.
std::string GetDefaultWallpaperThumbnailURL(int index);

// Checks if the given URL points to one of the default wallpapers. If it is,
// returns true and sets |wallpaper_index| to the corresponding index parsed
// from |url|. For example: chrome://wallpaper/default_2 will set
// |wallpaper_index| to 2. If not a default wallpaper url, returns false.
// |url| and |wallpaper_index| must not be NULL.
bool IsDefaultWallpaperURL(const std::string url, int* wallpaper_index);

// A DataSource for chrome://wallpaper/ URLs.
class WallpaperThumbnailSource : public ChromeURLDataManager::DataSource {
 public:
  WallpaperThumbnailSource();

  // ChromeURLDataManager::DataSource implementation.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id) OVERRIDE;
  virtual std::string GetMimeType(const std::string&) const OVERRIDE;

 private:
  virtual ~WallpaperThumbnailSource();

  DISALLOW_COPY_AND_ASSIGN(WallpaperThumbnailSource);
};

}  // namespace options2
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_WALLPAPER_THUMBNAIL_SOURCE2_H_
