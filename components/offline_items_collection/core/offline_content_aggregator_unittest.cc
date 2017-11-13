// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/offline_content_aggregator.h"

#include <map>

#include "base/memory/ptr_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"
#include "components/offline_items_collection/core/test_support/scoped_mock_offline_content_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ContainerEq;
using testing::Return;

namespace offline_items_collection {
namespace {

struct CompareOfflineItemsById {
  bool operator()(const OfflineItem& a, const OfflineItem& b) const {
    return a.id < b.id;
  }
};

// A custom comparator that makes sure two vectors contain the same elements.
// TODO(dtrainor): Look into building a better matcher that works with gmock.
template <typename T>
bool VectorContentsEq(const std::vector<T>& list1,
                      const std::vector<T>& list2) {
  if (list1.size() != list2.size())
    return false;

  std::map<T, int, CompareOfflineItemsById> occurance_counts;
  for (auto it = list1.begin(); it != list1.end(); it++)
    occurance_counts[*it]++;

  for (auto it = list2.begin(); it != list2.end(); it++)
    occurance_counts[*it]--;

  for (auto it = occurance_counts.begin(); it != occurance_counts.end(); it++) {
    if (it->second != 0)
      return false;
  }

  return true;
}

// Helper class that automatically unregisters itself from the aggregator in the
// case that someone calls OpenItem on it.
class OpenItemRemovalOfflineContentProvider
    : public ScopedMockOfflineContentProvider {
 public:
  OpenItemRemovalOfflineContentProvider(const std::string& name_space,
                                        OfflineContentAggregator* aggregator)
      : ScopedMockOfflineContentProvider(name_space, aggregator) {}
  ~OpenItemRemovalOfflineContentProvider() override {}

  void OpenItem(const ContentId& id) override {
    ScopedMockOfflineContentProvider::OpenItem(id);
    Unregister();
  }
};

class OfflineContentAggregatorTest : public testing::Test {
 public:
  OfflineContentAggregatorTest()
      : task_runner_(new base::TestMockTimeTaskRunner), handle_(task_runner_) {}
  ~OfflineContentAggregatorTest() override {}

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle handle_;
  OfflineContentAggregator aggregator_;
};

TEST_F(OfflineContentAggregatorTest, ObserversAddedBeforeProvidersAvailable) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);
  EXPECT_FALSE(aggregator_.AreItemsAvailable());
  EXPECT_TRUE(provider1.HasObserver(&aggregator_));
  EXPECT_TRUE(provider2.HasObserver(&aggregator_));

  ScopedMockOfflineContentProvider::ScopedMockObserver observer1(&aggregator_);
  ScopedMockOfflineContentProvider::ScopedMockObserver observer2(&aggregator_);
  task_runner_->RunUntilIdle();

  {
    EXPECT_CALL(observer1, OnItemsAvailable(&aggregator_)).Times(0);
    EXPECT_CALL(observer2, OnItemsAvailable(&aggregator_)).Times(0);
    provider1.NotifyOnItemsAvailable();
  }

  {
    EXPECT_CALL(observer1, OnItemsAvailable(&aggregator_)).Times(1);
    EXPECT_CALL(observer2, OnItemsAvailable(&aggregator_)).Times(1);
    provider2.NotifyOnItemsAvailable();
  }
}

TEST_F(OfflineContentAggregatorTest, ObserversAddedAfterProvidersAvailable) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);
  EXPECT_FALSE(aggregator_.AreItemsAvailable());
  EXPECT_TRUE(provider1.HasObserver(&aggregator_));
  EXPECT_TRUE(provider2.HasObserver(&aggregator_));

  provider1.NotifyOnItemsAvailable();
  provider2.NotifyOnItemsAvailable();

  {
    ScopedMockOfflineContentProvider::ScopedMockObserver observer1(
        &aggregator_);
    ScopedMockOfflineContentProvider::ScopedMockObserver observer2(
        &aggregator_);
    EXPECT_CALL(observer1, OnItemsAvailable(&aggregator_)).Times(1);
    EXPECT_CALL(observer2, OnItemsAvailable(&aggregator_)).Times(1);
    task_runner_->RunUntilIdle();
  }
}

TEST_F(OfflineContentAggregatorTest,
       ProvidersAddedAfterObserversNotifiedAvailable) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  EXPECT_FALSE(aggregator_.AreItemsAvailable());
  EXPECT_TRUE(provider1.HasObserver(&aggregator_));

  ScopedMockOfflineContentProvider::ScopedMockObserver observer1(&aggregator_);
  ScopedMockOfflineContentProvider::ScopedMockObserver observer2(&aggregator_);
  task_runner_->RunUntilIdle();

  {
    EXPECT_CALL(observer1, OnItemsAvailable(&aggregator_)).Times(1);
    EXPECT_CALL(observer2, OnItemsAvailable(&aggregator_)).Times(1);
    provider1.NotifyOnItemsAvailable();
  }

  {
    OfflineContentProvider::OfflineItemList items;
    items.push_back(OfflineItem());

    ScopedMockOfflineContentProvider provider2("2", &aggregator_);
    EXPECT_TRUE(provider2.HasObserver(&aggregator_));

    EXPECT_CALL(provider2, GetAllItems()).WillOnce(Return(items));
    EXPECT_CALL(observer1, OnItemsAvailable(&aggregator_)).Times(0);
    EXPECT_CALL(observer2, OnItemsAvailable(&aggregator_)).Times(0);
    EXPECT_CALL(observer1, OnItemsAdded(items)).Times(1);
    EXPECT_CALL(observer2, OnItemsAdded(items)).Times(1);
    provider2.NotifyOnItemsAvailable();
  }
}

TEST_F(OfflineContentAggregatorTest, QueryingItemsWithProviderThatIsntReady) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);
  EXPECT_FALSE(aggregator_.AreItemsAvailable());

  OfflineContentProvider::OfflineItemList items1;
  items1.push_back(OfflineItem(ContentId("1", "A")));
  items1.push_back(OfflineItem(ContentId("1", "B")));

  OfflineContentProvider::OfflineItemList items2;
  items2.push_back(OfflineItem(ContentId("2", "C")));
  items2.push_back(OfflineItem(ContentId("2", "D")));

  EXPECT_CALL(provider1, GetAllItems()).WillRepeatedly(Return(items1));
  EXPECT_CALL(provider2, GetAllItems()).WillRepeatedly(Return(items2));

  provider1.NotifyOnItemsAvailable();
  EXPECT_TRUE(VectorContentsEq(items1, aggregator_.GetAllItems()));

  OfflineContentProvider::OfflineItemList combined_items(items1);
  combined_items.insert(combined_items.end(), items2.begin(), items2.end());
  provider2.NotifyOnItemsAvailable();

  EXPECT_TRUE(VectorContentsEq(combined_items, aggregator_.GetAllItems()));
}

TEST_F(OfflineContentAggregatorTest, QueryingItemFromRemovedProvider) {
  ContentId id("1", "A");
  OfflineItem item(id);

  {
    ScopedMockOfflineContentProvider provider("1", &aggregator_);
    provider.NotifyOnItemsAvailable();
    EXPECT_TRUE(aggregator_.AreItemsAvailable());

    EXPECT_CALL(provider, GetItemById(id)).WillRepeatedly(Return(&item));
    EXPECT_EQ(&item, aggregator_.GetItemById(id));
  }

  EXPECT_EQ(nullptr, aggregator_.GetItemById(id));
}

TEST_F(OfflineContentAggregatorTest, QueryingItemWithProviderThatIsntReady) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);
  EXPECT_FALSE(aggregator_.AreItemsAvailable());

  ContentId id1("1", "A");
  ContentId id2("2", "B");
  ContentId id3("3", "C");

  OfflineItem item1(id1);
  OfflineItem item2(id2);

  EXPECT_CALL(provider1, GetItemById(id1)).WillRepeatedly(Return(&item1));
  EXPECT_CALL(provider2, GetItemById(id2)).WillRepeatedly(Return(&item2));

  EXPECT_EQ(nullptr, aggregator_.GetItemById(id1));
  EXPECT_EQ(nullptr, aggregator_.GetItemById(id2));
  EXPECT_EQ(nullptr, aggregator_.GetItemById(id3));

  provider1.NotifyOnItemsAvailable();
  EXPECT_EQ(&item1, aggregator_.GetItemById(id1));
  EXPECT_EQ(nullptr, aggregator_.GetItemById(id2));
  EXPECT_EQ(nullptr, aggregator_.GetItemById(id3));

  provider2.NotifyOnItemsAvailable();
  EXPECT_EQ(&item1, aggregator_.GetItemById(id1));
  EXPECT_EQ(&item2, aggregator_.GetItemById(id2));
  EXPECT_EQ(nullptr, aggregator_.GetItemById(id3));
}

TEST_F(OfflineContentAggregatorTest, GetItemByIdPropagatesToRightProvider) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);
  provider1.NotifyOnItemsAvailable();
  provider2.NotifyOnItemsAvailable();

  ContentId id1("1", "A");
  ContentId id2("2", "B");
  ContentId id3("1", "C");
  ContentId id4("3", "D");
  OfflineItem item1(id1);
  OfflineItem item2(id2);

  EXPECT_CALL(provider1, GetItemById(id1)).WillRepeatedly(Return(&item1));
  EXPECT_CALL(provider2, GetItemById(id2)).WillRepeatedly(Return(&item2));
  EXPECT_CALL(provider1, GetItemById(id3)).WillRepeatedly(Return(nullptr));

  EXPECT_EQ(&item1, aggregator_.GetItemById(id1));
  EXPECT_EQ(&item2, aggregator_.GetItemById(id2));
  EXPECT_EQ(nullptr, aggregator_.GetItemById(id3));
  EXPECT_EQ(nullptr, aggregator_.GetItemById(id4));
}

TEST_F(OfflineContentAggregatorTest, AreItemsAvailable) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);

  ScopedMockOfflineContentProvider::ScopedMockObserver observer1(&aggregator_);
  ScopedMockOfflineContentProvider::ScopedMockObserver observer2(&aggregator_);
  task_runner_->RunUntilIdle();

  EXPECT_FALSE(aggregator_.AreItemsAvailable());

  {
    EXPECT_CALL(observer1, OnItemsAvailable(&aggregator_)).Times(0);
    EXPECT_CALL(observer2, OnItemsAvailable(&aggregator_)).Times(0);

    provider1.NotifyOnItemsAvailable();
  }

  EXPECT_FALSE(aggregator_.AreItemsAvailable());

  {
    EXPECT_CALL(observer1, OnItemsAvailable(&aggregator_)).Times(1);
    EXPECT_CALL(observer2, OnItemsAvailable(&aggregator_)).Times(1);

    provider2.NotifyOnItemsAvailable();
  }

  EXPECT_TRUE(aggregator_.AreItemsAvailable());
}

TEST_F(OfflineContentAggregatorTest, ActionPropagatesToRightProvider) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);
  provider1.NotifyOnItemsAvailable();
  provider2.NotifyOnItemsAvailable();

  testing::InSequence sequence;
  ContentId id1("1", "A");
  ContentId id2("2", "B");
  EXPECT_CALL(provider1, OpenItem(id1)).Times(1);
  EXPECT_CALL(provider2, OpenItem(id2)).Times(1);
  EXPECT_CALL(provider1, RemoveItem(id1)).Times(1);
  EXPECT_CALL(provider2, RemoveItem(id2)).Times(1);
  EXPECT_CALL(provider1, CancelDownload(id1)).Times(1);
  EXPECT_CALL(provider2, CancelDownload(id2)).Times(1);
  EXPECT_CALL(provider1, ResumeDownload(id1, false)).Times(1);
  EXPECT_CALL(provider2, ResumeDownload(id2, true)).Times(1);
  EXPECT_CALL(provider1, PauseDownload(id1)).Times(1);
  EXPECT_CALL(provider2, PauseDownload(id2)).Times(1);
  EXPECT_CALL(provider1, GetVisualsForItem(id1, _)).Times(1);
  EXPECT_CALL(provider2, GetVisualsForItem(id2, _)).Times(1);
  aggregator_.OpenItem(id1);
  aggregator_.OpenItem(id2);
  aggregator_.RemoveItem(id1);
  aggregator_.RemoveItem(id2);
  aggregator_.CancelDownload(id1);
  aggregator_.CancelDownload(id2);
  aggregator_.ResumeDownload(id1, false);
  aggregator_.ResumeDownload(id2, true);
  aggregator_.PauseDownload(id1);
  aggregator_.PauseDownload(id2);
  aggregator_.GetVisualsForItem(id1, OfflineContentProvider::VisualsCallback());
  aggregator_.GetVisualsForItem(id2, OfflineContentProvider::VisualsCallback());
}

TEST_F(OfflineContentAggregatorTest, ActionPropagatesAfterInitialize) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);

  ContentId id1("1", "A");
  ContentId id2("2", "B");
  ContentId id3("2", "C");

  {
    EXPECT_CALL(provider1, PauseDownload(id1)).Times(0);
    aggregator_.PauseDownload(id1);
  }

  {
    testing::InSequence sequence;
    EXPECT_CALL(provider1, PauseDownload(id1)).Times(1);
    EXPECT_CALL(provider1, ResumeDownload(id1, true)).Times(1);
    EXPECT_CALL(provider1, OpenItem(id1)).Times(1);
    EXPECT_CALL(provider2, OpenItem(id2)).Times(0);

    aggregator_.ResumeDownload(id1, true);
    aggregator_.OpenItem(id1);
    provider1.NotifyOnItemsAvailable();
    aggregator_.OpenItem(id2);
  }

  {
    testing::InSequence sequence;
    EXPECT_CALL(provider2, OpenItem(id2)).Times(1);
    EXPECT_CALL(provider2, RemoveItem(id3)).Times(1);
    aggregator_.RemoveItem(id3);
    provider2.NotifyOnItemsAvailable();
  }
}

TEST_F(OfflineContentAggregatorTest, OnItemsAddedPropagatedToObservers) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);
  provider1.NotifyOnItemsAvailable();
  provider2.NotifyOnItemsAvailable();

  ScopedMockOfflineContentProvider::ScopedMockObserver observer1(&aggregator_);
  ScopedMockOfflineContentProvider::ScopedMockObserver observer2(&aggregator_);

  EXPECT_CALL(observer1, OnItemsAvailable(&aggregator_)).Times(1);
  EXPECT_CALL(observer2, OnItemsAvailable(&aggregator_)).Times(1);
  task_runner_->RunUntilIdle();

  OfflineContentProvider::OfflineItemList items1;
  items1.push_back(OfflineItem(ContentId("1", "A")));
  items1.push_back(OfflineItem(ContentId("1", "B")));

  OfflineContentProvider::OfflineItemList items2;
  items2.push_back(OfflineItem(ContentId("2", "C")));
  items2.push_back(OfflineItem(ContentId("2", "D")));

  EXPECT_CALL(observer1, OnItemsAdded(items1)).Times(1);
  EXPECT_CALL(observer1, OnItemsAdded(items2)).Times(1);
  EXPECT_CALL(observer2, OnItemsAdded(items1)).Times(1);
  EXPECT_CALL(observer2, OnItemsAdded(items2)).Times(1);
  provider1.NotifyOnItemsAdded(items1);
  provider2.NotifyOnItemsAdded(items2);
}

TEST_F(OfflineContentAggregatorTest, OnItemRemovedPropagatedToObservers) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);
  provider1.NotifyOnItemsAvailable();
  provider2.NotifyOnItemsAvailable();

  ScopedMockOfflineContentProvider::ScopedMockObserver observer1(&aggregator_);
  ScopedMockOfflineContentProvider::ScopedMockObserver observer2(&aggregator_);

  EXPECT_CALL(observer1, OnItemsAvailable(&aggregator_)).Times(1);
  EXPECT_CALL(observer2, OnItemsAvailable(&aggregator_)).Times(1);
  task_runner_->RunUntilIdle();

  ContentId id1("1", "A");
  ContentId id2("2", "B");

  EXPECT_CALL(observer1, OnItemRemoved(id1)).Times(1);
  EXPECT_CALL(observer1, OnItemRemoved(id2)).Times(1);
  EXPECT_CALL(observer2, OnItemRemoved(id1)).Times(1);
  EXPECT_CALL(observer2, OnItemRemoved(id2)).Times(1);
  provider1.NotifyOnItemRemoved(id1);
  provider2.NotifyOnItemRemoved(id2);
}

TEST_F(OfflineContentAggregatorTest, OnItemUpdatedPropagatedToObservers) {
  ScopedMockOfflineContentProvider provider1("1", &aggregator_);
  ScopedMockOfflineContentProvider provider2("2", &aggregator_);
  provider1.NotifyOnItemsAvailable();
  provider2.NotifyOnItemsAvailable();

  ScopedMockOfflineContentProvider::ScopedMockObserver observer1(&aggregator_);
  ScopedMockOfflineContentProvider::ScopedMockObserver observer2(&aggregator_);

  EXPECT_CALL(observer1, OnItemsAvailable(&aggregator_)).Times(1);
  EXPECT_CALL(observer2, OnItemsAvailable(&aggregator_)).Times(1);
  task_runner_->RunUntilIdle();

  OfflineItem item1(ContentId("1", "A"));
  OfflineItem item2(ContentId("2", "B"));

  EXPECT_CALL(observer1, OnItemUpdated(item1)).Times(1);
  EXPECT_CALL(observer1, OnItemUpdated(item2)).Times(1);
  EXPECT_CALL(observer2, OnItemUpdated(item1)).Times(1);
  EXPECT_CALL(observer2, OnItemUpdated(item2)).Times(1);
  provider1.NotifyOnItemUpdated(item1);
  provider2.NotifyOnItemUpdated(item2);
}

TEST_F(OfflineContentAggregatorTest, ProviderRemovedDuringCallbackFlush) {
  OpenItemRemovalOfflineContentProvider provider1("1", &aggregator_);

  ContentId id1("1", "A");
  ContentId id2("1", "B");
  aggregator_.OpenItem(id1);
  aggregator_.OpenItem(id2);
  aggregator_.RemoveItem(id2);

  EXPECT_CALL(provider1, OpenItem(id1)).Times(1);
  EXPECT_CALL(provider1, RemoveItem(id2)).Times(0);
  provider1.NotifyOnItemsAvailable();
}

TEST_F(OfflineContentAggregatorTest, SameProviderWithMultipleNamespaces) {
  MockOfflineContentProvider provider;
  ScopedMockOfflineContentProvider::ScopedMockObserver observer(&aggregator_);

  ContentId id1("1", "A");
  ContentId id2("2", "B");
  OfflineItem item1(id1);
  OfflineItem item2(id2);
  OfflineContentProvider::OfflineItemList items;
  items.push_back(item1);
  items.push_back(item2);

  aggregator_.RegisterProvider("1", &provider);
  aggregator_.RegisterProvider("2", &provider);
  EXPECT_TRUE(provider.HasObserver(&aggregator_));

  EXPECT_CALL(provider, GetAllItems()).WillRepeatedly(Return(items));
  EXPECT_CALL(provider, GetItemById(id1)).WillRepeatedly(Return(&item1));
  EXPECT_CALL(provider, GetItemById(id2)).WillRepeatedly(Return(&item2));

  EXPECT_CALL(observer, OnItemsAvailable(&aggregator_)).Times(1);
  provider.NotifyOnItemsAvailable();

  EXPECT_TRUE(VectorContentsEq(items, aggregator_.GetAllItems()));
  EXPECT_EQ(&item1, aggregator_.GetItemById(id1));
  EXPECT_EQ(&item2, aggregator_.GetItemById(id2));

  aggregator_.UnregisterProvider("1");
  EXPECT_TRUE(provider.HasObserver(&aggregator_));
  EXPECT_EQ(nullptr, aggregator_.GetItemById(id1));
  EXPECT_EQ(&item2, aggregator_.GetItemById(id2));

  aggregator_.UnregisterProvider("2");
  EXPECT_FALSE(provider.HasObserver(&aggregator_));
}

// Test that ensures if no provider is registered, the observer can get an
// initialization signal.
TEST_F(OfflineContentAggregatorTest, NoProviderWithObserver) {
  ScopedMockOfflineContentProvider::ScopedMockObserver observer;
  EXPECT_CALL(observer, OnItemsAvailable(&aggregator_)).Times(1);
  observer.AddProvider(&aggregator_);
  task_runner_->RunUntilIdle();
}

}  // namespace
}  // namespace offline_items_collection
