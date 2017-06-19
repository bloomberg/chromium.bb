// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/typed_url_sync_bridge.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/history/core/test/test_history_database.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/recording_model_type_change_processor.h"
#include "components/sync/model/sync_metadata_store.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::TypedUrlSpecifics;
using syncer::DataBatch;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::EntityDataPtr;
using syncer::KeyAndData;
using syncer::MetadataBatch;
using syncer::RecordingModelTypeChangeProcessor;

namespace history {

namespace {

// Visits with this timestamp are treated as expired.
const int kExpiredVisit = -1;

// Helper constants for tests.
const char kTitle[] = "pie";
const char kTitle2[] = "cookie";
const char kURL[] = "http://pie.com/";
const char kURL2[] = "http://cookie.com/";

// Create a new row object and the typed visit çorresponding with the time at
// |last_visit| in the |visits| vector.
URLRow MakeTypedUrlRow(const std::string& url,
                       const std::string& title,
                       int typed_count,
                       int64_t last_visit,
                       bool hidden,
                       VisitVector* visits) {
  // Give each URL a unique ID, to mimic the behavior of the real database.
  GURL gurl(url);
  URLRow history_url(gurl);
  history_url.set_title(base::UTF8ToUTF16(title));
  history_url.set_typed_count(typed_count);
  history_url.set_hidden(hidden);

  base::Time last_visit_time = base::Time::FromInternalValue(last_visit);
  history_url.set_last_visit(last_visit_time);

  if (typed_count > 0) {
    // Add a typed visit for time |last_visit|.
    visits->push_back(VisitRow(history_url.id(), last_visit_time, 0,
                               ui::PAGE_TRANSITION_TYPED, 0));
  } else {
    // Add a non-typed visit for time |last_visit|.
    visits->push_back(VisitRow(history_url.id(), last_visit_time, 0,
                               ui::PAGE_TRANSITION_RELOAD, 0));
  }

  history_url.set_visit_count(visits->size());
  return history_url;
}

void VerifyEqual(const TypedUrlSpecifics& s1, const TypedUrlSpecifics& s2) {
  // Instead of just comparing serialized strings, manually check fields to show
  // differences on failure.
  EXPECT_EQ(s1.url(), s2.url());
  EXPECT_EQ(s1.title(), s2.title());
  EXPECT_EQ(s1.hidden(), s2.hidden());
  EXPECT_EQ(s1.visits_size(), s2.visits_size());
  EXPECT_EQ(s1.visit_transitions_size(), s2.visit_transitions_size());
  EXPECT_EQ(s1.visits_size(), s1.visit_transitions_size());
  int size = s1.visits_size();
  for (int i = 0; i < size; ++i) {
    EXPECT_EQ(s1.visits(i), s2.visits(i)) << "visits differ at index " << i;
    EXPECT_EQ(s1.visit_transitions(i), s2.visit_transitions(i))
        << "visit_transitions differ at index " << i;
  }
}

void VerifyDataBatch(std::map<std::string, TypedUrlSpecifics> expected,
                     std::unique_ptr<DataBatch> batch) {
  while (batch->HasNext()) {
    const KeyAndData& pair = batch->Next();
    auto iter = expected.find(pair.first);
    ASSERT_NE(iter, expected.end());
    VerifyEqual(iter->second, pair.second->specifics.typed_url());
    // Removing allows us to verify we don't see the same item multiple times,
    // and that we saw everything we expected.
    expected.erase(iter);
  }
  EXPECT_TRUE(expected.empty());
}

class TestHistoryBackendDelegate : public HistoryBackend::Delegate {
 public:
  TestHistoryBackendDelegate() {}

  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override {}
  void SetInMemoryBackend(
      std::unique_ptr<InMemoryHistoryBackend> backend) override {}
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {}
  void NotifyURLVisited(ui::PageTransition transition,
                        const URLRow& row,
                        const RedirectList& redirects,
                        base::Time visit_time) override {}
  void NotifyURLsModified(const URLRows& changed_urls) override {}
  void NotifyURLsDeleted(bool all_history,
                         bool expired,
                         const URLRows& deleted_rows,
                         const std::set<GURL>& favicon_urls) override {}
  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const base::string16& term) override {}
  void NotifyKeywordSearchTermDeleted(URLID url_id) override {}
  void DBLoaded() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestHistoryBackendDelegate);
};

class TestHistoryBackend : public HistoryBackend {
 public:
  TestHistoryBackend()
      : HistoryBackend(new TestHistoryBackendDelegate(),
                       nullptr,
                       base::ThreadTaskRunnerHandle::Get()) {}

  bool IsExpiredVisitTime(const base::Time& time) override {
    return time.ToInternalValue() == kExpiredVisit;
  }

  URLID GetIdByUrl(const GURL& gurl) {
    return db()->GetRowForURL(gurl, nullptr);
  }

  void SetVisitsForUrl(URLRow& new_url, const VisitVector visits) {
    std::vector<history::VisitInfo> added_visits;
    URLRows new_urls;
    DeleteURL(new_url.url());
    for (const auto& visit : visits) {
      added_visits.push_back(
          history::VisitInfo(visit.visit_time, visit.transition));
    }
    new_urls.push_back(new_url);
    AddPagesWithDetails(new_urls, history::SOURCE_SYNCED);
    AddVisits(new_url.url(), added_visits, history::SOURCE_SYNCED);
    new_url.set_id(GetIdByUrl(new_url.url()));
  }

 private:
  ~TestHistoryBackend() override {}
};

}  // namespace

class TypedURLSyncBridgeTest : public testing::Test {
 public:
  TypedURLSyncBridgeTest() : typed_url_sync_bridge_(NULL) {}
  ~TypedURLSyncBridgeTest() override {}

  void SetUp() override {
    fake_history_backend_ = new TestHistoryBackend();
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    fake_history_backend_->Init(
        false, TestHistoryDatabaseParamsForPath(test_dir_.GetPath()));
    std::unique_ptr<TypedURLSyncBridge> bridge =
        base::MakeUnique<TypedURLSyncBridge>(
            fake_history_backend_.get(), fake_history_backend_->db(),
            RecordingModelTypeChangeProcessor::FactoryForBridgeTest(&processor_,
                                                                    false));
    typed_url_sync_bridge_ = bridge.get();
    typed_url_sync_bridge_->Init();
    typed_url_sync_bridge_->history_backend_observer_.RemoveAll();
    fake_history_backend_->SetTypedURLSyncBridgeForTest(std::move(bridge));
  }

  void TearDown() override { fake_history_backend_->Closing(); }

  // Starts sync for |typed_url_sync_bridge_| with |initial_data| as the
  // initial sync data.
  void StartSyncing(const std::vector<TypedUrlSpecifics>& specifics) {
    // Set change processor.
    const auto error =
        bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(),
                                CreateEntityChangeList(specifics));

    EXPECT_FALSE(error);
  }

  // Fills |specifics| with the sync data for |url| and |visits|.
  static bool WriteToTypedUrlSpecifics(const URLRow& url,
                                       const VisitVector& visits,
                                       TypedUrlSpecifics* specifics) {
    return TypedURLSyncBridge::WriteToTypedUrlSpecifics(url, visits, specifics);
  }

  std::string GetStorageKey(const TypedUrlSpecifics& specifics) {
    std::string key = bridge()->GetStorageKeyInternal(specifics.url());
    return key;
  }

  EntityDataPtr SpecificsToEntity(const TypedUrlSpecifics& specifics) {
    EntityData data;
    data.client_tag_hash = "ignored";
    *data.specifics.mutable_typed_url() = specifics;
    return data.PassToPtr();
  }

  EntityChangeList CreateEntityChangeList(
      const std::vector<TypedUrlSpecifics>& specifics_vector) {
    EntityChangeList entity_change_list;
    for (const auto& specifics : specifics_vector) {
      entity_change_list.push_back(EntityChange::CreateAdd(
          GetStorageKey(specifics), SpecificsToEntity(specifics)));
    }
    return entity_change_list;
  }

  std::map<std::string, TypedUrlSpecifics> ExpectedMap(
      const std::vector<TypedUrlSpecifics>& specifics_vector) {
    std::map<std::string, TypedUrlSpecifics> map;
    for (const auto& specifics : specifics_vector) {
      map[GetStorageKey(specifics)] = specifics;
    }
    return map;
  }

  void VerifyLocalHistoryData(const std::vector<TypedUrlSpecifics>& expected) {
    bridge()->GetAllData(base::Bind(&VerifyDataBatch, ExpectedMap(expected)));
  }

  TypedURLSyncBridge* bridge() { return typed_url_sync_bridge_; }

  TypedURLSyncMetadataDatabase* metadata_store() {
    return bridge()->sync_metadata_database_;
  }

  const RecordingModelTypeChangeProcessor& processor() { return *processor_; }

 protected:
  base::MessageLoop message_loop_;
  base::ScopedTempDir test_dir_;
  scoped_refptr<TestHistoryBackend> fake_history_backend_;
  TypedURLSyncBridge* typed_url_sync_bridge_;
  // A non-owning pointer to the processor given to the bridge. Will be null
  // before being given to the bridge, to make ownership easier.
  RecordingModelTypeChangeProcessor* processor_ = nullptr;
};

// Add two typed urls locally and verify bridge can get them from GetAllData.
TEST_F(TypedURLSyncBridgeTest, GetAllData) {
  // Add two urls to backend.
  VisitVector visits1, visits2;
  URLRow row1 = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits1);
  URLRow row2 = MakeTypedUrlRow(kURL2, kTitle2, 2, 4, false, &visits2);
  fake_history_backend_->SetVisitsForUrl(row1, visits1);
  fake_history_backend_->SetVisitsForUrl(row2, visits2);

  // Create the same data in sync.
  TypedUrlSpecifics typed_url1, typed_url2;
  WriteToTypedUrlSpecifics(row1, visits1, &typed_url1);
  WriteToTypedUrlSpecifics(row2, visits2, &typed_url2);

  // Check that the local cache is still correct.
  VerifyLocalHistoryData({typed_url1, typed_url2});
}

// Add a typed url locally and one to sync with the same data. Starting sync
// should result in no changes.
TEST_F(TypedURLSyncBridgeTest, MergeUrlNoChange) {
  // Add a url to backend.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  fake_history_backend_->SetVisitsForUrl(row, visits);

  // Create the same data in sync.
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(row, visits, typed_url);

  StartSyncing({*typed_url});
  EXPECT_TRUE(processor().put_multimap().empty());

  // Check that the local cache was is still correct.
  VerifyLocalHistoryData({*typed_url});
}

// Add a corupted typed url locally, has typed url count 1, but no real typed
// url visit. Starting sync should not pick up this url.
TEST_F(TypedURLSyncBridgeTest, MergeUrlNoTypedUrl) {
  // Add a url to backend.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 0, 3, false, &visits);

  // Mark typed_count to 1 even when there is no typed url visit.
  row.set_typed_count(1);
  fake_history_backend_->SetVisitsForUrl(row, visits);

  StartSyncing(std::vector<TypedUrlSpecifics>());
  EXPECT_TRUE(processor().put_multimap().empty());

  MetadataBatch metadata_batch;
  metadata_store()->GetAllSyncMetadata(&metadata_batch);
  EXPECT_EQ(0u, metadata_batch.TakeAllMetadata().size());
}

// Starting sync with no sync data should just push the local url to sync.
TEST_F(TypedURLSyncBridgeTest, MergeUrlEmptySync) {
  // Add a url to backend.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  fake_history_backend_->SetVisitsForUrl(row, visits);

  StartSyncing(std::vector<TypedUrlSpecifics>());

  // Check that the local cache was is still correct.
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(row, visits, typed_url);
  VerifyLocalHistoryData({*typed_url});

  // Check that the server was updated correctly.
  ASSERT_EQ(1U, processor().put_multimap().size());
  auto recorded_specifics_iterator =
      processor().put_multimap().find(GetStorageKey(*typed_url));
  EXPECT_NE(processor().put_multimap().end(), recorded_specifics_iterator);
  TypedUrlSpecifics recorded_specifics =
      recorded_specifics_iterator->second->specifics.typed_url();

  ASSERT_EQ(1, recorded_specifics.visits_size());
  EXPECT_EQ(3, recorded_specifics.visits(0));
  ASSERT_EQ(1, recorded_specifics.visit_transitions_size());
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            recorded_specifics.visit_transitions(0));
}

// Starting sync with no local data should just push the synced url into the
// backend.
TEST_F(TypedURLSyncBridgeTest, MergeUrlEmptyLocal) {
  // Create the sync data.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(row, visits, typed_url);

  StartSyncing({*typed_url});
  EXPECT_EQ(0u, processor().put_multimap().size());

  // Check that the backend was updated correctly.
  VisitVector all_visits;
  base::Time server_time = base::Time::FromInternalValue(3);
  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  ASSERT_EQ(1U, all_visits.size());
  EXPECT_EQ(server_time, all_visits[0].visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits[0].transition, visits[0].transition));
}

// Starting sync with both local and sync have same typed URL, but different
// visit. After merge, both local and sync should have two same visits.
TEST_F(TypedURLSyncBridgeTest, SimpleMerge) {
  // Add a url to backend.
  VisitVector visits1;
  URLRow row1 = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits1);
  fake_history_backend_->SetVisitsForUrl(row1, visits1);

  // Create the sync data.
  VisitVector visits2;
  URLRow row2 = MakeTypedUrlRow(kURL, kTitle, 1, 4, false, &visits2);
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(row2, visits2, typed_url);

  StartSyncing({*typed_url});
  EXPECT_EQ(1u, processor().put_multimap().size());

  // Check that the backend was updated correctly.
  VisitVector all_visits;
  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  ASSERT_EQ(2U, all_visits.size());
  EXPECT_EQ(base::Time::FromInternalValue(3), all_visits[0].visit_time);
  EXPECT_EQ(base::Time::FromInternalValue(4), all_visits[1].visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits[0].transition, visits1[0].transition));
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits[1].transition, visits2[0].transition));
}

}  // namespace history
