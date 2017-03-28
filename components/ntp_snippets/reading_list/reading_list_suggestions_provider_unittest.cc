// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/reading_list/reading_list_suggestions_provider.h"

#include "base/memory/ptr_util.h"
#include "base/test/simple_test_clock.h"
#include "components/ntp_snippets/mock_content_suggestions_provider_observer.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

namespace {

class ReadingListSuggestionsProviderTest : public ::testing::Test {
 public:
  ReadingListSuggestionsProviderTest() {
    model_ = base::MakeUnique<ReadingListModelImpl>(
        /*storage_layer=*/nullptr, /*pref_service=*/nullptr,
        base::MakeUnique<base::SimpleTestClock>());
    EXPECT_CALL(observer_,
                OnCategoryStatusChanged(testing::_, ReadingListCategory(),
                                        CategoryStatus::AVAILABLE_LOADING))
        .RetiresOnSaturation();

    provider_ = base::MakeUnique<ReadingListSuggestionsProvider>(&observer_,
                                                                 model_.get());
  }

  Category ReadingListCategory() {
    return Category::FromKnownCategory(KnownCategories::READING_LIST);
  }

 protected:
  std::unique_ptr<ReadingListModelImpl> model_;
  testing::StrictMock<MockContentSuggestionsProviderObserver> observer_;
  std::unique_ptr<ReadingListSuggestionsProvider> provider_;
};

TEST_F(ReadingListSuggestionsProviderTest, CategoryInfo) {
  CategoryInfo categoryInfo = provider_->GetCategoryInfo(ReadingListCategory());
  EXPECT_EQ(ContentSuggestionsAdditionalAction::VIEW_ALL,
            categoryInfo.additional_action());
}

}  // namespace
}  // namespace ntp_snippets
