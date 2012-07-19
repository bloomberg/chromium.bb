// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/wallpaper_thumbnail_source.h"

#include "ash/desktop_background/desktop_background_resources.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "grit/ui_resources.h"
#include "net/base/mime_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"

namespace chromeos {
namespace options2 {

class WallpaperThumbnailSource::ThumbnailEncodingOperation
  : public base::RefCountedThreadSafe<
        WallpaperThumbnailSource::ThumbnailEncodingOperation> {
 public:
  ThumbnailEncodingOperation(int request_id,
                             const chromeos::User* user,
                             scoped_refptr<base::RefCountedBytes> data)
      : request_id_(request_id),
        user_(user),
        data_(data) {
  }

  int request_id() {
    return request_id_;
  }

  static void Run(scoped_refptr<ThumbnailEncodingOperation> teo) {
    teo->EncodeThumbnail();
  }

  void EncodeThumbnail() {
    if (cancel_flag_.IsSet())
      return;
    gfx::PNGCodec::EncodeBGRASkBitmap(user_->wallpaper_thumbnail(),
                                      false, &data_->data());
  }

  void Cancel() {
    cancel_flag_.Set();
  }

 private:
  friend class base::RefCountedThreadSafe<
      WallpaperThumbnailSource::ThumbnailEncodingOperation>;

  ~ThumbnailEncodingOperation() {}

  base::CancellationFlag cancel_flag_;

  int request_id_;

  const chromeos::User* user_;

  scoped_refptr<base::RefCountedBytes> data_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailEncodingOperation);
};

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
    : DataSource(chrome::kChromeUIWallpaperThumbnailHost, NULL),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

std::string WallpaperThumbnailSource::GetMimeType(const std::string&) const {
  return "images/png";
}

void WallpaperThumbnailSource::StartDataRequest(const std::string& path,
                                                bool is_incognito,
                                                int request_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  CancelPendingCustomThumbnailEncodingOperation();
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WallpaperThumbnailSource::GetCurrentUserThumbnail,
                 this, path, request_id));
}

WallpaperThumbnailSource::~WallpaperThumbnailSource() {
}

void WallpaperThumbnailSource::GetCurrentUserThumbnail(
    const std::string& path,
    int request_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string email;
  if (IsCustomWallpaperPath(path, &email)) {
    const chromeos::User* user = chromeos::UserManager::Get()->FindUser(email);
    if (!user) {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(&WallpaperThumbnailSource::SendCurrentUserNullThumbnail,
                     this, request_id));
      return;
    }
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(
            &WallpaperThumbnailSource::StartCustomThumbnailEncodingOperation,
            this, user, request_id));
    return;
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&WallpaperThumbnailSource::SendCurrentUserDefaultThumbnail,
                 this, path, request_id));
}

void WallpaperThumbnailSource::StartCustomThumbnailEncodingOperation(
    const chromeos::User* user,
    int request_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  CancelPendingCustomThumbnailEncodingOperation();
  scoped_refptr<base::RefCountedBytes> data = new base::RefCountedBytes();
  thumbnail_encoding_op_ = new ThumbnailEncodingOperation(request_id,
                                                          user,
                                                          data);
  base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ThumbnailEncodingOperation::Run, thumbnail_encoding_op_),
      base::Bind(&WallpaperThumbnailSource::SendCurrentUserCustomThumbnail,
                 weak_ptr_factory_.GetWeakPtr(), data, request_id),
      true /* task_is_slow */);
}

void WallpaperThumbnailSource::CancelPendingCustomThumbnailEncodingOperation() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (thumbnail_encoding_op_.get()) {
    thumbnail_encoding_op_->Cancel();
    SendResponse(thumbnail_encoding_op_->request_id(), NULL);
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WallpaperThumbnailSource::SendCurrentUserNullThumbnail(int request_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  SendResponse(request_id, NULL);
}

void WallpaperThumbnailSource::SendCurrentUserCustomThumbnail(
  scoped_refptr<base::RefCountedBytes> data,
  int request_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  SendResponse(request_id, data);
}

void WallpaperThumbnailSource::SendCurrentUserDefaultThumbnail(
  const std::string& path,
  int request_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  int idr = PathToIDR(path);
  if (idr == -1) {
    SendResponse(request_id, NULL);
    return;
  }
  const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SendResponse(request_id,
               rb.LoadDataResourceBytes(idr, ui::SCALE_FACTOR_100P));
}

}  // namespace options2
}  // namespace chromeos
