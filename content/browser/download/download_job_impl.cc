// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_job_impl.h"

#include "content/public/browser/web_contents.h"

namespace content {

DownloadJobImpl::DownloadJobImpl(
    DownloadItemImpl* download_item,
    std::unique_ptr<DownloadRequestHandleInterface> request_handle)
    : DownloadJob(download_item), request_handle_(std::move(request_handle)) {}

DownloadJobImpl::~DownloadJobImpl() = default;

void DownloadJobImpl::Start() {
  DownloadJob::StartDownload();
}

void DownloadJobImpl::Cancel(bool user_cancel) {
  if (request_handle_)
    request_handle_->CancelRequest();
}

void DownloadJobImpl::Pause() {
  DownloadJob::Pause();
  if (request_handle_)
    request_handle_->PauseRequest();
}

void DownloadJobImpl::Resume(bool resume_request) {
  DownloadJob::Resume(resume_request);
  if (!resume_request)
    return;

  if (request_handle_)
    request_handle_->ResumeRequest();
}

WebContents* DownloadJobImpl::GetWebContents() const {
  return request_handle_ ? request_handle_->GetWebContents() : nullptr;
}

}  // namespace content
