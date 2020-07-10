// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/extension_search_engine_data_updater.h"

#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for ExtensionSearchEngineDataUpdater class.
class ExtensionSearchEngineDataUpdaterTest : public PlatformTest {
 protected:
  ExtensionSearchEngineDataUpdaterTest()
      : search_by_image_key_(base::SysUTF8ToNSString(
            app_group::kChromeAppGroupSupportsSearchByImage)) {}

  void SetUp() override {
    PlatformTest::SetUp();

    template_url_service_.reset(new TemplateURLService(nullptr, 0));
    template_url_service_->Load();
    observer_.reset(
        new ExtensionSearchEngineDataUpdater(template_url_service_.get()));

    NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
    [shared_defaults setBool:NO forKey:search_by_image_key_];
  }

  bool StoredSupportsSearchByImage() {
    NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
    return [shared_defaults boolForKey:search_by_image_key_];
  }

  std::unique_ptr<TemplateURLService> template_url_service_;

 private:
  std::unique_ptr<ExtensionSearchEngineDataUpdater> observer_;
  NSString* search_by_image_key_;
};

TEST_F(ExtensionSearchEngineDataUpdaterTest, AddSupportedSearchEngine) {
  ASSERT_FALSE(StoredSupportsSearchByImage());

  const char kImageSearchURL[] = "http://foo.com/sbi";
  const char kPostParamsString[] = "image_content={google:imageThumbnail}";

  TemplateURLData supported_template_url_data{};
  supported_template_url_data.image_url = kImageSearchURL;
  supported_template_url_data.image_url_post_params = kPostParamsString;
  TemplateURL supported_template_url(supported_template_url_data);

  template_url_service_->SetUserSelectedDefaultSearchProvider(
      &supported_template_url);

  ASSERT_TRUE(StoredSupportsSearchByImage());
}

TEST_F(ExtensionSearchEngineDataUpdaterTest, AddUnsupportedSearchEngine) {
  ASSERT_FALSE(StoredSupportsSearchByImage());

  TemplateURLData unsupported_template_url_data{};
  TemplateURL unsupported_template_url(unsupported_template_url_data);

  template_url_service_->SetUserSelectedDefaultSearchProvider(
      &unsupported_template_url);

  ASSERT_FALSE(StoredSupportsSearchByImage());
}
