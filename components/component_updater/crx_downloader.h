// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_CRX_DOWNLOADER_H_
#define COMPONENTS_COMPONENT_UPDATER_CRX_DOWNLOADER_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

namespace component_updater {

// Defines a download interface for downloading components, with retrying on
// fallback urls in case of errors. This class implements a chain of
// responsibility design pattern. It can give successors in the chain a chance
// to handle a download request, until one of them succeeds, or there are no
// more urls or successors to try. A callback is always called at the end of
// the download, one time only.
// When multiple urls and downloaders exists, first all the urls are tried, in
// the order they are provided in the StartDownload function argument. After
// that, the download request is routed to the next downloader in the chain.
// The members of this class expect to be called from the main thread only.
class CrxDownloader {
 public:
  struct DownloadMetrics {
    enum Downloader { kNone = 0, kUrlFetcher, kBits };

    DownloadMetrics();

    GURL url;

    Downloader downloader;

    int error;

    int64_t downloaded_bytes;  // -1 means that the byte count is unknown.
    int64_t total_bytes;

    uint64_t download_time_ms;
  };

  // Contains the progress or the outcome of the download.
  struct Result {
    Result();

    // Download error: 0 indicates success.
    int error;

    // Path of the downloaded file if the download was successful.
    base::FilePath response;

    // Number of bytes actually downloaded, not including the bytes downloaded
    // as a result of falling back on urls.
    int64_t downloaded_bytes;

    // Number of bytes expected to be downloaded.
    int64_t total_bytes;
  };

  // The callback fires only once, regardless of how many urls are tried, and
  // how many successors in the chain of downloaders have handled the
  // download. The callback interface can be extended if needed to provide
  // more visibility into how the download has been handled, including
  // specific error codes and download metrics.
  typedef base::Callback<void(const Result& result)> DownloadCallback;

  // The callback may fire 0 or many times during a download. Since this
  // class implements a chain of responsibility, the callback can fire for
  // different urls and different downloaders. The number of actual downloaded
  // bytes is not guaranteed to monotonically increment over time.
  typedef base::Callback<void(const Result& result)> ProgressCallback;

  // Factory method to create an instance of this class and build the
  // chain of responsibility. |is_background_download| specifies that a
  // background downloader be used, if the platform supports it.
  // |url_fetcher_task_runner| should be an IO capable task runner able to
  // support UrlFetcherDownloader. |background_task_runner| should be an
  // IO capable thread able to support BackgroundDownloader.
  static CrxDownloader* Create(
      bool is_background_download,
      net::URLRequestContextGetter* context_getter,
      scoped_refptr<base::SequencedTaskRunner> url_fetcher_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner);
  virtual ~CrxDownloader();

  void set_progress_callback(const ProgressCallback& progress_callback);

  // Starts the download. One instance of the class handles one download only.
  // One instance of CrxDownloader can only be started once, otherwise the
  // behavior is undefined. The callback gets invoked if the download can't
  // be started.
  void StartDownloadFromUrl(const GURL& url,
                            const DownloadCallback& download_callback);
  void StartDownload(const std::vector<GURL>& urls,
                     const DownloadCallback& download_callback);

  const std::vector<DownloadMetrics> download_metrics() const;

 protected:
  explicit CrxDownloader(scoped_ptr<CrxDownloader> successor);

  // Handles the fallback in the case of multiple urls and routing of the
  // download to the following successor in the chain. Derived classes must call
  // this function after each attempt at downloading the urls provided
  // in the StartDownload function.
  // In case of errors, |is_handled| indicates that a server side error has
  // occured for the current url and the url should not be retried down
  // the chain to avoid DDOS of the server. This url will be removed from the
  // list of url and never tried again.
  void OnDownloadComplete(bool is_handled,
                          const Result& result,
                          const DownloadMetrics& download_metrics);

  // Calls the callback when progress is made.
  void OnDownloadProgress(const Result& result);

  // Returns the url which is currently being downloaded from.
  GURL url() const;

 private:
  virtual void DoStartDownload(const GURL& url) = 0;

  base::ThreadChecker thread_checker_;

  std::vector<GURL> urls_;
  scoped_ptr<CrxDownloader> successor_;
  DownloadCallback download_callback_;
  ProgressCallback progress_callback_;

  std::vector<GURL>::iterator current_url_;

  std::vector<DownloadMetrics> download_metrics_;

  DISALLOW_COPY_AND_ASSIGN(CrxDownloader);
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_CRX_DOWNLOADER_H_
