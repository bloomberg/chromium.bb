// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_job.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/download/public/common/download_task_runner.h"
#include "content/browser/download/download_item_impl.h"

namespace content {

DownloadJob::DownloadJob(
    DownloadItemImpl* download_item,
    std::unique_ptr<download::DownloadRequestHandleInterface> request_handle)
    : download_item_(download_item),
      request_handle_(std::move(request_handle)),
      is_paused_(false),
      weak_ptr_factory_(this) {}

DownloadJob::~DownloadJob() = default;

void DownloadJob::Cancel(bool user_cancel) {
  if (request_handle_)
    request_handle_->CancelRequest(user_cancel);
}

void DownloadJob::Pause() {
  is_paused_ = true;

  download::DownloadFile* download_file = download_item_->download_file_.get();
  if (download_file) {
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&download::DownloadFile::Pause,
                       // Safe because we control download file lifetime.
                       base::Unretained(download_file)));
  }
  if (request_handle_)
    request_handle_->PauseRequest();
}

void DownloadJob::Resume(bool resume_request) {
  is_paused_ = false;
  if (!resume_request)
    return;

  download::DownloadFile* download_file = download_item_->download_file_.get();
  if (download_file) {
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&download::DownloadFile::Resume,
                       // Safe because we control download file lifetime.
                       base::Unretained(download_file)));
  }

  if (request_handle_)
    request_handle_->ResumeRequest();
}

void DownloadJob::Start(
    download::DownloadFile* download_file_,
    download::DownloadFile::InitializeCallback callback,
    const download::DownloadItem::ReceivedSlices& received_slices) {
  download::GetDownloadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &download::DownloadFile::Initialize,
          // Safe because we control download file lifetime.
          base::Unretained(download_file_),
          base::BindRepeating(&DownloadJob::OnDownloadFileInitialized,
                              weak_ptr_factory_.GetWeakPtr(), callback),
          base::BindRepeating(&DownloadJob::CancelRequestWithOffset,
                              weak_ptr_factory_.GetWeakPtr()),
          received_slices, IsParallelizable()));
}

void DownloadJob::OnDownloadFileInitialized(
    download::DownloadFile::InitializeCallback callback,
    download::DownloadInterruptReason result,
    int64_t bytes_wasted) {
  std::move(callback).Run(result, bytes_wasted);
}

bool DownloadJob::AddInputStream(std::unique_ptr<download::InputStream> stream,
                                 int64_t offset,
                                 int64_t length) {
  download::DownloadFile* download_file = download_item_->download_file_.get();
  if (!download_file) {
    CancelRequestWithOffset(offset);
    return false;
  }

  // download_file_ is owned by download_item_ on the UI thread and is always
  // deleted on the download task runner after download_file_ is nulled out.
  // So it's safe to use base::Unretained here.
  download::GetDownloadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&download::DownloadFile::AddInputStream,
                                base::Unretained(download_file),
                                std::move(stream), offset, length));
  return true;
}

void DownloadJob::CancelRequestWithOffset(int64_t offset) {
}

bool DownloadJob::IsParallelizable() const {
  return false;
}

bool DownloadJob::IsSavePackageDownload() const {
  return false;
}

}  // namespace content
