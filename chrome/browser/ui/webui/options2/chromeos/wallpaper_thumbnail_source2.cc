// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/wallpaper_thumbnail_source2.h"

#include "ash/desktop_background/desktop_background_resources.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "grit/ui_resources.h"
#include "net/base/mime_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"

namespace chromeos {
namespace options2 {

namespace {

const char kDefaultWallpaperPrefix[] = "default_";
const char kCustomWallpaperPrefix[] = "custom_";

// Parse an integer from |path| and save it to |index|. For example, default_20
// will set |index| to 20.
// |path| and |index| must not be NULL.
bool ParseIndexFromPath(const std::string& path, int* index) {
  // TODO(bshe): We should probably save a string instead of index for
  // extensibility. Remove this function when we migrate to string preference.
  DCHECK(index);
  if (!StartsWithASCII(path, kDefaultWallpaperPrefix, false))
    return false;
  return base::StringToInt(base::StringPiece(path.begin() +
      strlen(kDefaultWallpaperPrefix), path.end()), index);
}

// Convert |path| to corresponding IDR. Return -1 if the path is invalid.
// |path| must not be NULL.
int PathToIDR(const std::string& path) {
  int idr = -1;
  int index = ash::GetInvalidWallpaperIndex();
  if (ParseIndexFromPath(path, &index))
    idr = ash::GetWallpaperInfo(index).thumb_id;
  return idr;
}

// True if |path| is a custom wallpaper thumbnail URL and set |email| parsed
// from |path|.
// custom url = "custom_|email|?date" where date is current time.
bool IsCustomWallpaperPath(const std::string& path, std::string* email) {
  if (!StartsWithASCII(path, kCustomWallpaperPrefix, false))
    return false;

  std::string sub_path = path.substr(strlen(kCustomWallpaperPrefix));
  *email = sub_path.substr(0, sub_path.find_first_of("?"));
  return true;
}

}  // namespace

std::string GetDefaultWallpaperThumbnailURL(int index) {
  return StringPrintf("%s%s%d", chrome::kChromeUIWallpaperThumbnailURL,
                      kDefaultWallpaperPrefix, index);
}

bool IsDefaultWallpaperURL(const std::string url, int* wallpaper_index) {
  DCHECK(wallpaper_index);
  *wallpaper_index = ash::GetInvalidWallpaperIndex();
  if (!StartsWithASCII(url, chrome::kChromeUIWallpaperThumbnailURL, false))
      return false;
  std::string sub_path = url.substr(strlen(
      chrome::kChromeUIWallpaperThumbnailURL));
  return ParseIndexFromPath(sub_path, wallpaper_index);
}

WallpaperThumbnailSource::WallpaperThumbnailSource()
    : DataSource(chrome::kChromeUIWallpaperThumbnailHost, NULL) {
}

WallpaperThumbnailSource::~WallpaperThumbnailSource() {
}

void WallpaperThumbnailSource::StartDataRequest(const std::string& path,
                                                bool is_incognito,
                                                int request_id) {
  std::string email;
  if (IsCustomWallpaperPath(path, &email)) {
    const chromeos::User* user = chromeos::UserManager::Get()->FindUser(email);
    if (user) {
      std::vector<unsigned char> data;
      gfx::PNGCodec::EncodeBGRASkBitmap(user->wallpaper_thumbnail(),
                                        false, &data);
      SendResponse(request_id, new base::RefCountedBytes(data));
      return;
    }
  }
  int idr = PathToIDR(path);
  if (idr != -1) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SendResponse(request_id, rb.LoadDataResourceBytes(idr));
  }
}

std::string WallpaperThumbnailSource::GetMimeType(const std::string&) const {
  return "images/png";
}

}  // namespace options2
}  // namespace chromeos
