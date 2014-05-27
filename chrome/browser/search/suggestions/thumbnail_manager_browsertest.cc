// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "chrome/browser/search/suggestions/thumbnail_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace {

const char kTestUrl1[] = "http://go.com/";
const char kTestUrl2[] = "http://goal.com/";
const char kTestImagePath[] = "files/image_decoding/droids.png";
const char kInvalidImagePath[] = "files/DOESNOTEXIST";

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

using content::BrowserThread;

class ThumbnailManagerBrowserTest : public InProcessBrowserTest {
 public:
  ThumbnailManagerBrowserTest()
    : num_callback_null_called_(0),
      num_callback_valid_called_(0),
      thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      test_server_(net::SpawnedTestServer::TYPE_HTTP,
                   net::SpawnedTestServer::kLocalhost,
                   base::FilePath(kDocRoot)) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(test_server_.Start());

    context_ = new net::TestURLRequestContextGetter(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
    context_->AddRef();
  }

  virtual void TearDown() OVERRIDE {
    BrowserThread::ReleaseSoon(BrowserThread::IO, FROM_HERE, context_);
  }

  void OnThumbnailAvailable(const GURL& url, const SkBitmap* bitmap) {
    if (bitmap) {
      num_callback_valid_called_++;
    } else {
      num_callback_null_called_++;
    }
  }
  int num_callback_null_called_;
  int num_callback_valid_called_;
  content::TestBrowserThreadBundle thread_bundle_;
  net::SpawnedTestServer test_server_;
  net::TestURLRequestContextGetter* context_;
};

}  // namespace

namespace suggestions {

IN_PROC_BROWSER_TEST_F(ThumbnailManagerBrowserTest, FetchThumbnails) {
  SuggestionsProfile suggestions_profile;
  ChromeSuggestion* suggestion = suggestions_profile.add_suggestions();
  suggestion->set_url(kTestUrl1);
  suggestion->set_thumbnail(test_server_.GetURL(kTestImagePath).spec());

  TestingProfile profile;
  ThumbnailManager thumbnail_manager(&profile);
  thumbnail_manager.InitializeThumbnailMap(suggestions_profile);
  thumbnail_manager.set_request_context(context_);

  // Fetch existing URL.
  thumbnail_manager.GetPageThumbnail(
      GURL(kTestUrl1),
      base::Bind(&ThumbnailManagerBrowserTest::OnThumbnailAvailable,
                 base::Unretained(this)));

  content::RunMessageLoop();

  EXPECT_EQ(0, num_callback_null_called_);
  EXPECT_EQ(1, num_callback_valid_called_);

  // Fetch non-existing URL.
  thumbnail_manager.GetPageThumbnail(
      GURL(kTestUrl2),
      base::Bind(&ThumbnailManagerBrowserTest::OnThumbnailAvailable,
                 base::Unretained(this)));

  content::RunMessageLoop();

  EXPECT_EQ(1, num_callback_null_called_);
  EXPECT_EQ(1, num_callback_valid_called_);
}

IN_PROC_BROWSER_TEST_F(ThumbnailManagerBrowserTest, FetchThumbnailsMultiple) {
  SuggestionsProfile suggestions_profile;
  ChromeSuggestion* suggestion = suggestions_profile.add_suggestions();
  suggestion->set_url(kTestUrl1);
  suggestion->set_thumbnail(test_server_.GetURL(kTestImagePath).spec());

  TestingProfile profile;
  ThumbnailManager thumbnail_manager(&profile);
  thumbnail_manager.InitializeThumbnailMap(suggestions_profile);
  thumbnail_manager.set_request_context(context_);

  // Fetch non-existing URL, and add more while request is in flight.
  for (int i = 0 ; i < 5; i++) {
    thumbnail_manager.GetPageThumbnail(
      GURL(kTestUrl1),
      base::Bind(&ThumbnailManagerBrowserTest::OnThumbnailAvailable,
                 base::Unretained(this)));
  }

  content::RunMessageLoop();

  EXPECT_EQ(0, num_callback_null_called_);
  EXPECT_EQ(5, num_callback_valid_called_);
}

IN_PROC_BROWSER_TEST_F(ThumbnailManagerBrowserTest, FetchThumbnailsInvalid) {
  SuggestionsProfile suggestions_profile;
  ChromeSuggestion* suggestion = suggestions_profile.add_suggestions();
  suggestion->set_url(kTestUrl1);
  suggestion->set_thumbnail(test_server_.GetURL(kInvalidImagePath).spec());

  TestingProfile profile;
  ThumbnailManager thumbnail_manager(&profile);
  thumbnail_manager.InitializeThumbnailMap(suggestions_profile);
  thumbnail_manager.set_request_context(context_);

  // Fetch existing URL that has invalid thumbnail.
  thumbnail_manager.GetPageThumbnail(
      GURL(kTestUrl1),
      base::Bind(&ThumbnailManagerBrowserTest::OnThumbnailAvailable,
                 base::Unretained(this)));

  content::RunMessageLoop();

  EXPECT_EQ(1, num_callback_null_called_);
  EXPECT_EQ(0, num_callback_valid_called_);
}

}  // namespace suggestions
