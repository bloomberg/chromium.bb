// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/available_offline_content_provider.h"

#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace android {
namespace {

using offline_items_collection::OfflineContentAggregator;
using testing::_;

std::unique_ptr<KeyedService> BuildOfflineContentAggregator(
    content::BrowserContext* context) {
  return std::make_unique<OfflineContentAggregator>();
}

offline_items_collection::OfflineItem UselessItem() {
  offline_items_collection::OfflineItem item;
  item.original_url = GURL("https://uesless");
  item.filter = offline_items_collection::FILTER_IMAGE;
  item.id.id = "Useless";
  return item;
}

offline_items_collection::OfflineItem OldOfflinePage() {
  offline_items_collection::OfflineItem item;
  item.original_url = GURL("https://already_read");
  item.filter = offline_items_collection::FILTER_PAGE;
  item.id.id = "AlreadyRead";
  item.is_suggested = true;
  item.last_accessed_time = base::Time::Now();
  return item;
}

offline_items_collection::OfflineItem SuggestedOfflinePageItem() {
  offline_items_collection::OfflineItem item;
  item.original_url = GURL("https://page");
  item.filter = offline_items_collection::FILTER_PAGE;
  item.id.id = "SuggestedPage";
  item.id.name_space = "testnamespace";
  item.is_suggested = true;
  item.title = "Page Title";
  item.description = "snippet";
  // Using Time::Now() isn't ideal, but this should result in "4 hours ago"
  // even if the test takes 1 hour to run.
  item.creation_time =
      base::Time::Now() - base::TimeDelta::FromMinutes(60 * 3.5);
  return item;
}

offline_items_collection::OfflineItem VideoItem() {
  offline_items_collection::OfflineItem item;
  item.original_url = GURL("https://video");
  item.filter = offline_items_collection::FILTER_VIDEO;
  item.id.id = "VideoItem";
  return item;
}

offline_items_collection::OfflineItem AudioItem() {
  offline_items_collection::OfflineItem item;
  item.original_url = GURL("https://audio");
  item.filter = offline_items_collection::FILTER_AUDIO;
  item.id.id = "AudioItem";
  return item;
}

class AvailableOfflineContentTest : public testing::Test {
 protected:
  void SetUp() override {
    // To control the items in the aggregator, we create it and register a
    // single MockOfflineContentProvider.
    aggregator = static_cast<OfflineContentAggregator*>(
        OfflineContentAggregatorFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile, &BuildOfflineContentAggregator));
    aggregator->RegisterProvider("offline_pages", &content_provider);
  }

  std::vector<chrome::mojom::AvailableOfflineContentPtr> ListAndWait() {
    std::vector<chrome::mojom::AvailableOfflineContentPtr> suggestions;
    bool received = false;
    provider.List(base::BindLambdaForTesting(
        [&](std::vector<chrome::mojom::AvailableOfflineContentPtr> result) {
          received = true;
          suggestions = std::move(result);
        }));
    thread_bundle.RunUntilIdle();
    EXPECT_TRUE(received);
    return suggestions;
  }

  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;
  base::test::ScopedFeatureList scoped_feature_list;
  OfflineContentAggregator* aggregator;
  offline_items_collection::MockOfflineContentProvider content_provider;
  AvailableOfflineContentProvider provider{&profile};
};

TEST_F(AvailableOfflineContentTest, NoContent) {
  std::vector<chrome::mojom::AvailableOfflineContentPtr> suggestions =
      ListAndWait();

  EXPECT_TRUE(suggestions.empty());
}

TEST_F(AvailableOfflineContentTest, AllContentFilteredOut) {
  scoped_feature_list.InitAndEnableFeature(chrome::android::kNewNetErrorPageUI);
  content_provider.SetItems({UselessItem(), OldOfflinePage()});

  std::vector<chrome::mojom::AvailableOfflineContentPtr> suggestions =
      ListAndWait();

  EXPECT_TRUE(suggestions.empty());
}

TEST_F(AvailableOfflineContentTest, ThreeItems) {
  scoped_feature_list.InitAndEnableFeature(chrome::android::kNewNetErrorPageUI);
  content_provider.SetItems({
      UselessItem(), VideoItem(), SuggestedOfflinePageItem(), AudioItem(),
  });

  std::vector<chrome::mojom::AvailableOfflineContentPtr> suggestions =
      ListAndWait();

  // Check that the right suggestions have been received in order.
  EXPECT_EQ(3ul, suggestions.size());
  EXPECT_EQ(SuggestedOfflinePageItem().id.id, suggestions[0]->id);
  EXPECT_EQ(VideoItem().id.id, suggestions[1]->id);
  EXPECT_EQ(AudioItem().id.id, suggestions[2]->id);

  // For a single suggestion, make sure all the fields are correct. We can
  // assume the other items match.
  const chrome::mojom::AvailableOfflineContentPtr& first = suggestions[0];
  const offline_items_collection::OfflineItem page_item =
      SuggestedOfflinePageItem();
  EXPECT_EQ(page_item.id.id, first->id);
  EXPECT_EQ(page_item.id.name_space, first->name_space);
  EXPECT_EQ(page_item.title, first->title);
  EXPECT_EQ(page_item.description, first->snippet);
  EXPECT_EQ("4 hours ago", first->date_modified);
  // attribution and thumbnail_data_uri not yet implemented.
  EXPECT_EQ("", first->attribution);
  EXPECT_EQ("", first->thumbnail_data_uri);
}

TEST_F(AvailableOfflineContentTest, NotEnabled) {
  scoped_feature_list.InitAndDisableFeature(
      chrome::android::kNewNetErrorPageUI);
  content_provider.SetItems({SuggestedOfflinePageItem()});

  std::vector<chrome::mojom::AvailableOfflineContentPtr> suggestions =
      ListAndWait();

  EXPECT_TRUE(suggestions.empty());
}

}  // namespace
}  // namespace android
