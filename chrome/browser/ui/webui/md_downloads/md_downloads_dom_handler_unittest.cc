// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_downloads/md_downloads_dom_handler.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestMdDownloadsDOMHandler : public MdDownloadsDOMHandler {
 public:
  explicit TestMdDownloadsDOMHandler(content::DownloadManager* download_manager,
                                     content::WebUI* web_ui)
      : MdDownloadsDOMHandler(download_manager, web_ui) {}

  using MdDownloadsDOMHandler::set_web_ui;
};

}  // namespace

// A fixture to test MdDownloadsDOMHandler.
class MdDownloadsDOMHandlerTest : public testing::Test {
 public:
  // testing::Test:
  void SetUp() override {
    ON_CALL(manager_, GetBrowserContext())
        .WillByDefault(testing::Return(&profile_));
  }

  TestingProfile* profile() { return &profile_; }
  content::MockDownloadManager* manager() { return &manager_; }
  content::TestWebUI* web_ui() { return &web_ui_; }

 private:
  // NOTE: The initialization order of these members matters.
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;

  testing::NiceMock<content::MockDownloadManager> manager_;
  content::TestWebUI web_ui_;
};

TEST_F(MdDownloadsDOMHandlerTest, ChecksForRemovedFiles) {
  EXPECT_CALL(*manager(), CheckForHistoryFilesRemoval());
  TestMdDownloadsDOMHandler handler(manager(), web_ui());

  testing::Mock::VerifyAndClear(manager());

  EXPECT_CALL(*manager(), CheckForHistoryFilesRemoval());
  handler.OnJavascriptDisallowed();
}

TEST_F(MdDownloadsDOMHandlerTest, HandleGetDownloads) {
  TestMdDownloadsDOMHandler handler(manager(), web_ui());
  handler.set_web_ui(web_ui());

  base::ListValue empty_search_terms;
  handler.HandleGetDownloads(&empty_search_terms);

  EXPECT_EQ(1U, web_ui()->call_data().size());
  EXPECT_EQ("downloads.Manager.insertItems",
            web_ui()->call_data()[0]->function_name());
}
