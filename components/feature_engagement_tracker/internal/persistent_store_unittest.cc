// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/persistent_store.h"

#include <map>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/test/histogram_tester.h"
#include "components/feature_engagement_tracker/internal/proto/event.pb.h"
#include "components/feature_engagement_tracker/internal/stats.h"
#include "components/feature_engagement_tracker/internal/test/event_util.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement_tracker {

namespace {

void VerifyEventsInListAndMap(const std::map<std::string, Event>& map,
                              const std::vector<Event>& list) {
  ASSERT_EQ(map.size(), list.size());

  for (const auto& event : list) {
    const auto& it = map.find(event.name());
    ASSERT_NE(map.end(), it);
    test::VerifyEventsEqual(&event, &it->second);
  }
}

class PersistentStoreTest : public ::testing::Test {
 public:
  PersistentStoreTest()
      : db_(nullptr),
        storage_dir_(FILE_PATH_LITERAL("/persistent/store/lalala")) {
    load_callback_ =
        base::Bind(&PersistentStoreTest::LoadCallback, base::Unretained(this));
  }

  void TearDown() override {
    db_events_.clear();
    db_ = nullptr;
    store_.reset();
  }

 protected:
  void SetUpDB() {
    DCHECK(!db_);
    DCHECK(!store_);

    auto db = base::MakeUnique<leveldb_proto::test::FakeDB<Event>>(&db_events_);
    db_ = db.get();
    store_.reset(new PersistentStore(storage_dir_, std::move(db)));
  }

  void LoadCallback(bool success, std::unique_ptr<std::vector<Event>> events) {
    load_successful_ = success;
    load_results_ = std::move(events);
  }

  // Callback results.
  base::Optional<bool> load_successful_;
  std::unique_ptr<std::vector<Event>> load_results_;

  Store::OnLoadedCallback load_callback_;
  std::map<std::string, Event> db_events_;
  leveldb_proto::test::FakeDB<Event>* db_;
  std::unique_ptr<Store> store_;

  // Constant test data.
  base::FilePath storage_dir_;
};

}  // namespace

TEST_F(PersistentStoreTest, StorageDirectory) {
  SetUpDB();
  store_->Load(load_callback_);
  EXPECT_EQ(storage_dir_, db_->GetDirectory());
}

TEST_F(PersistentStoreTest, SuccessfulInitAndLoadEmptyStore) {
  SetUpDB();

  base::HistogramTester histogram_tester;

  store_->Load(load_callback_);
  // The initialize should not trigger a response to the callback.
  db_->InitCallback(true);
  EXPECT_FALSE(load_successful_.has_value());

  // The load should trigger a response to the callback.
  db_->LoadCallback(true);
  EXPECT_TRUE(load_successful_.value());

  // Validate that we have no entries.
  EXPECT_NE(nullptr, load_results_);
  EXPECT_TRUE(load_results_->empty());

  // Verify histograms.
  std::string suffix =
      stats::ToDbHistogramSuffix(stats::StoreType::EVENTS_STORE);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Init." + suffix, 1, 1);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Load." + suffix, 1, 1);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.TotalEvents", 0, 1);
}

TEST_F(PersistentStoreTest, SuccessfulInitAndLoadWithEvents) {
  // Populate fake Event entries.
  Event event1;
  event1.set_name("event1");
  test::SetEventCountForDay(&event1, 1, 1);

  Event event2;
  event2.set_name("event2");
  test::SetEventCountForDay(&event2, 1, 3);
  test::SetEventCountForDay(&event2, 2, 5);

  db_events_.insert(std::pair<std::string, Event>(event1.name(), event1));
  db_events_.insert(std::pair<std::string, Event>(event2.name(), event2));

  SetUpDB();

  base::HistogramTester histogram_tester;

  // The initialize should not trigger a response to the callback.
  store_->Load(load_callback_);
  db_->InitCallback(true);
  EXPECT_FALSE(load_successful_.has_value());

  // The load should trigger a response to the callback.
  db_->LoadCallback(true);
  EXPECT_TRUE(load_successful_.value());
  EXPECT_NE(nullptr, load_results_);

  // Validate that we have the two events that we expect.
  VerifyEventsInListAndMap(db_events_, *load_results_);

  // Verify histograms.
  std::string suffix =
      stats::ToDbHistogramSuffix(stats::StoreType::EVENTS_STORE);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Init." + suffix, 1, 1);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Load." + suffix, 1, 1);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.TotalEvents", 3, 1);
}

TEST_F(PersistentStoreTest, SuccessfulInitBadLoad) {
  base::HistogramTester histogram_tester;
  SetUpDB();

  store_->Load(load_callback_);

  // The initialize should not trigger a response to the callback.
  db_->InitCallback(true);
  EXPECT_FALSE(load_successful_.has_value());

  // The load will fail and should trigger the callback.
  db_->LoadCallback(false);
  EXPECT_FALSE(load_successful_.value());
  EXPECT_FALSE(store_->IsReady());

  // Histograms.
  std::string suffix =
      stats::ToDbHistogramSuffix(stats::StoreType::EVENTS_STORE);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Init." + suffix, 1, 1);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Load." + suffix, 0, 1);
  histogram_tester.ExpectTotalCount("InProductHelp.Db.TotalEvents", 0);
}

TEST_F(PersistentStoreTest, BadInit) {
  base::HistogramTester histogram_tester;
  SetUpDB();

  store_->Load(load_callback_);

  // The initialize will fail and should trigger the callback.
  db_->InitCallback(false);
  EXPECT_FALSE(load_successful_.value());
  EXPECT_FALSE(store_->IsReady());

  // Histograms.
  std::string suffix =
      stats::ToDbHistogramSuffix(stats::StoreType::EVENTS_STORE);
  histogram_tester.ExpectBucketCount("InProductHelp.Db.Init." + suffix, 0, 1);
  histogram_tester.ExpectTotalCount("InProductHelp.Db.Load." + suffix, 0);
  histogram_tester.ExpectTotalCount("InProductHelp.Db.TotalEvents", 0);
}

TEST_F(PersistentStoreTest, IsReady) {
  SetUpDB();
  EXPECT_FALSE(store_->IsReady());

  store_->Load(load_callback_);
  EXPECT_FALSE(store_->IsReady());

  db_->InitCallback(true);
  EXPECT_FALSE(store_->IsReady());

  db_->LoadCallback(true);
  EXPECT_TRUE(store_->IsReady());
}

TEST_F(PersistentStoreTest, WriteEvent) {
  SetUpDB();

  store_->Load(load_callback_);
  db_->InitCallback(true);
  db_->LoadCallback(true);

  Event event;
  event.set_name("event");
  test::SetEventCountForDay(&event, 1, 2);

  store_->WriteEvent(event);
  db_->UpdateCallback(true);

  EXPECT_EQ(1U, db_events_.size());

  const auto& it = db_events_.find("event");
  EXPECT_NE(db_events_.end(), it);
  test::VerifyEventsEqual(&event, &it->second);
}

TEST_F(PersistentStoreTest, WriteAndDeleteEvent) {
  SetUpDB();

  store_->Load(load_callback_);
  db_->InitCallback(true);
  db_->LoadCallback(true);

  Event event;
  event.set_name("event");
  test::SetEventCountForDay(&event, 1, 2);

  store_->WriteEvent(event);
  db_->UpdateCallback(true);

  EXPECT_EQ(1U, db_events_.size());

  store_->DeleteEvent("event");
  db_->UpdateCallback(true);

  const auto& it = db_events_.find("event");
  EXPECT_EQ(db_events_.end(), it);
}

}  // namespace feature_engagement_tracker
