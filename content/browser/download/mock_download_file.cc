// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mock_download_file.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Return;

namespace content {
namespace {

void SuccessRun(const DownloadFile::InitializeCallback& initialize_callback) {
  initialize_callback.Run(DOWNLOAD_INTERRUPT_REASON_NONE);
}

void PostSuccessRun(
    const DownloadFile::InitializeCallback& initialize_callback,
    const DownloadFile::CancelRequestCallback& cancel_request_callback,
    const DownloadItem::ReceivedSlices& received_slices,
    bool is_parallelizable) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(&SuccessRun, initialize_callback));
}

}  // namespace

MockDownloadFile::MockDownloadFile() {
  // This is here because |Initialize()| is normally called right after
  // construction.
  ON_CALL(*this, Initialize(_, _, _, _))
      .WillByDefault(::testing::Invoke(PostSuccessRun));
}

MockDownloadFile::~MockDownloadFile() {
}

void MockDownloadFile::AddInputStream(
    std::unique_ptr<DownloadManager::InputStream> input_stream,
    int64_t offset,
    int64_t length) {
  // Gmock currently can't mock method that takes move-only parameters,
  // delegate the EXPECT_CALL count to |DoAddByteStream|.
  DoAddInputStream(input_stream.get(), offset, length);
}

}  // namespace content
