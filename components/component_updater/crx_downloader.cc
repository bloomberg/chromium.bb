// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/crx_downloader.h"

#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "components/component_updater/url_fetcher_downloader.h"

#if defined(OS_WIN)
#include "components/component_updater/background_downloader_win.h"
#endif

namespace component_updater {

CrxDownloader::Result::Result()
    : error(0), downloaded_bytes(-1), total_bytes(-1) {
}

CrxDownloader::DownloadMetrics::DownloadMetrics()
    : downloader(kNone),
      error(0),
      downloaded_bytes(-1),
      total_bytes(-1),
      download_time_ms(0) {
}

// On Windows, the first downloader in the chain is a background downloader,
// which uses the BITS service.
CrxDownloader* CrxDownloader::Create(
    bool is_background_download,
    net::URLRequestContextGetter* context_getter,
    scoped_refptr<base::SequencedTaskRunner> url_fetcher_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> background_task_runner) {
  scoped_ptr<CrxDownloader> url_fetcher_downloader(
      new UrlFetcherDownloader(scoped_ptr<CrxDownloader>().Pass(),
                               context_getter,
                               url_fetcher_task_runner));
#if defined(OS_WIN)
  if (is_background_download) {
    return new BackgroundDownloader(
        url_fetcher_downloader.Pass(), context_getter, background_task_runner);
  }
#endif

  return url_fetcher_downloader.release();
}

CrxDownloader::CrxDownloader(scoped_ptr<CrxDownloader> successor)
    : successor_(successor.Pass()) {
}

CrxDownloader::~CrxDownloader() {
}

void CrxDownloader::set_progress_callback(
    const ProgressCallback& progress_callback) {
  progress_callback_ = progress_callback;
}

GURL CrxDownloader::url() const {
  return current_url_ != urls_.end() ? *current_url_ : GURL();
}

const std::vector<CrxDownloader::DownloadMetrics>
CrxDownloader::download_metrics() const {
  if (!successor_)
    return download_metrics_;

  std::vector<DownloadMetrics> retval(successor_->download_metrics());
  retval.insert(
      retval.begin(), download_metrics_.begin(), download_metrics_.end());
  return retval;
}

void CrxDownloader::StartDownloadFromUrl(
    const GURL& url,
    const DownloadCallback& download_callback) {
  std::vector<GURL> urls;
  urls.push_back(url);
  StartDownload(urls, download_callback);
}

void CrxDownloader::StartDownload(const std::vector<GURL>& urls,
                                  const DownloadCallback& download_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (urls.empty()) {
    // Make a result and complete the download with a generic error for now.
    Result result;
    result.error = -1;
    download_callback.Run(result);
    return;
  }

  // If the urls are mutated while this downloader is active, then the
  // behavior is undefined in the sense that the outcome of the download could
  // be inconsistent for the list of urls. At any rate, the |current_url_| is
  // reset at this point, and the iterator will be valid in all conditions.
  urls_ = urls;
  current_url_ = urls_.begin();
  download_callback_ = download_callback;

  DoStartDownload(*current_url_);
}

void CrxDownloader::OnDownloadComplete(
    bool is_handled,
    const Result& result,
    const DownloadMetrics& download_metrics) {
  DCHECK(thread_checker_.CalledOnValidThread());

  download_metrics_.push_back(download_metrics);

  if (result.error) {
    // If an error has occured, in general try the next url if there is any,
    // then move on to the successor in the chain if there is any successor.
    // If this downloader has received a 5xx error for the current url,
    // as indicated by the |is_handled| flag, remove that url from the list of
    // urls so the url is never retried. In both cases, move on to the
    // next url.
    if (!is_handled) {
      ++current_url_;
    } else {
      current_url_ = urls_.erase(current_url_);
    }

    // Try downloading from another url from the list.
    if (current_url_ != urls_.end()) {
      DoStartDownload(*current_url_);
      return;
    }

    // If there is another downloader that can accept this request, then hand
    // the request over to it so that the successor can try the pruned list
    // of urls. Otherwise, the request ends here since the current downloader
    // has tried all urls and it can't fall back on any other downloader.
    if (successor_ && !urls_.empty()) {
      successor_->StartDownload(urls_, download_callback_);
      return;
    }
  }

  download_callback_.Run(result);
}

void CrxDownloader::OnDownloadProgress(const Result& result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (progress_callback_.is_null())
    return;

  progress_callback_.Run(result);
}

}  // namespace component_updater
