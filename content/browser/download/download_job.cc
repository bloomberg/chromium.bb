// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_job.h"

#include "base/bind_helpers.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_item_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

DownloadJob::DownloadJob(DownloadItemImpl* download_item)
    : download_item_(download_item), is_paused_(false) {}

DownloadJob::~DownloadJob() = default;

void DownloadJob::Pause() {
  is_paused_ = true;
}

void DownloadJob::Resume(bool resume_request) {
  is_paused_ = false;
}

void DownloadJob::StartDownload() const {
  download_item_->StartDownload();
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

}  // namespace content
