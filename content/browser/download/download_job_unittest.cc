// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/browser/download/download_job.h"
#include "content/browser/download/mock_download_item_impl.h"
#include "content/browser/download/mock_download_job.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

namespace content {

// Test for DownloadJob base class functionalities.
class DownloadJobTest : public testing::Test {
 public:
  DownloadJobTest() = default;
  ~DownloadJobTest() override = default;

  void SetUp() override {
    item_delegate_ = base::MakeUnique<DownloadItemImplDelegate>();
    download_item_ =
        base::MakeUnique<NiceMock<MockDownloadItemImpl>>(item_delegate_.get());
    download_job_ = base::MakeUnique<MockDownloadJob>(download_item_.get());
  }

  content::TestBrowserThreadBundle browser_threads_;
  std::unique_ptr<DownloadItemImplDelegate> item_delegate_;
  std::unique_ptr<MockDownloadItemImpl> download_item_;
  std::unique_ptr<MockDownloadJob> download_job_;
};

TEST_F(DownloadJobTest, Pause) {
  DCHECK(download_job_->download_item());
}

}  // namespace content
