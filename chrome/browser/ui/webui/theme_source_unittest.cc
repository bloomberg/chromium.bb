// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

// A mock URLDataSource (so we can override SendResponse to get at its data).
class MockURLDataSource : public URLDataSource {
 public:
  explicit MockURLDataSource(content::URLDataSourceDelegate* delegate)
      : URLDataSource(std::string(), delegate),
        result_request_id_(-1), result_data_size_(0) {
  }

  virtual void SendResponse(int request_id,
                            base::RefCountedMemory* data) OVERRIDE {
    result_data_size_ = data ? data->size() : 0;
    result_request_id_ = request_id;
  }

  int result_request_id_;
  size_t result_data_size_;

 private:
  ~MockURLDataSource() {}
};

class WebUISourcesTest : public testing::Test {
 public:
  WebUISourcesTest() : ui_thread_(BrowserThread::UI, MessageLoop::current()) {}

  TestingProfile* profile() const { return profile_.get(); }
  ThemeSource* theme_source() const { return theme_source_; }
  MockURLDataSource* data_source() const { return data_source_.get(); }

 private:
  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    theme_source_ = new ThemeSource(profile_.get());
    data_source_ = new MockURLDataSource(theme_source_);
    theme_source_->set_url_data_source_for_testing(data_source_.get());
  }

  virtual void TearDown() {
    theme_source_ = NULL;
    data_source_ = NULL;
    profile_.reset(NULL);
  }

  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;

  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<MockURLDataSource> data_source_;
  ThemeSource* theme_source_;
};

TEST_F(WebUISourcesTest, ThemeSourceMimeTypes) {
  EXPECT_EQ(theme_source()->GetMimeType("css/new_tab_theme.css"), "text/css");
  EXPECT_EQ(theme_source()->GetMimeType("css/new_tab_theme.css?foo"),
                                        "text/css");
  EXPECT_EQ(theme_source()->GetMimeType("WRONGURL"), "image/png");
}

TEST_F(WebUISourcesTest, ThemeSourceImages) {
  // We used to PNGEncode the images ourselves, but encoder differences
  // invalidated that. We now just check that the image exists.
  theme_source()->StartDataRequest("IDR_THEME_FRAME_INCOGNITO", true, 1);
  size_t min = 0;
  EXPECT_EQ(data_source()->result_request_id_, 1);
  EXPECT_GT(data_source()->result_data_size_, min);

  theme_source()->StartDataRequest("IDR_THEME_TOOLBAR", true, 2);
  EXPECT_EQ(data_source()->result_request_id_, 2);
  EXPECT_GT(data_source()->result_data_size_, min);
}

TEST_F(WebUISourcesTest, ThemeSourceCSS) {
  content::TestBrowserThread io_thread(BrowserThread::IO,
                                       MessageLoop::current());
  // Generating the test data for the NTP CSS would just involve copying the
  // method, or being super brittle and hard-coding the result (requiring
  // an update to the unittest every time the CSS template changes), so we
  // just check for a successful request and data that is non-null.
  size_t empty_size = 0;

  theme_source()->StartDataRequest("css/new_tab_theme.css", false, 1);
  EXPECT_EQ(data_source()->result_request_id_, 1);
  EXPECT_NE(data_source()->result_data_size_, empty_size);

  theme_source()->StartDataRequest("css/new_tab_theme.css?pie", false, 3);
  EXPECT_EQ(data_source()->result_request_id_, 3);
  EXPECT_NE(data_source()->result_data_size_, empty_size);

  // Check that we send NULL back when we can't find what we're looking for.
  theme_source()->StartDataRequest("css/WRONGURL", false, 7);
  EXPECT_EQ(data_source()->result_request_id_, 7);
  EXPECT_EQ(data_source()->result_data_size_, empty_size);
}
