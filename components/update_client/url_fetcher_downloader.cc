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
#include "components/update_client/utils.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
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
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : CrxDownloader(std::move(successor)),
      url_loader_factory_(std::move(url_loader_factory)) {}

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

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("url_fetcher_downloader", R"(
        semantics {
          sender: "Component Updater"
          description:
            "The component updater in Chrome is responsible for updating code "
            "and data modules such as Flash, CrlSet, Origin Trials, etc. These "
            "modules are updated on cycles independent of the Chrome release "
            "tracks. It runs in the browser process and communicates with a "
            "set of servers using the Omaha protocol to find the latest "
            "versions of components, download them, and register them with the "
            "rest of Chrome."
          trigger: "Manual or automatic software updates."
          data:
            "The URL that refers to a component. It is obfuscated for most "
            "components."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled."
          chrome_policy {
            ComponentUpdatesEnabled {
              policy_options {mode: MANDATORY}
              ComponentUpdatesEnabled: false
            }
          }
        })");

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
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  const int kMaxRetries = 3;
  url_loader_->SetRetryOptions(
      kMaxRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);

  url_loader_->SetOnResponseStartedCallback(base::BindOnce(
      &UrlFetcherDownloader::OnResponseStarted, base::Unretained(this)));

  // For the end-to-end system it is important that the client reports the
  // number of bytes it loaded from the server even in the case that the
  // overall network transaction failed.
  url_loader_->SetAllowPartialResults(true);

  VLOG(1) << "Starting background download: " << url.spec();
  url_loader_->DownloadToFile(
      url_loader_factory_.get(),
      base::BindOnce(&UrlFetcherDownloader::OnURLLoadComplete,
                     base::Unretained(this)),
      response);

  download_start_time_ = base::TimeTicks::Now();
}

void UrlFetcherDownloader::OnURLLoadComplete(base::FilePath file_path) {
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
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  int fetch_error = -1;
  if (!file_path.empty() && response_code == 200) {
    fetch_error = 0;
  } else if (response_code != -1) {
    fetch_error = response_code;
  } else {
    fetch_error = url_loader_->NetError();
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
      fetch_error && !url_loader_->GetContentSize()
          ? -1
          : url_loader_->GetContentSize();
  download_metrics.total_bytes = total_bytes_;
  download_metrics.download_time_ms = download_time.InMilliseconds();

  VLOG(1) << "Downloaded " << url_loader_->GetContentSize() << " bytes in "
          << download_time.InMilliseconds() << "ms from "
          << url_loader_->GetFinalURL().spec() << " to "
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

  // TODO(crbug.com/871211): |Result| is not being used on production.
  // Clean it up.
  OnDownloadProgress(Result());
}

}  // namespace update_client
