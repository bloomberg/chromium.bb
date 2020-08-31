// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/answer_card/answer_card_result.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/views/view.h"

namespace app_list {
namespace test {

namespace {

constexpr char kSearchResultUrl[] =
    "http://www.google.com/search?q=weather&strippable_part=something";
constexpr char kStrippedSearchResultUrl[] =
    "http://google.com/search?q=weather";
constexpr char kCardUrl[] = "http://google.com/coac?q=weather";

}  // namespace

class AnswerCardResultTest : public AppListTestBase {
 public:
  AnswerCardResultTest() = default;

  std::unique_ptr<AnswerCardResult> CreateResult(
      const GURL& potential_card_url,
      const GURL& search_result_url,
      const GURL& stripped_search_result_url) const {
    return std::make_unique<AnswerCardResult>(
        profile_.get(), app_list_controller_delegate_.get(), potential_card_url,
        search_result_url, stripped_search_result_url);
  }

  const GURL& GetLastOpenedUrl() const {
    return app_list_controller_delegate_->last_opened_url();
  }

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    app_list_controller_delegate_ =
        std::make_unique<::test::TestAppListControllerDelegate>();
  }

 private:
  std::unique_ptr<::test::TestAppListControllerDelegate>
      app_list_controller_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AnswerCardResultTest);
};

TEST_F(AnswerCardResultTest, Basic) {
  std::unique_ptr<AnswerCardResult> result = CreateResult(
      GURL(kCardUrl), GURL(kSearchResultUrl), GURL(kStrippedSearchResultUrl));

  EXPECT_EQ(kCardUrl, result->id());
  EXPECT_EQ(kStrippedSearchResultUrl, result->equivalent_result_id().value());
  EXPECT_EQ(kCardUrl, result->query_url()->spec());
  EXPECT_EQ(ash::SearchResultDisplayType::kCard, result->display_type());
  EXPECT_EQ(1, result->relevance());

  result->Open(ui::EF_NONE);
  EXPECT_EQ(kSearchResultUrl, GetLastOpenedUrl().spec());
}

}  // namespace test
}  // namespace app_list
