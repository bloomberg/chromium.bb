// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_provider.h"

#include "base/callback.h"
#include "components/ntp_snippets/category_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::StrictMock;

namespace ntp_snippets {

// We do not care about any of these methods. We use mock methods so that we can
// declare a StrictMock below and ensure none of them are called.
class MockContentSuggestionsProvider : public ContentSuggestionsProvider {
 public:
  MockContentSuggestionsProvider(Observer* observer,
                                 CategoryFactory* category_factory)
      : ContentSuggestionsProvider(observer, category_factory) {}
  virtual ~MockContentSuggestionsProvider() = default;

  MOCK_METHOD1(GetCategoryStatus, CategoryStatus(Category));
  MOCK_METHOD1(GetCategoryInfo, CategoryInfo(Category));
  MOCK_METHOD1(DismissSuggestion, void(const std::string&));
  MOCK_METHOD2(FetchSuggestionImage,
               void(const std::string&, const ImageFetchedCallback&));
  MOCK_METHOD3(ClearHistory,
               void(base::Time,
                    base::Time,
                    const base::Callback<bool(const GURL&)>&));
  MOCK_METHOD1(ClearCachedSuggestions, void(Category));
  MOCK_METHOD2(GetDismissedSuggestionsForDebugging,
               void(Category, const DismissedSuggestionsCallback&));
  MOCK_METHOD1(ClearDismissedSuggestionsForDebugging, void(Category));
};

class ContentSuggestionsProviderTest : public ::testing::Test {
 protected:
  ContentSuggestionsProviderTest() : provider_(nullptr, &factory_) {}

  std::string MakeUniqueID(Category category,
                           const std::string& within_category_id) const {
    return provider_.MakeUniqueID(category, within_category_id);
  }

  Category GetCategoryFromUniqueID(const std::string& unique_id) const {
    return provider_.GetCategoryFromUniqueID(unique_id);
  }

  std::string GetWithinCategoryIDFromUniqueID(
      const std::string& unique_id) const {
    return provider_.GetWithinCategoryIDFromUniqueID(unique_id);
  }

  CategoryFactory factory_;
  StrictMock<MockContentSuggestionsProvider> provider_;
};

TEST_F(ContentSuggestionsProviderTest, Articles) {
  Category kArticles = factory_.FromKnownCategory(KnownCategories::ARTICLES);
  const std::string kUrl = "https://chromium.org/";

  std::string uid = MakeUniqueID(kArticles, kUrl);
  EXPECT_THAT(uid, Eq("10001|https://chromium.org/"));
  EXPECT_THAT(GetCategoryFromUniqueID(uid), Eq(kArticles));
  EXPECT_THAT(GetWithinCategoryIDFromUniqueID(uid), Eq(kUrl));
}

TEST_F(ContentSuggestionsProviderTest, Remote) {
  Category kRemoteCategory = factory_.FromRemoteCategory(2);
  const std::string kUrl = "https://chromium.org/";

  std::string uid = MakeUniqueID(kRemoteCategory, kUrl);
  EXPECT_THAT(uid, Eq("10002|https://chromium.org/"));
  EXPECT_THAT(GetCategoryFromUniqueID(uid), Eq(kRemoteCategory));
  EXPECT_THAT(GetWithinCategoryIDFromUniqueID(uid), Eq(kUrl));
}

TEST_F(ContentSuggestionsProviderTest, FromID) {
  Category kCategory = factory_.FromIDValue(10003);
  const std::string kUrl = "https://chromium.org/";

  std::string uid = MakeUniqueID(kCategory, kUrl);
  EXPECT_THAT(uid, Eq("10003|https://chromium.org/"));
  EXPECT_THAT(GetCategoryFromUniqueID(uid), Eq(kCategory));
  EXPECT_THAT(GetWithinCategoryIDFromUniqueID(uid), Eq(kUrl));
}

TEST_F(ContentSuggestionsProviderTest, Death) {
  const std::string kUrl = "https://chromium.org/";
#if DCHECK_IS_ON()
  EXPECT_DEATH_IF_SUPPORTED(GetCategoryFromUniqueID(kUrl),
                            "Not a valid unique_id");
  EXPECT_DEATH_IF_SUPPORTED(GetWithinCategoryIDFromUniqueID(kUrl),
                            "Not a valid unique_id");

  EXPECT_DEATH_IF_SUPPORTED(GetCategoryFromUniqueID("one|" + kUrl),
                            "Non-numeric category");
  EXPECT_THAT(GetWithinCategoryIDFromUniqueID("one|" + kUrl), Eq(kUrl));
#else
  const Category kNoCategory = factory_.FromIDValue(-1);

  EXPECT_THAT(GetCategoryFromUniqueID(kUrl), Eq(kNoCategory));
  EXPECT_THAT(GetWithinCategoryIDFromUniqueID(kUrl), Eq(kUrl));

  EXPECT_THAT(GetCategoryFromUniqueID("one|" + kUrl), Eq(kNoCategory));
  EXPECT_THAT(GetWithinCategoryIDFromUniqueID("one|" + kUrl), Eq(kUrl));
#endif
}

}  // namespace ntp_snippets
