// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_job.h"

#include "base/bind_helpers.h"
#include "content/browser/download/download_item_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace content {

DownloadJob::DownloadJob(
    DownloadItemImpl* download_item,
    std::unique_ptr<DownloadRequestHandleInterface> request_handle)
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
  if (request_handle_)
    request_handle_->PauseRequest();
  is_paused_ = true;
}

void DownloadJob::Resume(bool resume_request) {
  is_paused_ = false;
  if (!resume_request)
    return;

  if (request_handle_)
    request_handle_->ResumeRequest();
}

WebContents* DownloadJob::GetWebContents() const {
  return request_handle_ ? request_handle_->GetWebContents() : nullptr;
}

void DownloadJob::Start(DownloadFile* download_file_,
                        const DownloadFile::InitializeCallback& callback,
                        const DownloadItem::ReceivedSlices& received_slices) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFile::Initialize,
                 // Safe because we control download file lifetime.
                 base::Unretained(download_file_),
                 base::Bind(&DownloadJob::OnDownloadFileInitialized,
                            weak_ptr_factory_.GetWeakPtr(), callback),
                 base::Bind(&DownloadJob::CancelRequestWithOffset,
                            weak_ptr_factory_.GetWeakPtr()),
                 received_slices, IsParallelizable()));
}

void DownloadJob::OnDownloadFileInitialized(
    const DownloadFile::InitializeCallback& callback,
    DownloadInterruptReason result) {
  callback.Run(result);
}

bool DownloadJob::AddByteStream(std::unique_ptr<ByteStreamReader> stream_reader,
                                int64_t offset,
                                int64_t length) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DownloadFile* download_file = download_item_->download_file_.get();
  if (!download_file)
    return false;

  // download_file_ is owned by download_item_ on the UI thread and is always
  // deleted on the FILE thread after download_file_ is nulled out.
  // So it's safe to use base::Unretained here.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFile::AddByteStream, base::Unretained(download_file),
                 base::Passed(&stream_reader), offset, length));
  return true;
}

void DownloadJob::CancelRequestWithOffset(int64_t offset) {
  NOTREACHED();
}

bool DownloadJob::IsParallelizable() const {
  return false;
}

bool DownloadJob::IsSavePackageDownload() const {
  return false;
}

}  // namespace content
