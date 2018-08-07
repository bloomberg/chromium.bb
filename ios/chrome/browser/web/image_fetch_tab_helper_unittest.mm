// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/image_fetch_tab_helper.h"

#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;

// Test fixture for ImageFetchTabHelper class.
class ImageFetchTabHelperTest : public web::WebTestWithWebState {
 public:
  void OnImageData(const std::string* data) {
    if (data) {
      image_data_ = std::make_unique<std::string>(*data);
    }
    on_image_data_called_ = true;
  }

 protected:
  ImageFetchTabHelperTest() = default;

  void SetUp() override {
    WebTestWithWebState::SetUp();
    ASSERT_TRUE(LoadHtml("<html></html>"));
    ImageFetchTabHelper::CreateForWebState(web_state());
  }

  ImageFetchTabHelper* image_fetch_tab_helper() {
    return ImageFetchTabHelper::FromWebState(web_state());
  }

  bool on_image_data_called_ = false;
  std::unique_ptr<std::string> image_data_;

  DISALLOW_COPY_AND_ASSIGN(ImageFetchTabHelperTest);
};

// Tests that ImageFetchTabHelper::GetImageData returns image data in callback
// when fetching image data succeeded.
TEST_F(ImageFetchTabHelperTest, GetImageDataJsSucceed) {
  // Injects fake |__gCrWeb.imageFetch.getImageData| that returns "abc" in
  // base64 as image data.
  id script_result = ExecuteJavaScript(
      @"__gCrWeb.imageFetch = {}; __gCrWeb.imageFetch.getImageData = "
      @"function(id, url)"
       "{ __gCrWeb.message.invokeOnHost({'command': 'imageFetch.getImageData', "
       "'id': id, 'data': btoa('abc')}); }; true;");
  ASSERT_NSEQ(@YES, script_result);

  image_fetch_tab_helper()->GetImageData(
      GURL("http://a.com/"),
      base::BindOnce(&ImageFetchTabHelperTest::OnImageData,
                     base::Unretained(this)));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return on_image_data_called_;
  }));

  ASSERT_TRUE(image_data_);
  EXPECT_EQ("abc", *image_data_);
}

// Tests that ImageFetchTabHelper::GetImageData returns nullptr in callback when
// fetching image data failed.
TEST_F(ImageFetchTabHelperTest, GetImageDataJsFail) {
  id script_result = ExecuteJavaScript(
      @"__gCrWeb.imageFetch = {}; __gCrWeb.imageFetch.getImageData = "
      @"function(id, url)"
       "{ __gCrWeb.message.invokeOnHost({'command': 'imageFetch.getImageData', "
       "'id': id}); }; true;");
  ASSERT_NSEQ(@YES, script_result);

  image_fetch_tab_helper()->GetImageData(
      GURL("http://a.com/"),
      base::BindOnce(&ImageFetchTabHelperTest::OnImageData,
                     base::Unretained(this)));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return on_image_data_called_;
  }));

  EXPECT_FALSE(image_data_);
}

// Tests that ImageFetchTabHelper::GetImageData returns nullptr in callback when
// WebState is destroyed.
TEST_F(ImageFetchTabHelperTest, GetImageDataWebStateDestroy) {
  image_fetch_tab_helper()->GetImageData(
      GURL("http://a.com/"),
      base::BindOnce(&ImageFetchTabHelperTest::OnImageData,
                     base::Unretained(this)));

  DestroyWebState();

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return on_image_data_called_;
  }));

  EXPECT_FALSE(image_data_);
}

// Tests that ImageFetchTabHelper::GetImageData returns nullptr in callback when
// WebState navigates to a new web page.
TEST_F(ImageFetchTabHelperTest, GetImageDataWebStateNavigate) {
  image_fetch_tab_helper()->GetImageData(
      GURL("http://a.com/"),
      base::BindOnce(&ImageFetchTabHelperTest::OnImageData,
                     base::Unretained(this)));

  LoadHtml(@"<html>new</html>"), GURL("http://new.webpage.com/");

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return on_image_data_called_;
  }));

  EXPECT_FALSE(image_data_);
}
