// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_JOB_H_
#define CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_JOB_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "content/browser/download/download_job_impl.h"
#include "content/browser/download/download_worker.h"
#include "content/common/content_export.h"

namespace content {

// DownloadJob that can create concurrent range requests to fetch different
// parts of the file.
// The original request is hold in base class.
class CONTENT_EXPORT ParallelDownloadJob : public DownloadJobImpl,
                                           public DownloadWorker::Delegate {
 public:
  ParallelDownloadJob(
      DownloadItemImpl* download_item,
      std::unique_ptr<DownloadRequestHandleInterface> request_handle,
      const DownloadCreateInfo& create_info);
  ~ParallelDownloadJob() override;

  // DownloadJobImpl implementation.
  void Start() override;
  void Cancel(bool user_cancel) override;
  void Pause() override;
  void Resume(bool resume_request) override;
  bool UsesParallelRequests() const override;

 protected:
  // Virtual for testing.
  virtual int GetParallelRequestCount() const;

 private:
  friend class ParallelDownloadJobTest;

  using WorkerMap =
      std::unordered_map<int64_t, std::unique_ptr<DownloadWorker>>;

  // DownloadWorker::Delegate implementation.
  void OnByteStreamReady(
      DownloadWorker* worker,
      std::unique_ptr<ByteStreamReader> stream_reader) override;
  void OnServerResponseError(DownloadWorker* worker,
                             DownloadInterruptReason reason) override;

  // Build parallel requests after a delay, to effectively measure the single
  // stream bandwidth.
  void BuildParallelRequestAfterDelay();

  // Build parallel requests to download. This function is the entry point for
  // all parallel downloads.
  void BuildParallelRequests();

  // Build one http request for each slice from the second slice.
  // The first slice represents the original request.
  void ForkSubRequests(const DownloadItem::ReceivedSlices& slices_to_download);

  // Create one range request, virtual for testing.
  virtual void CreateRequest(int64_t offset, int64_t length);

  // Information about the initial request when download is started.
  int64_t initial_request_offset_;

  // The length of the response body of the original request. May be less than
  // the size of the target file if the request starts from non-zero offset.
  int64_t content_length_;

  // Map from the offset position of the slice to the worker that downloads the
  // slice.
  WorkerMap workers_;

  // Used to send parallel requests after a delay based on Finch config.
  base::OneShotTimer timer_;

  // If we have sent parallel requests.
  bool requests_sent_;

  DISALLOW_COPY_AND_ASSIGN(ParallelDownloadJob);
};

}  //  namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_JOB_H_
