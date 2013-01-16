// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class WebUISourcesTest : public testing::Test {
 public:
  WebUISourcesTest()
      : result_data_size_(0),
        ui_thread_(BrowserThread::UI, MessageLoop::current()) {}

  TestingProfile* profile() const { return profile_.get(); }
  ThemeSource* theme_source() const { return theme_source_.get(); }
  size_t result_data_size() const { return result_data_size_; }

  void StartDataRequest(const std::string& source, bool is_incognito) {
    theme_source()->StartDataRequest(source, is_incognito, callback_);
  }

  size_t result_data_size_;

 private:
  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    theme_source_.reset(new ThemeSource(profile_.get()));
    callback_ = base::Bind(&WebUISourcesTest::SendResponse,
                           base::Unretained(this));
  }

  virtual void TearDown() {
    theme_source_.reset();
    profile_.reset();
  }

  void SendResponse(base::RefCountedMemory* data) {
    result_data_size_ = data ? data->size() : 0;
  }

  content::URLDataSource::GotDataCallback callback_;

  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<ThemeSource> theme_source_;
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
  StartDataRequest("IDR_THEME_FRAME_INCOGNITO", true);
  size_t min = 0;
  EXPECT_GT(result_data_size_, min);

  StartDataRequest("IDR_THEME_TOOLBAR", true);
  EXPECT_GT(result_data_size_, min);
}

TEST_F(WebUISourcesTest, ThemeSourceCSS) {
  content::TestBrowserThread io_thread(BrowserThread::IO,
                                       MessageLoop::current());
  // Generating the test data for the NTP CSS would just involve copying the
  // method, or being super brittle and hard-coding the result (requiring
  // an update to the unittest every time the CSS template changes), so we
  // just check for a successful request and data that is non-null.
  size_t empty_size = 0;

  StartDataRequest("css/new_tab_theme.css", false);
  EXPECT_NE(result_data_size_, empty_size);

  StartDataRequest("css/new_tab_theme.css?pie", false);
  EXPECT_NE(result_data_size_, empty_size);

  // Check that we send NULL back when we can't find what we're looking for.
  StartDataRequest("css/WRONGURL", false);
  EXPECT_EQ(result_data_size_, empty_size);
}
