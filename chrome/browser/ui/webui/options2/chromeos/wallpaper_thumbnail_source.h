// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_WALLPAPER_THUMBNAIL_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_WALLPAPER_THUMBNAIL_SOURCE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"

namespace base {
class RefCountedBytes;
}

namespace chromeos {

class User;

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

  virtual std::string GetMimeType(const std::string&) const OVERRIDE;

  // ChromeURLDataManager::DataSource implementation.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id) OVERRIDE;

 private:
  class ThumbnailEncodingOperation;

  virtual ~WallpaperThumbnailSource();

  void GetCurrentUserThumbnail(const std::string& path,
                               int request_id);

  void StartCustomThumbnailEncodingOperation(const chromeos::User* user,
                                             int request_id);

  void CancelPendingCustomThumbnailEncodingOperation();

  void SendCurrentUserNullThumbnail(int request_id);

  void SendCurrentUserCustomThumbnail(
      scoped_refptr<base::RefCountedBytes> data,
      int request_id);

  void SendCurrentUserDefaultThumbnail(const std::string& path,
                                       int request_id);

  scoped_refptr<ThumbnailEncodingOperation> thumbnail_encoding_op_;

  base::WeakPtrFactory<WallpaperThumbnailSource> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperThumbnailSource);
};

}  // namespace options2
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_WALLPAPER_THUMBNAIL_SOURCE_H_
