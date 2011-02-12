// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted_memory.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/web_ui_theme_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/testing_profile.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

// A mock ThemeSource (so we can override SendResponse to get at its data).
class MockThemeSource : public WebUIThemeSource {
 public:
  explicit MockThemeSource(Profile* profile)
      : WebUIThemeSource(profile),
        result_request_id_(-1),
        result_data_size_(0) {
  }

  virtual void SendResponse(int request_id, RefCountedMemory* data) {
    result_data_size_ = data ? data->size() : 0;
    result_request_id_ = request_id;
  }

  int result_request_id_;
  size_t result_data_size_;

 private:
  ~MockThemeSource() {}
};

class WebUISourcesTest : public testing::Test {
 public:
  WebUISourcesTest() : ui_thread_(BrowserThread::UI, MessageLoop::current()) {}

  TestingProfile* profile() const { return profile_.get(); }
  MockThemeSource* theme_source() const { return theme_source_.get(); }
 private:
  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->InitThemes();
    theme_source_ = new MockThemeSource(profile_.get());
  }

  virtual void TearDown() {
    theme_source_ = NULL;
    profile_.reset(NULL);
  }

  MessageLoop loop_;
  BrowserThread ui_thread_;

  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<MockThemeSource> theme_source_;
};

TEST_F(WebUISourcesTest, ThemeSourceMimeTypes) {
  EXPECT_EQ(theme_source()->GetMimeType("css/newtab.css"), "text/css");
  EXPECT_EQ(theme_source()->GetMimeType("css/newtab.css?foo"), "text/css");
  EXPECT_EQ(theme_source()->GetMimeType("WRONGURL"), "image/png");
}

TEST_F(WebUISourcesTest, ThemeSourceImages) {
  // We used to PNGEncode the images ourselves, but encoder differences
  // invalidated that. We now just check that the image exists.
  theme_source()->StartDataRequest("IDR_THEME_FRAME_INCOGNITO", true, 1);
  size_t min = 0;
  EXPECT_EQ(theme_source()->result_request_id_, 1);
  EXPECT_GT(theme_source()->result_data_size_, min);

  theme_source()->StartDataRequest("IDR_THEME_TOOLBAR", true, 2);
  EXPECT_EQ(theme_source()->result_request_id_, 2);
  EXPECT_GT(theme_source()->result_data_size_, min);
}

TEST_F(WebUISourcesTest, ThemeSourceCSS) {
  BrowserThread io_thread(BrowserThread::IO, MessageLoop::current());
  // Generating the test data for the NTP CSS would just involve copying the
  // method, or being super brittle and hard-coding the result (requiring
  // an update to the unittest every time the CSS template changes), so we
  // just check for a successful request and data that is non-null.
  size_t empty_size = 0;

  theme_source()->StartDataRequest("css/newtab.css", false, 1);
  EXPECT_EQ(theme_source()->result_request_id_, 1);
  EXPECT_NE(theme_source()->result_data_size_, empty_size);

  theme_source()->StartDataRequest("css/newtab.css?pie", false, 3);
  EXPECT_EQ(theme_source()->result_request_id_, 3);
  EXPECT_NE(theme_source()->result_data_size_, empty_size);

  // Check that we send NULL back when we can't find what we're looking for.
  theme_source()->StartDataRequest("css/WRONGURL", false, 7);
  EXPECT_EQ(theme_source()->result_request_id_, 7);
  EXPECT_EQ(theme_source()->result_data_size_, empty_size);
}
