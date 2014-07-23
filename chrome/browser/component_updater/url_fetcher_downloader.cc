// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/url_fetcher_downloader.h"

#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/component_updater/component_updater_utils.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

namespace component_updater {

UrlFetcherDownloader::UrlFetcherDownloader(
    scoped_ptr<CrxDownloader> successor,
    net::URLRequestContextGetter* context_getter,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : CrxDownloader(successor.Pass()),
      context_getter_(context_getter),
      task_runner_(task_runner),
      downloaded_bytes_(-1),
      total_bytes_(-1) {
}

UrlFetcherDownloader::~UrlFetcherDownloader() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void UrlFetcherDownloader::DoStartDownload(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  url_fetcher_.reset(
      net::URLFetcher::Create(0, url, net::URLFetcher::GET, this));
  url_fetcher_->SetRequestContext(context_getter_);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DISABLE_CACHE);
  url_fetcher_->SetAutomaticallyRetryOn5xx(false);
  url_fetcher_->SaveResponseToTemporaryFile(task_runner_);

  VLOG(1) << "Starting background download: " << url.spec();
  url_fetcher_->Start();

  download_start_time_ = base::Time::Now();

  downloaded_bytes_ = -1;
  total_bytes_ = -1;
}

void UrlFetcherDownloader::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const base::Time download_end_time(base::Time::Now());
  const base::TimeDelta download_time =
      download_end_time >= download_start_time_
          ? download_end_time - download_start_time_
          : base::TimeDelta();

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
  result.downloaded_bytes = downloaded_bytes_;
  result.total_bytes = total_bytes_;

  DownloadMetrics download_metrics;
  download_metrics.url = url();
  download_metrics.downloader = DownloadMetrics::kUrlFetcher;
  download_metrics.error = fetch_error;
  download_metrics.downloaded_bytes = downloaded_bytes_;
  download_metrics.total_bytes = total_bytes_;
  download_metrics.download_time_ms = download_time.InMilliseconds();

  base::FilePath local_path_;
  source->GetResponseAsFilePath(false, &local_path_);
  VLOG(1) << "Downloaded " << downloaded_bytes_ << " bytes in "
          << download_time.InMilliseconds() << "ms from "
          << source->GetURL().spec() << " to " << local_path_.value();
  CrxDownloader::OnDownloadComplete(is_handled, result, download_metrics);
}

void UrlFetcherDownloader::OnURLFetchDownloadProgress(
    const net::URLFetcher* source,
    int64 current,
    int64 total) {
  DCHECK(thread_checker_.CalledOnValidThread());

  downloaded_bytes_ = current;
  total_bytes_ = total;

  Result result;
  result.downloaded_bytes = downloaded_bytes_;
  result.total_bytes = total_bytes_;

  OnDownloadProgress(result);
}

}  // namespace component_updater
