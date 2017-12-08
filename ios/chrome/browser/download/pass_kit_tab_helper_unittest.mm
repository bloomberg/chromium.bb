// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/pass_kit_tab_helper.h"

#include <memory>

#import <PassKit/PassKit.h>

#include "ios/chrome/browser/download/pass_kit_mime_type.h"
#include "ios/chrome/browser/download/pass_kit_test_util.h"
#import "ios/chrome/test/fakes/fake_pass_kit_tab_helper_delegate.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
char kUrl[] = "https://test.test/";

// Used as no-op callback.
void DoNothing(int) {}
}  // namespace

// Test fixture for testing PassKitTabHelper class.
class PassKitTabHelperTest : public PlatformTest {
 protected:
  PassKitTabHelperTest()
      : delegate_([[FakePassKitTabHelperDelegate alloc]
            initWithWebState:&web_state_]) {
    PassKitTabHelper::CreateForWebState(&web_state_, delegate_);
  }

  PassKitTabHelper* tab_helper() {
    return PassKitTabHelper::FromWebState(&web_state_);
  }

  web::TestWebState web_state_;
  FakePassKitTabHelperDelegate* delegate_;
};

// Tests downloading empty pkpass file.
TEST_F(PassKitTabHelperTest, EmptyFile) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  task_ptr->SetDone(true);
  EXPECT_EQ(1U, delegate_.passes.count);
  EXPECT_TRUE([delegate_.passes.firstObject isKindOfClass:[NSNull class]]);
}

// Tests downloading 2 empty pkpass files.
TEST_F(PassKitTabHelperTest, MultipleEmptyFiles) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));

  auto task2 =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  web::FakeDownloadTask* task_ptr2 = task2.get();
  tab_helper()->Download(std::move(task2));

  task_ptr->SetDone(true);
  EXPECT_EQ(1U, delegate_.passes.count);
  EXPECT_TRUE([delegate_.passes.firstObject isKindOfClass:[NSNull class]]);

  task_ptr2->SetDone(true);
  EXPECT_EQ(2U, delegate_.passes.count);
  EXPECT_TRUE([delegate_.passes.lastObject isKindOfClass:[NSNull class]]);
}

// Tests downloading a valid pkpass file.
TEST_F(PassKitTabHelperTest, ValidPassKitFile) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));

  std::string pass_data = testing::GetTestPass();
  auto buffer = base::MakeRefCounted<net::IOBuffer>(pass_data.size());
  memcpy(buffer->data(), pass_data.c_str(), pass_data.size());
  // Writing to URLFetcherStringWriter, which is used by PassKitTabHelper is
  // synchronous, so it's ok to ignore Write's completion callback.
  task_ptr->GetResponseWriter()->Write(buffer.get(), pass_data.size(),
                                       base::BindRepeating(&DoNothing));
  task_ptr->SetDone(true);

  EXPECT_EQ(1U, delegate_.passes.count);
  PKPass* pass = delegate_.passes.firstObject;
  EXPECT_TRUE([pass isKindOfClass:[PKPass class]]);
  EXPECT_EQ(PKPassTypeBarcode, pass.passType);
  EXPECT_NSEQ(@"pass.com.apple.devpubs.example", pass.passTypeIdentifier);
  EXPECT_NSEQ(@"Toy Town", pass.organizationName);
}
