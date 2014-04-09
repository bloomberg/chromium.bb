// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_api.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

using base::BinaryValue;
using content::BrowserThread;

typedef base::Callback<void(bool success, const std::string&)> FetchCallback;

namespace set_wallpaper = extensions::api::wallpaper::SetWallpaper;

namespace {

class WallpaperFetcher : public net::URLFetcherDelegate {
 public:
  WallpaperFetcher() {}

  virtual ~WallpaperFetcher() {}

  void FetchWallpaper(const GURL& url, FetchCallback callback) {
    CancelPreviousFetch();
    callback_ = callback;
    url_fetcher_.reset(net::URLFetcher::Create(url,
                                               net::URLFetcher::GET,
                                               this));
    url_fetcher_->SetRequestContext(
        g_browser_process->system_request_context());
    url_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
    url_fetcher_->Start();
  }

 private:
  // URLFetcherDelegate overrides:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    DCHECK(url_fetcher_.get() == source);

    bool success = source->GetStatus().is_success() &&
                   source->GetResponseCode() == net::HTTP_OK;
    std::string response;
    if (success) {
      source->GetResponseAsString(&response);
    } else {
      response = base::StringPrintf(
          "Downloading wallpaper %s failed. The response code is %d.",
          source->GetOriginalURL().ExtractFileName().c_str(),
          source->GetResponseCode());
    }
    url_fetcher_.reset();
    callback_.Run(success, response);
  }

  void CancelPreviousFetch() {
    if (url_fetcher_.get()) {
      std::string error = base::StringPrintf(
          "Downloading wallpaper %s is canceled.",
          url_fetcher_->GetOriginalURL().ExtractFileName().c_str());
      callback_.Run(false, error);
      url_fetcher_.reset();
    }
  }

  scoped_ptr<net::URLFetcher> url_fetcher_;
  FetchCallback callback_;
};

base::LazyInstance<WallpaperFetcher> g_wallpaper_fetcher =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

WallpaperSetWallpaperFunction::WallpaperSetWallpaperFunction() {
}

WallpaperSetWallpaperFunction::~WallpaperSetWallpaperFunction() {
}

bool WallpaperSetWallpaperFunction::RunImpl() {
  params_ = set_wallpaper::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);

  // Gets email address and username hash while at UI thread.
  user_id_ = chromeos::UserManager::Get()->GetLoggedInUser()->email();
  user_id_hash_ =
      chromeos::UserManager::Get()->GetLoggedInUser()->username_hash();

  if (params_->details.wallpaper_data) {
    StartDecode(*params_->details.wallpaper_data);
  } else {
    GURL wallpaper_url(*params_->details.url);
    if (wallpaper_url.is_valid()) {
      g_wallpaper_fetcher.Get().FetchWallpaper(
          wallpaper_url,
          base::Bind(&WallpaperSetWallpaperFunction::OnWallpaperFetched, this));
    } else {
      SetError("URL is invalid.");
      SendResponse(false);
    }
  }
  return true;
}

void WallpaperSetWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& wallpaper) {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  chromeos::UserImage::RawImage raw_image(
      params_->details.wallpaper_data->begin(),
      params_->details.wallpaper_data->end());
  chromeos::UserImage image(wallpaper, raw_image);
  base::FilePath thumbnail_path = wallpaper_manager->GetCustomWallpaperPath(
      chromeos::kThumbnailWallpaperSubDir,
      user_id_hash_,
      params_->details.name);

  sequence_token_ = BrowserThread::GetBlockingPool()->
      GetNamedSequenceToken(chromeos::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::BLOCK_SHUTDOWN);
  ash::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      set_wallpaper::Params::Details::ToString(params_->details.layout));
  bool update_wallpaper =
      user_id_ == chromeos::UserManager::Get()->GetActiveUser()->email();
  wallpaper_manager->SetCustomWallpaper(user_id_,
                                        user_id_hash_,
                                        params_->details.name,
                                        layout,
                                        chromeos::User::CUSTOMIZED,
                                        image,
                                        update_wallpaper);
  unsafe_wallpaper_decoder_ = NULL;

  if (params_->details.thumbnail) {
    wallpaper.EnsureRepsForSupportedScales();
    scoped_ptr<gfx::ImageSkia> deep_copy(wallpaper.DeepCopy());
    // Generates thumbnail before call api function callback. We can then
    // request thumbnail in the javascript callback.
    task_runner->PostTask(FROM_HERE,
        base::Bind(
            &WallpaperSetWallpaperFunction::GenerateThumbnail,
            this, thumbnail_path, base::Passed(&deep_copy)));
  } else {
    SendResponse(true);
  }
}

void WallpaperSetWallpaperFunction::GenerateThumbnail(
    const base::FilePath& thumbnail_path, scoped_ptr<gfx::ImageSkia> image) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  chromeos::UserImage wallpaper(*image.get());
  if (!base::PathExists(thumbnail_path.DirName()))
    base::CreateDirectory(thumbnail_path.DirName());

  scoped_refptr<base::RefCountedBytes> data;
  chromeos::WallpaperManager::Get()->ResizeWallpaper(
      wallpaper,
      ash::WALLPAPER_LAYOUT_STRETCH,
      chromeos::kWallpaperThumbnailWidth,
      chromeos::kWallpaperThumbnailHeight,
      &data,
      NULL);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &WallpaperSetWallpaperFunction::ThumbnailGenerated,
          this, data));
}

void WallpaperSetWallpaperFunction::ThumbnailGenerated(
    base::RefCountedBytes* data) {
  BinaryValue* result = BinaryValue::CreateWithCopiedBuffer(
      reinterpret_cast<const char*>(data->front()), data->size());
  SetResult(result);
  SendResponse(true);
}

void WallpaperSetWallpaperFunction::OnWallpaperFetched(
    bool success,
    const std::string& response) {
  if (success) {
    params_->details.wallpaper_data.reset(new std::string(response));
    StartDecode(*params_->details.wallpaper_data);
  } else {
    SetError(response);
    SendResponse(false);
  }
}
