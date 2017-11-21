// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_controller_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/testing/wait_util.h"
#include "ios/web/public/test/fakes/fake_download_controller_delegate.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/test/web_test.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {
const char kContentDisposition[] = "attachment; filename=file.test";
const char kMimeType[] = "application/pdf";
}  //  namespace

// Test fixture for testing DownloadControllerImpl class.
class DownloadControllerImplTest : public WebTest {
 protected:
  DownloadControllerImplTest() : delegate_(download_controller()) {
    web_state_.SetBrowserState(GetBrowserState());
  }

  DownloadController* download_controller() {
    return DownloadController::FromBrowserState(GetBrowserState());
  }

  TestWebState web_state_;
  FakeDownloadControllerDelegate delegate_;
};

// Tests that DownloadController::FromBrowserState returns the same object for
// each call.
TEST_F(DownloadControllerImplTest, FromBrowserState) {
  DownloadController* controller = download_controller();
  ASSERT_TRUE(controller);
  ASSERT_EQ(controller, download_controller());
}

// Tests that DownloadController::CreateDownloadTask calls
// DownloadControllerDelegate::OnDownloadCreated.
TEST_F(DownloadControllerImplTest, OnDownloadCreated) {
  NSString* identifier = [NSUUID UUID].UUIDString;
  GURL url("https://download.test");
  download_controller()->CreateDownloadTask(&web_state_, identifier, url,
                                            kContentDisposition,
                                            /*total_bytes=*/-1, kMimeType);

  ASSERT_EQ(1U, delegate_.alive_download_tasks().size());
  DownloadTask* task = delegate_.alive_download_tasks()[0].second.get();
  EXPECT_EQ(&web_state_, delegate_.alive_download_tasks()[0].first);
  EXPECT_NSEQ(identifier, task->GetIndentifier());
  EXPECT_EQ(url, task->GetOriginalUrl());
  EXPECT_FALSE(task->IsDone());
  EXPECT_EQ(0, task->GetErrorCode());
  EXPECT_EQ(-1, task->GetTotalBytes());
  EXPECT_EQ(-1, task->GetPercentComplete());
  EXPECT_EQ(kContentDisposition, task->GetContentDisposition());
  EXPECT_EQ(kMimeType, task->GetMimeType());
  EXPECT_EQ("file.test", base::UTF16ToUTF8(task->GetSuggestedFilename()));
}

// Tests that DownloadController::FromBrowserState does not crash if used
// without delegate.
TEST_F(DownloadControllerImplTest, NullDelegate) {
  download_controller()->SetDelegate(nullptr);
  GURL url("https://download.test");
  download_controller()->CreateDownloadTask(
      &web_state_, [NSUUID UUID].UUIDString, url, kContentDisposition,
      /*total_bytes=*/-1, kMimeType);
}

}  // namespace web
