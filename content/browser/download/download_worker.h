// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_WORKER_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_WORKER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/url_downloader.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_url_parameters.h"

namespace content {

// Helper class used to send subsequent range requests to fetch slices of the
// file after handling response of the original non-range request.
// TODO(xingliu): we should consider to reuse this class for single connection
// download.
class CONTENT_EXPORT DownloadWorker : public UrlDownloader::Delegate {
 public:
  class Delegate {
   public:
    // Called when the the byte stream is established after server response is
    // handled. The stream contains data starts from |offset| of the
    // destination file.
    virtual void OnByteStreamReady(
        DownloadWorker* worker,
        std::unique_ptr<ByteStreamReader> stream_reader) = 0;
  };

  DownloadWorker(DownloadWorker::Delegate* delegate,
                 int64_t offset,
                 int64_t length);
  virtual ~DownloadWorker();

  int64_t offset() const { return offset_; }
  int64_t length() const { return length_; }

  // Send network request to ask for a download.
  void SendRequest(std::unique_ptr<DownloadUrlParameters> params);

  // Download operations.
  void Pause();
  void Resume();
  void Cancel();

 private:
  // UrlDownloader::Delegate implementation.
  void OnUrlDownloaderStarted(
      std::unique_ptr<DownloadCreateInfo> create_info,
      std::unique_ptr<ByteStreamReader> stream_reader,
      const DownloadUrlParameters::OnStartedCallback& callback) override;
  void OnUrlDownloaderStopped(UrlDownloader* downloader) override;

  void AddUrlDownloader(
      std::unique_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread>
          downloader);

  DownloadWorker::Delegate* const delegate_;

  // The starting position of the content for this worker to download.
  int64_t offset_;

  // The length of the request. May be 0 to fetch to the end of the file.
  int64_t length_;

  // States of the worker.
  bool is_paused_;
  bool is_canceled_;

  // Used to control the network request. Live on UI thread.
  std::unique_ptr<DownloadRequestHandleInterface> request_handle_;

  // Used to handle the url request. Live and die on IO thread.
  std::unique_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread>
      url_downloader_;

  base::WeakPtrFactory<DownloadWorker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadWorker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_WORKER_H_
