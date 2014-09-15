// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/customization_wallpaper_downloader.h"

#include <math.h>
#include <algorithm>

#include "base/files/file_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace chromeos {
namespace {
// This is temporary file suffix (for downloading or resizing).
const char kTemporarySuffix[] = ".tmp";

// Sleep between wallpaper retries (used multiplied by squared retry number).
const unsigned kRetrySleepSeconds = 10;

// Retry is infinite with increasing intervals. When calculated delay becomes
// longer than maximum (kMaxRetrySleepSeconds) it is set to the maximum.
const double kMaxRetrySleepSeconds = 6 * 3600;  // 6 hours

void CreateWallpaperDirectory(const base::FilePath& wallpaper_dir,
                              bool* success) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  DCHECK(success);

  *success = CreateDirectoryAndGetError(wallpaper_dir, NULL);
  if (!*success) {
    NOTREACHED() << "Failed to create directory '" << wallpaper_dir.value()
                 << "'";
  }
}

void RenameTemporaryFile(const base::FilePath& from,
                         const base::FilePath& to,
                         bool* success) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  DCHECK(success);

  base::File::Error error;
  if (base::ReplaceFile(from, to, &error)) {
    *success = true;
  } else {
    LOG(WARNING)
        << "Failed to rename temporary file of Customized Wallpaper. error="
        << error;
    *success = false;
  }
}

}  // namespace

CustomizationWallpaperDownloader::CustomizationWallpaperDownloader(
    net::URLRequestContextGetter* url_context_getter,
    const GURL& wallpaper_url,
    const base::FilePath& wallpaper_dir,
    const base::FilePath& wallpaper_downloaded_file,
    base::Callback<void(bool success, const GURL&)>
        on_wallpaper_fetch_completed)
    : url_context_getter_(url_context_getter),
      wallpaper_url_(wallpaper_url),
      wallpaper_dir_(wallpaper_dir),
      wallpaper_downloaded_file_(wallpaper_downloaded_file),
      wallpaper_temporary_file_(wallpaper_downloaded_file.value() +
                                kTemporarySuffix),
      retries_(0),
      retry_delay_(base::TimeDelta::FromSeconds(kRetrySleepSeconds)),
      on_wallpaper_fetch_completed_(on_wallpaper_fetch_completed),
      weak_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

CustomizationWallpaperDownloader::~CustomizationWallpaperDownloader() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void CustomizationWallpaperDownloader::StartRequest() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(wallpaper_url_.is_valid());

  url_fetcher_.reset(
      net::URLFetcher::Create(wallpaper_url_, net::URLFetcher::GET, this));
  url_fetcher_->SetRequestContext(url_context_getter_.get());
  url_fetcher_->SetLoadFlags(net::LOAD_BYPASS_CACHE |
                             net::LOAD_DISABLE_CACHE |
                             net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SEND_AUTH_DATA);
  base::SequencedWorkerPool* blocking_pool =
      content::BrowserThread::GetBlockingPool();
  url_fetcher_->SaveResponseToFileAtPath(
      wallpaper_temporary_file_,
      blocking_pool->GetSequencedTaskRunner(blocking_pool->GetSequenceToken()));
  url_fetcher_->Start();
}

void CustomizationWallpaperDownloader::Retry() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ++retries_;

  const double delay_seconds = std::min(
      kMaxRetrySleepSeconds,
      static_cast<double>(retries_) * retries_ * retry_delay_.InSecondsF());
  const base::TimeDelta delay = base::TimeDelta::FromSecondsD(delay_seconds);

  VLOG(1) << "Schedule Customized Wallpaper download in " << delay.InSecondsF()
          << " seconds (retry = " << retries_ << ").";
  retry_current_delay_ = delay;
  request_scheduled_.Start(
      FROM_HERE, delay, this, &CustomizationWallpaperDownloader::StartRequest);
}

void CustomizationWallpaperDownloader::Start() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  scoped_ptr<bool> success(new bool(false));

  base::Closure mkdir_closure = base::Bind(&CreateWallpaperDirectory,
                                           wallpaper_dir_,
                                           base::Unretained(success.get()));
  base::Closure on_created_closure =
      base::Bind(&CustomizationWallpaperDownloader::OnWallpaperDirectoryCreated,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(success.Pass()));
  if (!content::BrowserThread::PostBlockingPoolTaskAndReply(
          FROM_HERE, mkdir_closure, on_created_closure)) {
    LOG(WARNING) << "Failed to start Customized Wallpaper download.";
  }
}

void CustomizationWallpaperDownloader::OnWallpaperDirectoryCreated(
    scoped_ptr<bool> success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (*success)
    StartRequest();
}

void CustomizationWallpaperDownloader::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(url_fetcher_.get(), source);

  const net::URLRequestStatus status = source->GetStatus();
  const int response_code = source->GetResponseCode();

  const bool server_error =
      !status.is_success() ||
      (response_code >= net::HTTP_INTERNAL_SERVER_ERROR &&
       response_code < (net::HTTP_INTERNAL_SERVER_ERROR + 100));

  VLOG(1) << "CustomizationWallpaperDownloader::OnURLFetchComplete(): status="
          << status.status();

  if (server_error) {
    url_fetcher_.reset();
    Retry();
    return;
  }

  base::FilePath response_path;
  url_fetcher_->GetResponseAsFilePath(true, &response_path);
  url_fetcher_.reset();

  scoped_ptr<bool> success(new bool(false));

  base::Closure rename_closure = base::Bind(&RenameTemporaryFile,
                                            response_path,
                                            wallpaper_downloaded_file_,
                                            base::Unretained(success.get()));
  base::Closure on_rename_closure =
      base::Bind(&CustomizationWallpaperDownloader::OnTemporaryFileRenamed,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(success.Pass()));
  if (!content::BrowserThread::PostBlockingPoolTaskAndReply(
          FROM_HERE, rename_closure, on_rename_closure)) {
    LOG(WARNING)
        << "Failed to start Customized Wallpaper Rename DownloadedFile.";
    on_wallpaper_fetch_completed_.Run(false, wallpaper_url_);
  }
}

void CustomizationWallpaperDownloader::OnTemporaryFileRenamed(
    scoped_ptr<bool> success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  on_wallpaper_fetch_completed_.Run(*success, wallpaper_url_);
}

}  //   namespace chromeos
