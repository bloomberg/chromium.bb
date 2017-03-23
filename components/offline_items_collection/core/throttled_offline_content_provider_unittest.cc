// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/offline_content_aggregator.h"

#include "base/memory/ptr_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"
#include "components/offline_items_collection/core/test_support/scoped_mock_offline_content_provider.h"
#include "components/offline_items_collection/core/throttled_offline_content_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace offline_items_collection {
namespace {

// Helper class to automatically trigger another OnItemUpdated() to the
// underlying provider when this observer gets notified of OnItemUpdated().
// This will only happen the first time the ContentId of the udpated OfflineItem
// matches the ContentId of the OfflineItem passed into this class constructor.
class TriggerSingleReentrantUpdateHelper
    : public ScopedMockOfflineContentProvider::ScopedMockObserver {
 public:
  TriggerSingleReentrantUpdateHelper(
      OfflineContentProvider* provider,
      MockOfflineContentProvider* wrapped_provider,
      const OfflineItem& new_item)
      : ScopedMockObserver(provider),
        wrapped_provider_(wrapped_provider),
        new_item_(new_item) {}
  ~TriggerSingleReentrantUpdateHelper() override {}

  void OnItemUpdated(const OfflineItem& item) override {
    if (wrapped_provider_) {
      if (item.id == new_item_.id)
        wrapped_provider_->NotifyOnItemUpdated(new_item_);
      wrapped_provider_ = nullptr;
    }
    ScopedMockObserver::OnItemUpdated(item);
  }

 private:
  MockOfflineContentProvider* wrapped_provider_;
  OfflineItem new_item_;
};

class ThrottledOfflineContentProviderTest : public testing::Test {
 public:
  ThrottledOfflineContentProviderTest()
      : task_runner_(new base::TestMockTimeTaskRunner),
        handle_(task_runner_),
        provider_(base::TimeDelta::FromMilliseconds(1), &wrapped_provider_) {}
  ~ThrottledOfflineContentProviderTest() override {}

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle handle_;

  MockOfflineContentProvider wrapped_provider_;
  ThrottledOfflineContentProvider provider_;
};

TEST_F(ThrottledOfflineContentProviderTest, TestReadyBeforeObserver) {
  EXPECT_FALSE(provider_.AreItemsAvailable());
  wrapped_provider_.NotifyOnItemsAvailable();
  EXPECT_TRUE(provider_.AreItemsAvailable());

  ScopedMockOfflineContentProvider::ScopedMockObserver observer1(&provider_);
  ScopedMockOfflineContentProvider::ScopedMockObserver observer2(&provider_);

  EXPECT_CALL(observer1, OnItemsAvailable(&provider_));
  EXPECT_CALL(observer2, OnItemsAvailable(&provider_));
  task_runner_->RunUntilIdle();
}

TEST_F(ThrottledOfflineContentProviderTest, TestReadyAfterObserver) {
  ScopedMockOfflineContentProvider::ScopedMockObserver observer(&provider_);

  EXPECT_CALL(observer, OnItemsAvailable(&provider_));
  EXPECT_FALSE(provider_.AreItemsAvailable());
  wrapped_provider_.NotifyOnItemsAvailable();
  EXPECT_TRUE(provider_.AreItemsAvailable());
}

TEST_F(ThrottledOfflineContentProviderTest, TestBasicPassthrough) {
  ScopedMockOfflineContentProvider::ScopedMockObserver observer(&provider_);

  ContentId id("1", "A");
  OfflineItem item(id);

  OfflineContentProvider::OfflineItemList items;
  items.push_back(item);

  testing::InSequence sequence;
  EXPECT_CALL(observer, OnItemsAvailable(&provider_));
  EXPECT_CALL(wrapped_provider_, OpenItem(id));
  EXPECT_CALL(wrapped_provider_, RemoveItem(id));
  EXPECT_CALL(wrapped_provider_, CancelDownload(id));
  EXPECT_CALL(wrapped_provider_, PauseDownload(id));
  EXPECT_CALL(wrapped_provider_, ResumeDownload(id));
  EXPECT_CALL(wrapped_provider_, GetItemById(id)).WillRepeatedly(Return(&item));
  EXPECT_CALL(wrapped_provider_, GetAllItems()).WillRepeatedly(Return(items));

  wrapped_provider_.NotifyOnItemsAvailable();
  provider_.OpenItem(id);
  provider_.RemoveItem(id);
  provider_.CancelDownload(id);
  provider_.PauseDownload(id);
  provider_.ResumeDownload(id);
  EXPECT_EQ(&item, provider_.GetItemById(id));
  EXPECT_EQ(items, provider_.GetAllItems());
}

TEST_F(ThrottledOfflineContentProviderTest, TestRemoveCancelsUpdate) {
  ScopedMockOfflineContentProvider::ScopedMockObserver observer(&provider_);

  ContentId id("1", "A");
  OfflineItem item(id);

  EXPECT_CALL(observer, OnItemsAvailable(&provider_)).Times(1);
  EXPECT_CALL(observer, OnItemUpdated(item)).Times(0);
  EXPECT_CALL(observer, OnItemRemoved(id)).Times(1);

  wrapped_provider_.NotifyOnItemsAvailable();
  wrapped_provider_.NotifyOnItemUpdated(item);
  wrapped_provider_.NotifyOnItemRemoved(id);
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(ThrottledOfflineContentProviderTest, TestOnItemUpdatedSquashed) {
  ScopedMockOfflineContentProvider::ScopedMockObserver observer(&provider_);

  ContentId id1("1", "A");
  ContentId id2("2", "B");

  OfflineItem item1(id1);
  OfflineItem item2(id2);

  OfflineItem updated_item1(id1);
  updated_item1.title = "updated1";
  OfflineItem updated_item2(id2);
  updated_item2.title = "updated2";

  EXPECT_CALL(observer, OnItemsAvailable(&provider_)).Times(1);
  EXPECT_CALL(observer, OnItemUpdated(updated_item1)).Times(1);
  EXPECT_CALL(observer, OnItemUpdated(updated_item2)).Times(1);

  wrapped_provider_.NotifyOnItemsAvailable();
  wrapped_provider_.NotifyOnItemUpdated(item1);
  wrapped_provider_.NotifyOnItemUpdated(item2);
  wrapped_provider_.NotifyOnItemUpdated(updated_item2);
  wrapped_provider_.NotifyOnItemUpdated(updated_item1);

  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(ThrottledOfflineContentProviderTest, TestGetItemByIdOverridesUpdate) {
  ScopedMockOfflineContentProvider::ScopedMockObserver observer(&provider_);

  ContentId id1("1", "A");
  ContentId id2("2", "B");

  OfflineItem item1(id1);
  OfflineItem item2(id2);

  OfflineItem updated_item1(id1);
  updated_item1.title = "updated1";

  EXPECT_CALL(observer, OnItemsAvailable(&provider_)).Times(1);
  EXPECT_CALL(wrapped_provider_, GetItemById(id1))
      .WillRepeatedly(Return(&updated_item1));
  EXPECT_CALL(observer, OnItemUpdated(updated_item1)).Times(1);
  EXPECT_CALL(observer, OnItemUpdated(item2)).Times(1);

  wrapped_provider_.NotifyOnItemsAvailable();
  wrapped_provider_.NotifyOnItemUpdated(item1);
  wrapped_provider_.NotifyOnItemUpdated(item2);

  EXPECT_EQ(&updated_item1, provider_.GetItemById(id1));

  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(ThrottledOfflineContentProviderTest, TestGetAllItemsOverridesUpdate) {
  ScopedMockOfflineContentProvider::ScopedMockObserver observer(&provider_);

  ContentId id1("1", "A");
  ContentId id2("2", "B");

  OfflineItem item1(id1);
  OfflineItem item2(id2);

  OfflineItem updated_item1(id1);
  updated_item1.title = "updated1";

  OfflineContentProvider::OfflineItemList items;
  items.push_back(updated_item1);
  items.push_back(item2);

  EXPECT_CALL(observer, OnItemsAvailable(&provider_)).Times(1);
  EXPECT_CALL(wrapped_provider_, GetAllItems()).WillRepeatedly(Return(items));
  EXPECT_CALL(observer, OnItemUpdated(updated_item1)).Times(1);
  EXPECT_CALL(observer, OnItemUpdated(item2)).Times(1);

  wrapped_provider_.NotifyOnItemsAvailable();
  wrapped_provider_.NotifyOnItemUpdated(item1);
  wrapped_provider_.NotifyOnItemUpdated(item2);

  EXPECT_EQ(items, provider_.GetAllItems());

  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(ThrottledOfflineContentProviderTest, TestThrottleWorksProperly) {
  ScopedMockOfflineContentProvider::ScopedMockObserver observer(&provider_);

  ContentId id1("1", "A");

  OfflineItem item1(id1);

  OfflineItem item2(id1);
  item2.title = "updated1";

  OfflineItem item3(id1);
  item3.title = "updated2";

  EXPECT_CALL(observer, OnItemsAvailable(&provider_)).Times(1);
  EXPECT_CALL(observer, OnItemUpdated(item2)).Times(1);

  wrapped_provider_.NotifyOnItemsAvailable();

  {
    wrapped_provider_.NotifyOnItemUpdated(item1);
    wrapped_provider_.NotifyOnItemUpdated(item2);
    task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  }

  {
    EXPECT_CALL(observer, OnItemUpdated(item3)).Times(1);
    wrapped_provider_.NotifyOnItemUpdated(item3);
    task_runner_->FastForwardUntilNoTasksRemain();
  }
}

TEST_F(ThrottledOfflineContentProviderTest, TestReentrantUpdatesGetQueued) {
  ContentId id("1", "A");

  OfflineItem item(id);
  OfflineItem updated_item(id);
  updated_item.title = "updated";

  TriggerSingleReentrantUpdateHelper observer(&provider_, &wrapped_provider_,
                                              updated_item);

  EXPECT_CALL(observer, OnItemsAvailable(&provider_)).Times(1);

  wrapped_provider_.NotifyOnItemsAvailable();

  {
    wrapped_provider_.NotifyOnItemUpdated(item);
    EXPECT_CALL(observer, OnItemUpdated(item)).Times(1);
    task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  }

  {
    EXPECT_CALL(observer, OnItemUpdated(updated_item)).Times(1);
    task_runner_->FastForwardUntilNoTasksRemain();
  }
}

}  // namespace
}  // namespace offline_items_collection;
