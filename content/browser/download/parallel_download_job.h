// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_JOB_H_
#define CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_JOB_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "content/browser/download/download_job_impl.h"
#include "content/browser/download/download_worker.h"
#include "content/common/content_export.h"

namespace content {

// DownloadJob that can create concurrent range requests to fetch different
// parts of the file.
// The original request is hold in base class DownloadUrlJob.
class CONTENT_EXPORT ParallelDownloadJob : public DownloadJobImpl {
 public:
  ParallelDownloadJob(
      DownloadItemImpl* download_item,
      std::unique_ptr<DownloadRequestHandleInterface> request_handle);
  ~ParallelDownloadJob() override;

  // DownloadUrlJob implementation.
  void Cancel(bool user_cancel) override;
  void Pause() override;
  void Resume(bool resume_request) override;

 private:
  friend class ParallelDownloadJobTest;

  typedef std::vector<std::unique_ptr<DownloadWorker>> WorkerList;

  // Build multiple http requests for a new download,
  // the rest of the bytes starting from |bytes_received| will be equally
  // distributed to each connection, including the original connection.
  // the last connection may take additional bytes.
  void ForkRequestsForNewDownload(int64_t bytes_received, int64_t total_bytes);

  // Create one range request, virtual for testing.
  virtual void CreateRequest(int64_t offset, int64_t length);

  // Number of concurrent requests for a new download, include the original
  // request.
  int request_num_;

  // Subsequent tasks to send range requests.
  WorkerList workers_;

  DISALLOW_COPY_AND_ASSIGN(ParallelDownloadJob);
};

}  //  namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_PARALLEL_DOWNLOAD_JOB_H_
