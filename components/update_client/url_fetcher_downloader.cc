// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/url_fetcher_downloader.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/update_client/network.h"
#include "components/update_client/utils.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

constexpr base::TaskTraits kTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

}  // namespace

namespace update_client {

UrlFetcherDownloader::UrlFetcherDownloader(
    std::unique_ptr<CrxDownloader> successor,
    scoped_refptr<NetworkFetcherFactory> network_fetcher_factory)
    : CrxDownloader(std::move(successor)),
      network_fetcher_factory_(network_fetcher_factory) {}

UrlFetcherDownloader::~UrlFetcherDownloader() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void UrlFetcherDownloader::DoStartDownload(const GURL& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::PostTaskWithTraitsAndReply(
      FROM_HERE, kTaskTraits,
      base::BindOnce(&UrlFetcherDownloader::CreateDownloadDir,
                     base::Unretained(this)),
      base::BindOnce(&UrlFetcherDownloader::StartURLFetch,
                     base::Unretained(this), url));
}

void UrlFetcherDownloader::CreateDownloadDir() {
  base::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_url_fetcher_"),
                               &download_dir_);
}

void UrlFetcherDownloader::StartURLFetch(const GURL& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (download_dir_.empty()) {
    Result result;
    result.error = -1;

    DownloadMetrics download_metrics;
    download_metrics.url = url;
    download_metrics.downloader = DownloadMetrics::kUrlFetcher;
    download_metrics.error = -1;
    download_metrics.downloaded_bytes = -1;
    download_metrics.total_bytes = -1;
    download_metrics.download_time_ms = 0;

    main_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&UrlFetcherDownloader::OnDownloadComplete,
                                  base::Unretained(this), false, result,
                                  download_metrics));
    return;
  }

  const base::FilePath response =
      download_dir_.AppendASCII(url.ExtractFileName());
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->load_flags = net::LOAD_DO_NOT_SEND_COOKIES |
                                 net::LOAD_DO_NOT_SAVE_COOKIES |
                                 net::LOAD_DISABLE_CACHE;
  network_fetcher_ =
      network_fetcher_factory_->Create(std::move(resource_request));

  const int kMaxRetries = 3;
  network_fetcher_->SetRetryOptions(
      kMaxRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);

  network_fetcher_->SetOnResponseStartedCallback(base::BindOnce(
      &UrlFetcherDownloader::OnResponseStarted, base::Unretained(this)));

  // For the end-to-end system it is important that the client reports the
  // number of bytes it loaded from the server even in the case that the
  // overall network transaction failed.
  network_fetcher_->SetAllowPartialResults(true);

  VLOG(1) << "Starting background download: " << url.spec();
  network_fetcher_->DownloadToFile(
      base::BindOnce(&UrlFetcherDownloader::OnNetworkFetcherComplete,
                     base::Unretained(this)),
      response);

  download_start_time_ = base::TimeTicks::Now();
}

void UrlFetcherDownloader::OnNetworkFetcherComplete(base::FilePath file_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const base::TimeTicks download_end_time(base::TimeTicks::Now());
  const base::TimeDelta download_time =
      download_end_time >= download_start_time_
          ? download_end_time - download_start_time_
          : base::TimeDelta();

  // Consider a 5xx response from the server as an indication to terminate
  // the request and avoid overloading the server in this case.
  // is not accepting requests for the moment.
  int response_code = -1;
  if (network_fetcher_->ResponseInfo() &&
      network_fetcher_->ResponseInfo()->headers) {
    response_code = network_fetcher_->ResponseInfo()->headers->response_code();
  }

  int fetch_error = -1;
  if (!file_path.empty() && response_code == 200) {
    fetch_error = 0;
  } else if (response_code != -1) {
    fetch_error = response_code;
  } else {
    fetch_error = network_fetcher_->NetError();
  }

  const bool is_handled = fetch_error == 0 || IsHttpServerError(fetch_error);

  Result result;
  result.error = fetch_error;
  if (!fetch_error) {
    result.response = file_path;
  }

  DownloadMetrics download_metrics;
  download_metrics.url = url();
  download_metrics.downloader = DownloadMetrics::kUrlFetcher;
  download_metrics.error = fetch_error;
  // Tests expected -1, in case of failures and no content is available.
  download_metrics.downloaded_bytes =
      fetch_error && !network_fetcher_->GetContentSize()
          ? -1
          : network_fetcher_->GetContentSize();
  download_metrics.total_bytes = total_bytes_;
  download_metrics.download_time_ms = download_time.InMilliseconds();

  VLOG(1) << "Downloaded " << network_fetcher_->GetContentSize() << " bytes in "
          << download_time.InMilliseconds() << "ms from "
          << network_fetcher_->GetFinalURL().spec() << " to "
          << result.response.value();

  // Delete the download directory in the error cases.
  if (fetch_error && !download_dir_.empty())
    base::PostTaskWithTraits(
        FROM_HERE, kTaskTraits,
        base::BindOnce(IgnoreResult(&base::DeleteFile), download_dir_, true));

  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&UrlFetcherDownloader::OnDownloadComplete,
                                base::Unretained(this), is_handled, result,
                                download_metrics));
}

// This callback is used to indicate that a download has been started.
void UrlFetcherDownloader::OnResponseStarted(
    const GURL& final_url,
    const network::ResourceResponseHead& response_head) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (response_head.content_length != -1)
    total_bytes_ = response_head.content_length;

  OnDownloadProgress();
}

}  // namespace update_client
