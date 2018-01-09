// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/download_manager_tab_helper.h"

#import "ios/chrome/test/fakes/fake_download_manager_tab_helper_delegate.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
char kUrl[] = "https://test.test/";
const char kMimeType[] = "";
}

// Test fixture for testing DownloadManagerTabHelper class.
class DownloadManagerTabHelperTest : public PlatformTest {
 protected:
  DownloadManagerTabHelperTest()
      : delegate_([[FakeDownloadManagerTabHelperDelegate alloc] init]) {
    DownloadManagerTabHelper::CreateForWebState(&web_state_, delegate_);
  }

  DownloadManagerTabHelper* tab_helper() {
    return DownloadManagerTabHelper::FromWebState(&web_state_);
  }

  web::TestWebState web_state_;
  FakeDownloadManagerTabHelperDelegate* delegate_;
};

// Tests that created download has NotStarted state.
TEST_F(DownloadManagerTabHelperTest, DownloadCreation) {
  ASSERT_FALSE(delegate_.state);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);

  tab_helper()->Download(std::move(task));

  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, *delegate_.state);
}

// Tests that "done" download has Complete state.
TEST_F(DownloadManagerTabHelperTest, DownloadUpdate) {
  ASSERT_FALSE(delegate_.state);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);

  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  task_ptr->SetDone(true);

  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kComplete, *delegate_.state);
}

// Tests hiding and showing WebState.
TEST_F(DownloadManagerTabHelperTest, HideAndShowWebState) {
  ASSERT_FALSE(delegate_.state);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);
  tab_helper()->Download(std::move(task));
  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, *delegate_.state);

  web_state_.WasHidden();
  EXPECT_FALSE(delegate_.state);

  web_state_.WasShown();
  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, *delegate_.state);
}
