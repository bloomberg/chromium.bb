// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/model/search/search_result.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_result.h"
#include "chrome/browser/ui/app_list/search/arc/icon_decode_request.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"

namespace app_list {

class ArcPlayStoreSearchProviderTest : public AppListTestBase {
 public:
  ArcPlayStoreSearchProviderTest() = default;
  ~ArcPlayStoreSearchProviderTest() override = default;

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

 protected:
  std::unique_ptr<ArcPlayStoreSearchProvider> CreateSearch(int max_results) {
    return std::make_unique<ArcPlayStoreSearchProvider>(
        max_results, profile_.get(), controller_.get());
  }

  scoped_refptr<extensions::Extension> CreateExtension(const std::string& id) {
    return extensions::ExtensionBuilder()
        .SetManifest(extensions::DictionaryBuilder()
                         .Set("name", "test")
                         .Set("version", "0.1")
                         .Build())
        .SetID(id)
        .Build();
  }

  void AddExtension(const extensions::Extension* extension) {
    service()->AddExtension(extension);
  }

 private:
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  ArcAppTest arc_test_;

  DISALLOW_COPY_AND_ASSIGN(ArcPlayStoreSearchProviderTest);
};

TEST_F(ArcPlayStoreSearchProviderTest, Basic) {
  constexpr size_t kMaxResults = 12;
  constexpr char kQuery[] = "Play App";

  std::unique_ptr<ArcPlayStoreSearchProvider> provider =
      CreateSearch(kMaxResults);
  EXPECT_TRUE(provider->results().empty());
  IconDecodeRequest::DisableSafeDecodingForTesting();

  AddExtension(CreateExtension(extension_misc::kGmailAppId).get());

  // Check that the result size of a query doesn't exceed the |kMaxResults|.
  provider->Start(base::UTF8ToUTF16(kQuery));
  const app_list::SearchProvider::Results& results = provider->results();
  ASSERT_GT(results.size(), 0u);
  // Play Store returns |kMaxResults| results, but the first one (GMail) already
  // has Chrome extension installed, so it will be skipped.
  ASSERT_EQ(kMaxResults - 1, results.size());

  // Check that information is correctly set in each result.
  for (size_t i = 0; i < results.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing result %zu", i));
    EXPECT_EQ(base::UTF16ToUTF8(results[i]->title()),
              base::StringPrintf("%s %zu", kQuery, i));
    EXPECT_EQ(results[i]->display_type(), ash::SearchResultDisplayType::kTile);
    EXPECT_EQ(base::UTF16ToUTF8(results[i]->formatted_price()),
              base::StringPrintf("$%zu.22", i));
    EXPECT_EQ(results[i]->rating(), i);
    const bool is_instant_app = i % 2 == 0;
    EXPECT_EQ(results[i]->result_type(),
              is_instant_app ? ash::SearchResultType::kInstantApp
                             : ash::SearchResultType::kPlayStoreApp);
  }
}

}  // namespace app_list
