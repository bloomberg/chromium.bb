// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/url_fetcher_downloader.h"

#include "chrome/browser/component_updater/component_updater_utils.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace component_updater {

UrlFetcherDownloader::UrlFetcherDownloader(
    scoped_ptr<CrxDownloader> successor,
    net::URLRequestContextGetter* context_getter,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const DownloadCallback& download_callback)
    : CrxDownloader(successor.Pass(), download_callback),
      context_getter_(context_getter),
      task_runner_(task_runner),
      downloaded_bytes_(-1),
      total_bytes_(-1) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

UrlFetcherDownloader::~UrlFetcherDownloader() {}

void UrlFetcherDownloader::DoStartDownload(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  url_fetcher_.reset(net::URLFetcher::Create(0,
                                             url,
                                             net::URLFetcher::GET,
                                             this));
  url_fetcher_->SetRequestContext(context_getter_);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DISABLE_CACHE);
  url_fetcher_->SetAutomaticallyRetryOn5xx(false);
  url_fetcher_->SaveResponseToTemporaryFile(task_runner_);

  url_fetcher_->Start();

  download_start_time_ = base::Time::Now();

  downloaded_bytes_ = -1;
  total_bytes_ = -1;
}

void UrlFetcherDownloader::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const base::Time download_end_time(base::Time::Now());
  const base::TimeDelta download_time =
    download_end_time >= download_start_time_ ?
    download_end_time - download_start_time_ : base::TimeDelta();

  // Consider a 5xx response from the server as an indication to terminate
  // the request and avoid overloading the server in this case.
  // is not accepting requests for the moment.
  const int fetch_error(GetFetchError(*url_fetcher_));
  const bool is_handled = fetch_error == 0 || IsHttpServerError(fetch_error);

  Result result;
  result.error = fetch_error;
  if (!fetch_error) {
    source->GetResponseAsFilePath(true, &result.response);
  }

  DownloadMetrics download_metrics;
  download_metrics.url = url();
  download_metrics.downloader = DownloadMetrics::kUrlFetcher;
  download_metrics.error = fetch_error;
  download_metrics.bytes_downloaded = downloaded_bytes_;
  download_metrics.bytes_total = total_bytes_;
  download_metrics.download_time_ms = download_time.InMilliseconds();

  CrxDownloader::OnDownloadComplete(is_handled, result, download_metrics);
}

void UrlFetcherDownloader::OnURLFetchDownloadProgress(
    const net::URLFetcher* source,
    int64 current,
    int64 total) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  downloaded_bytes_ = current;
  total_bytes_ = total;
}

}  // namespace component_updater

