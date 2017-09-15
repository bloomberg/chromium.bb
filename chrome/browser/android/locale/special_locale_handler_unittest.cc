// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/locale/special_locale_handler.h"

#include <stddef.h>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockSpecialLocaleHandler : public SpecialLocaleHandler {
 public:
  MockSpecialLocaleHandler(std::string locale, TemplateURLService* service)
      : SpecialLocaleHandler(locale, service) {}

  ~MockSpecialLocaleHandler() override {}

 protected:
  std::vector<std::unique_ptr<TemplateURLData>> GetLocalPrepopulatedEngines()
      override {
    std::vector<std::unique_ptr<TemplateURLData>> result;
    result.push_back(TemplateURLDataFromPrepopulatedEngine(
        TemplateURLPrepopulateData::so_360));
    result.push_back(TemplateURLDataFromPrepopulatedEngine(
        TemplateURLPrepopulateData::naver));
    result.push_back(TemplateURLDataFromPrepopulatedEngine(
        TemplateURLPrepopulateData::google));
    return result;
  }

  int GetDesignatedSearchEngine() override {
    return TemplateURLPrepopulateData::naver.id;
  }
};

class SpecialLocaleHandlerTest : public testing::Test {
public:
  SpecialLocaleHandlerTest() {}

  void SetUp() override;
  void TearDown() override;
  SpecialLocaleHandler* handler() { return handler_.get(); }
  TemplateURLServiceTestUtil* test_util() { return test_util_.get(); }
  TemplateURLService* model() { return test_util_->model(); }


 private:
  content::TestBrowserThreadBundle thread_bundle_;  // To set up BrowserThreads.
  std::unique_ptr<SpecialLocaleHandler> handler_;
  std::unique_ptr<TemplateURLServiceTestUtil> test_util_;

  DISALLOW_COPY_AND_ASSIGN(SpecialLocaleHandlerTest);
};

void SpecialLocaleHandlerTest::SetUp() {
  test_util_.reset(new TemplateURLServiceTestUtil);
  handler_.reset(new MockSpecialLocaleHandler("jp", model()));
}

void SpecialLocaleHandlerTest::TearDown() {
  handler_.reset();
  test_util_.reset();
}

TEST_F(SpecialLocaleHandlerTest, AddLocalSearchEngines) {
  test_util()->VerifyLoad();
  auto naver = base::ASCIIToUTF16("naver.com");
  auto keyword_so = base::ASCIIToUTF16("so.com");
  ASSERT_EQ(nullptr, model()->GetTemplateURLForKeyword(naver));
  ASSERT_EQ(nullptr, model()->GetTemplateURLForKeyword(keyword_so));

  ASSERT_TRUE(
      handler()->LoadTemplateUrls(nullptr, JavaParamRef<jobject>(nullptr)));

  EXPECT_EQ(TemplateURLPrepopulateData::naver.id,
            model()->GetTemplateURLForKeyword(naver)->prepopulate_id());
  EXPECT_EQ(TemplateURLPrepopulateData::so_360.id,
            model()->GetTemplateURLForKeyword(keyword_so)->prepopulate_id());

  // Ensure multiple calls to Load do not duplicate the search engines.
  size_t existing_size = model()->GetTemplateURLs().size();
  ASSERT_TRUE(
      handler()->LoadTemplateUrls(nullptr, JavaParamRef<jobject>(nullptr)));
  EXPECT_EQ(existing_size, model()->GetTemplateURLs().size());
}

TEST_F(SpecialLocaleHandlerTest, RemoveLocalSearchEngines) {
  test_util()->VerifyLoad();
  ASSERT_TRUE(
      handler()->LoadTemplateUrls(nullptr, JavaParamRef<jobject>(nullptr)));
  // Make sure locale engines are loaded.
  auto keyword_naver = base::ASCIIToUTF16("naver.com");
  auto keyword_so = base::ASCIIToUTF16("so.com");
  ASSERT_EQ(TemplateURLPrepopulateData::naver.id,
            model()->GetTemplateURLForKeyword(keyword_naver)->prepopulate_id());
  ASSERT_EQ(TemplateURLPrepopulateData::so_360.id,
            model()->GetTemplateURLForKeyword(keyword_so)->prepopulate_id());

  handler()->RemoveTemplateUrls(nullptr, JavaParamRef<jobject>(nullptr));

  ASSERT_EQ(nullptr, model()->GetTemplateURLForKeyword(keyword_naver));
  ASSERT_EQ(nullptr, model()->GetTemplateURLForKeyword(keyword_so));
}

TEST_F(SpecialLocaleHandlerTest, OverrideDefaultSearch) {
  test_util()->VerifyLoad();
  ASSERT_EQ(TemplateURLPrepopulateData::google.id,
            model()->GetDefaultSearchProvider()->prepopulate_id());
  // Load local search engines first.
  ASSERT_TRUE(
      handler()->LoadTemplateUrls(nullptr, JavaParamRef<jobject>(nullptr)));

  ASSERT_EQ(TemplateURLPrepopulateData::google.id,
            model()->GetDefaultSearchProvider()->prepopulate_id());

  // Set one of the local search engine as default.
  handler()->OverrideDefaultSearchProvider(nullptr,
                                           JavaParamRef<jobject>(nullptr));
  ASSERT_EQ(TemplateURLPrepopulateData::naver.id,
            model()->GetDefaultSearchProvider()->prepopulate_id());

  // Revert the default search engine tweak.
  handler()->SetGoogleAsDefaultSearch(nullptr, JavaParamRef<jobject>(nullptr));
  ASSERT_EQ(TemplateURLPrepopulateData::google.id,
            model()->GetDefaultSearchProvider()->prepopulate_id());
}

TEST_F(SpecialLocaleHandlerTest, ChangedGoogleBaseURL) {
  test_util()->VerifyLoad();
  auto google_keyword = base::ASCIIToUTF16("google.com");
  ASSERT_THAT(model()->GetTemplateURLForKeyword(google_keyword),
              testing::NotNull());
  test_util()->SetGoogleBaseURL(GURL("http://google.de"));

  // After changing the base URL, the previous google keyword will no longer
  // match.
  ASSERT_EQ(nullptr, model()->GetTemplateURLForKeyword(google_keyword));

  ASSERT_TRUE(
      handler()->LoadTemplateUrls(nullptr, JavaParamRef<jobject>(nullptr)));

  auto template_urls = model()->GetTemplateURLs();
  ASSERT_EQ(1, std::count_if(template_urls.begin(), template_urls.end(),
                             [](TemplateURL* template_url) {
                               return template_url->prepopulate_id() ==
                                      TemplateURLPrepopulateData::google.id;
                             }));
}
