// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/attachments/fake_attachment_downloader.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/sync/model/attachments/attachment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class FakeAttachmentDownloaderTest : public testing::Test {
 protected:
  FakeAttachmentDownloaderTest() {}

  void SetUp() override {}

  void TearDown() override {}

  AttachmentDownloader* downloader() { return &attachment_downloader_; }

  AttachmentDownloader::DownloadCallback download_callback() {
    return base::Bind(&FakeAttachmentDownloaderTest::DownloadDone,
                      base::Unretained(this));
  }

  const std::vector<AttachmentDownloader::DownloadResult>& download_results() {
    return download_results_;
  }

  void RunMessageLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 private:
  void DownloadDone(const AttachmentDownloader::DownloadResult& result,
                    std::unique_ptr<Attachment> attachment) {
    download_results_.push_back(result);
  }

  base::MessageLoop message_loop_;
  FakeAttachmentDownloader attachment_downloader_;
  std::vector<AttachmentDownloader::DownloadResult> download_results_;
};

TEST_F(FakeAttachmentDownloaderTest, DownloadAttachment) {
  AttachmentId attachment_id = AttachmentId::Create(0, 0);
  downloader()->DownloadAttachment(attachment_id, download_callback());
  RunMessageLoop();
  EXPECT_EQ(1U, download_results().size());
  EXPECT_EQ(AttachmentDownloader::DOWNLOAD_SUCCESS, download_results()[0]);
}

}  // namespace syncer
