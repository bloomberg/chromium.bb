// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_result.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/app_list/search_result.h"

namespace app_list {

class ArcPlayStoreSearchProviderTest : public AppListTestBase {
 public:
  ArcPlayStoreSearchProviderTest() = default;
  ~ArcPlayStoreSearchProviderTest() override = default;

  // AppListTestBase:
  void SetUp() override {
    AppListTestBase::SetUp();
    arc_test_.SetUp(profile());
    controller_ = base::MakeUnique<test::TestAppListControllerDelegate>();
  }

  void TearDown() override {
    controller_.reset();
    arc_test_.TearDown();
    AppListTestBase::TearDown();
  }

 protected:
  std::unique_ptr<ArcPlayStoreSearchProvider> CreateSearch(int max_results) {
    return base::MakeUnique<ArcPlayStoreSearchProvider>(
        max_results, profile_.get(), controller_.get());
  }

 private:
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  ArcAppTest arc_test_;

  DISALLOW_COPY_AND_ASSIGN(ArcPlayStoreSearchProviderTest);
};

TEST_F(ArcPlayStoreSearchProviderTest, Basic) {
  constexpr size_t kMaxResults = 6;
  constexpr char kQuery[] = "Play App";

  std::unique_ptr<ArcPlayStoreSearchProvider> provider =
      CreateSearch(kMaxResults);
  EXPECT_TRUE(provider->results().empty());
  ArcPlayStoreSearchResult::DisableSafeDecodingForTesting();

  // Check that the result size of a query doesn't exceed the |kMaxResults|.
  provider->Start(false, base::UTF8ToUTF16(kQuery));
  const app_list::SearchProvider::Results& results = provider->results();
  ASSERT_GT(results.size(), 0u);
  ASSERT_GE(kMaxResults, results.size());

  // Check that information is correctly set in each result.
  for (size_t i = 0; i < results.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing result %zu", i));
    EXPECT_EQ(base::UTF16ToUTF8(results[i]->title()),
              base::StringPrintf("%s %zu", kQuery, i));
    EXPECT_EQ(results[i]->display_type(), SearchResult::DISPLAY_TILE);
    EXPECT_EQ(base::UTF16ToUTF8(results[i]->formatted_price()),
              base::StringPrintf("$%zu.22", i));
    EXPECT_EQ(results[i]->rating(), i);
    const bool is_instant_app = i % 2 == 0;
    EXPECT_EQ(results[i]->result_type(),
              is_instant_app ? SearchResult::RESULT_INSTANT_APP
                             : SearchResult::RESULT_PLAYSTORE_APP);
  }
}

}  // namespace app_list
