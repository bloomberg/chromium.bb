// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_data_search_provider.h"

#include <memory>
#include <utility>

#include "ash/app_list/model/search/search_result.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/search/arc/icon_decode_request.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"

namespace app_list {

class ArcAppDataSearchProviderTest : public AppListTestBase {
 protected:
  ArcAppDataSearchProviderTest() = default;
  ~ArcAppDataSearchProviderTest() override = default;

  // AppListTestBase:
  void SetUp() override {
    AppListTestBase::SetUp();
    arc_test_.SetUp(profile());
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
  }

  void TearDown() override {
    controller_.reset();
    arc_test_.TearDown();
    AppListTestBase::TearDown();
  }

  std::unique_ptr<ArcAppDataSearchProvider> CreateSearch(int max_results) {
    return std::make_unique<ArcAppDataSearchProvider>(max_results, profile(),
                                                      controller_.get());
  }

 private:
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  ArcAppTest arc_test_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppDataSearchProviderTest);
};

TEST_F(ArcAppDataSearchProviderTest, Basic) {
  constexpr size_t kMaxResults = 6;
  constexpr char kQuery[] = "App Data Search";

  std::unique_ptr<ArcAppDataSearchProvider> provider =
      CreateSearch(kMaxResults);
  EXPECT_TRUE(provider->results().empty());
  IconDecodeRequest::DisableSafeDecodingForTesting();

  provider->Start(base::UTF8ToUTF16(kQuery));
  const auto& results = provider->results();
  EXPECT_EQ(kMaxResults, results.size());
  // Check that information is correctly set in each result.
  for (size_t i = 0; i < results.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing result %zu", i));
    EXPECT_EQ(base::UTF16ToUTF8(results[i]->title()),
              base::StringPrintf("Label %s %zu", kQuery, i));
    EXPECT_EQ(ash::SearchResultDisplayType::kTile, results[i]->display_type());
  }
}

}  // namespace app_list
