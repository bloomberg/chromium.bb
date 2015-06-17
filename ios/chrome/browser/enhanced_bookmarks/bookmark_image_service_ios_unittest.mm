// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enhanced_bookmarks/bookmark_image_service_ios.h"

#import <UIKit/UIKit.h>

#include "base/files/file_path.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_model.h"
#include "components/enhanced_bookmarks/test_image_store.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/web/public/test/test_web_thread.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {

class LocalTestImageStore : public TestImageStore {
 public:
  LocalTestImageStore() : TestImageStore() {
    sequence_checker_.DetachFromSequence();
  }
  ~LocalTestImageStore() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalTestImageStore);
};

class BookmarkImagesIOSTest : public PlatformTest {
 public:
  BookmarkImagesIOSTest()
      : message_loop_(base::MessageLoop::TYPE_IO),
        ui_thread_(web::WebThread::UI, &message_loop_),
        io_thread_(web::WebThread::IO, &message_loop_),
        request_context_getter_(new net::TestURLRequestContextGetter(
            message_loop_.message_loop_proxy())) {}

  ~BookmarkImagesIOSTest() override {}

  void SetUp() override {
    DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
    PlatformTest::SetUp();
    StartTestServer();
    SetUpServices();
  }

  void TearDown() override {
    DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
    TearDownServices();
    ShutdownTestServer();
    PlatformTest::TearDown();
  }

  bool WaitUntilLoop(bool (^condition)()) {
    DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
    base::Time maxDate = base::Time::Now() + base::TimeDelta::FromSeconds(10);
    while (!condition()) {
      if (base::Time::Now() > maxDate)
        return false;
      message_loop_.RunUntilIdle();
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
    }
    return true;
  }

  GURL get_bookmark_url() { return test_server_->GetURL("/index.html"); }

  GURL get_image_bookmark_url() { return test_server_->GetURL("/image.jpg"); }

  GURL get_missing_image_bookmark_url() {
    return test_server_->GetURL("/no-such-image.jpg");
  }

 protected:
  bookmarks::TestBookmarkClient bookmark_client_;
  scoped_ptr<bookmarks::BookmarkModel> bookmark_model_;
  scoped_ptr<enhanced_bookmarks::EnhancedBookmarkModel>
      enhanced_bookmark_model_;
  scoped_ptr<BookmarkImageServiceIOS> bookmark_image_service_;
  base::MessageLoop message_loop_;
  web::TestWebThread ui_thread_;
  web::TestWebThread io_thread_;
  scoped_refptr<base::SequencedWorkerPool> pool_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  scoped_ptr<net::test_server::EmbeddedTestServer> test_server_;

 private:
  void StartTestServer() {
    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(ios::DIR_TEST_DATA, &test_data_dir));
    test_data_dir = test_data_dir.AppendASCII("webdata/bookmarkimages");

    test_server_.reset(new net::test_server::EmbeddedTestServer);
    ASSERT_TRUE(test_server_->InitializeAndWaitUntilReady());
    test_server_->ServeFilesFromDirectory(test_data_dir);
  }

  void ShutdownTestServer() {
    ASSERT_TRUE(test_server_->ShutdownAndWaitUntilComplete());
    test_server_.reset();
  }

  void SetUpServices() {
    pool_ = new base::SequencedWorkerPool(1, "BookmarkImagesIOSTest");
    bookmark_model_ = bookmark_client_.CreateModel();
    enhanced_bookmark_model_.reset(
        new enhanced_bookmarks::EnhancedBookmarkModel(bookmark_model_.get(),
                                                      "123"));
    bookmark_image_service_.reset(new BookmarkImageServiceIOS(
        make_scoped_ptr(new LocalTestImageStore),
        enhanced_bookmark_model_.get(), request_context_getter_.get(), pool_));
  }

  void TearDownServices() {
    // Needs to call Shutdown on KeyedService to emulate the two-phase
    // shutdown process of DependencyManager.
    bookmark_image_service_->Shutdown();
    enhanced_bookmark_model_->Shutdown();
    bookmark_model_->Shutdown();
    pool_->Shutdown();

    bookmark_image_service_.reset();
    enhanced_bookmark_model_.reset();
    bookmark_model_.reset();
    pool_ = nullptr;
  }

  DISALLOW_COPY_AND_ASSIGN(BookmarkImagesIOSTest);
};

TEST_F(BookmarkImagesIOSTest, GetNoImage) {
  __block bool success = false;
  __block bool done = false;
  bookmark_image_service_->SalientImageResizedForUrl(
      get_bookmark_url(), CGSizeMake(100, 100), false,
      base::BindBlock(
          ^(scoped_refptr<enhanced_bookmarks::ImageRecord> imageRecord) {
            DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
            done = true;
            success = imageRecord->image->IsEmpty();
          }));

  EXPECT_TRUE(WaitUntilLoop(^{
    return done;
  }));
  EXPECT_TRUE(success);
}

TEST_F(BookmarkImagesIOSTest, GetNoImageFromURLInBookmark) {
  const bookmarks::BookmarkNode* node = bookmark_model_->AddURL(
      bookmark_model_->mobile_node(), 0, base::ASCIIToUTF16("Whatever"),
      get_bookmark_url());

  ASSERT_TRUE(enhanced_bookmark_model_->SetAllImages(
      node,
      GURL(),  // original URL.
      10, 10,
      get_missing_image_bookmark_url(),  // Thumbnail URL.
      10, 10));

  __block scoped_ptr<gfx::Image> returnedImage;
  __block GURL returnedImageUrl;
  __block bool done = false;
  bookmark_image_service_->SalientImageResizedForUrl(
      get_bookmark_url(), CGSizeMake(100, 100), false,
      base::BindBlock(
          ^(scoped_refptr<enhanced_bookmarks::ImageRecord> imageRecord) {
            DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
            done = true;
            returnedImage = imageRecord->image.Pass();
            returnedImageUrl = imageRecord->url;
          }));

  EXPECT_TRUE(WaitUntilLoop(^{
    return done;
  }));
  EXPECT_TRUE(returnedImage->IsEmpty());
  EXPECT_EQ(returnedImageUrl, get_missing_image_bookmark_url());
}

TEST_F(BookmarkImagesIOSTest, getImageFromURLInBookmark) {
  const bookmarks::BookmarkNode* node = bookmark_model_->AddURL(
      bookmark_model_->mobile_node(), 0, base::ASCIIToUTF16("Whatever"),
      get_bookmark_url());
  ASSERT_TRUE(enhanced_bookmark_model_->SetAllImages(node, GURL(), 10,
                                                     10,  // original url.
                                                     get_image_bookmark_url(),
                                                     550,
                                                     368));  // thumbnail url.

  __block scoped_ptr<gfx::Image> returnedImage;
  __block GURL returnedImageUrl;
  __block bool done = false;
  bookmark_image_service_->SalientImageResizedForUrl(
      get_bookmark_url(), CGSizeMake(100, 100), false,
      base::BindBlock(
          ^(scoped_refptr<enhanced_bookmarks::ImageRecord> imageRecord) {
            DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
            done = true;
            returnedImage = imageRecord->image.Pass();
            returnedImageUrl = imageRecord->url;
          }));

  EXPECT_TRUE(WaitUntilLoop(^{
    return done;
  }));
  EXPECT_FALSE(returnedImage->IsEmpty());
  EXPECT_EQ(returnedImageUrl, get_image_bookmark_url());
}

}  // namespace
