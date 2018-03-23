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
#include "components/download/public/common/download_job_impl.h"
#include "components/download/public/common/download_worker.h"
#include "content/common/content_export.h"

namespace content {

// DownloadJob that can create concurrent range requests to fetch different
// parts of the file.
// The original request is hold in base class.
class CONTENT_EXPORT ParallelDownloadJob
    : public download::DownloadJobImpl,
      public download::DownloadWorker::Delegate {
 public:
  ParallelDownloadJob(
      download::DownloadItem* download_item,
      std::unique_ptr<download::DownloadRequestHandleInterface> request_handle,
      const download::DownloadCreateInfo& create_info,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);
  ~ParallelDownloadJob() override;

  // DownloadJobImpl implementation.
  void Cancel(bool user_cancel) override;
  void Pause() override;
  void Resume(bool resume_request) override;
  void CancelRequestWithOffset(int64_t offset) override;

 protected:
  // DownloadJobImpl implementation.
  void OnDownloadFileInitialized(
      download::DownloadFile::InitializeCallback callback,
      download::DownloadInterruptReason result,
      int64_t bytes_wasted) override;

  // Virtual for testing.
  virtual int GetParallelRequestCount() const;
  virtual int64_t GetMinSliceSize() const;
  virtual int GetMinRemainingTimeInSeconds() const;

  using WorkerMap =
      std::unordered_map<int64_t, std::unique_ptr<download::DownloadWorker>>;

  // Map from the offset position of the slice to the worker that downloads the
  // slice.
  WorkerMap workers_;

 private:
  friend class ParallelDownloadJobTest;

  // DownloadWorker::Delegate implementation.
  void OnInputStreamReady(
      download::DownloadWorker* worker,
      std::unique_ptr<download::InputStream> input_stream) override;

  // Build parallel requests after a delay, to effectively measure the single
  // stream bandwidth.
  void BuildParallelRequestAfterDelay();

  // Build parallel requests to download. This function is the entry point for
  // all parallel downloads.
  void BuildParallelRequests();

  // Build one http request for each slice from the second slice.
  // The first slice represents the original request.
  void ForkSubRequests(
      const download::DownloadItem::ReceivedSlices& slices_to_download);

  // Create one range request, virtual for testing. Range request will start
  // from |offset| to |length|. Range request will be half open, e.g.
  // "Range:50-" if |length| is 0.
  virtual void CreateRequest(int64_t offset, int64_t length);

  // Information about the initial request when download is started.
  int64_t initial_request_offset_;

  // A snapshot of received slices when creating the parallel download job.
  // Download item's received slices may be different from this snapshot when
  // |BuildParallelRequests| is called.
  download::DownloadItem::ReceivedSlices initial_received_slices_;

  // The length of the response body of the original request.
  // Used to estimate the remaining size of the content when the initial
  // request is half open, i.e, |initial_request_length_| is
  // DownloadSaveInfo::kLengthFullContent.
  int64_t content_length_;

  // Used to send parallel requests after a delay based on Finch config.
  base::OneShotTimer timer_;

  // If we have sent parallel requests.
  bool requests_sent_;

  // If the download progress is canceled.
  bool is_canceled_;

  // SharedURLLoaderFactory to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(ParallelDownloadJob);
};

}  //  namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_JOB_H_
