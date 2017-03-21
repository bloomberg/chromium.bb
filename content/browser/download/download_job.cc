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

void DownloadJob::Interrupt(DownloadInterruptReason reason) {
  download_item_->InterruptAndDiscardPartialState(reason);
  download_item_->UpdateObservers();
}

void DownloadJob::AddByteStream(std::unique_ptr<ByteStreamReader> stream_reader,
                                int64_t offset,
                                int64_t length) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DownloadFile* download_file = download_item_->download_file_.get();
  if (!download_file) {
    // TODO(xingliu): Handle early and late incoming streams, see crbug/702683.
    VLOG(1) << "Download file is released when byte stream arrived. Discard "
               "the byte stream.";
    return;
  }

  // download_file_ is owned by download_item_ on the UI thread and is always
  // deleted on the FILE thread after download_file_ is nulled out.
  // So it's safe to use base::Unretained here.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFile::AddByteStream, base::Unretained(download_file),
                 base::Passed(&stream_reader), offset, length));
}

}  // namespace content
