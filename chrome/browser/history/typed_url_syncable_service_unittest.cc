// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/typed_url_syncable_service.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_types.h"
#include "content/public/browser/notification_types.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/typed_url_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using history::HistoryBackend;
using history::URLID;
using history::URLRow;
using history::URLRows;
using history::VisitRow;
using history::VisitVector;

namespace {

// Constants used to limit size of visits processed.
static const int kMaxTypedUrlVisits = 100;
static const int kMaxVisitsToFetch = 1000;

// Visits with this timestamp are treated as expired.
static const int EXPIRED_VISIT = -1;

// TestChangeProcessor --------------------------------------------------------

class TestChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  TestChangeProcessor() : change_output_(NULL) {}

  // syncer::SyncChangeProcessor implementation.
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE {
    change_output_->insert(change_output_->end(), change_list.begin(),
                           change_list.end());
    return syncer::SyncError();
  }

  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const
      OVERRIDE {
    return syncer::SyncDataList();
  }

  // Set pointer location to write SyncChanges to in ProcessSyncChanges.
  void SetChangeOutput(syncer::SyncChangeList *change_output) {
    change_output_ = change_output;
  }

 private:
  syncer::SyncChangeList *change_output_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeProcessor);
};

// TestHistoryBackend ----------------------------------------------------------

class TestHistoryBackend : public HistoryBackend {
 public:
  TestHistoryBackend() : HistoryBackend(base::FilePath(), 0, NULL, NULL) {}

  // HistoryBackend test implementation.
  virtual bool IsExpiredVisitTime(const base::Time& time) OVERRIDE {
    return time.ToInternalValue() == EXPIRED_VISIT;
  }

  virtual bool GetMostRecentVisitsForURL(
      URLID id,
      int max_visits,
      VisitVector* visits) OVERRIDE {
    if (local_db_visits_[id].empty())
      return false;

    visits->insert(visits->end(),
                   local_db_visits_[id].begin(),
                   local_db_visits_[id].end());
    return true;
  }

  // Helpers.
  void SetVisitsForUrl(URLID id, VisitVector* visits) {
    if (!local_db_visits_[id].empty()) {
      local_db_visits_[id].clear();
    }

    local_db_visits_[id].insert(local_db_visits_[id].end(),
                                visits->begin(),
                                visits->end());
  }

  void DeleteVisitsForUrl(const URLID& id) {
    local_db_visits_.erase(id);
  }

 private:
  virtual ~TestHistoryBackend() {}

  // Mock of visit table in local db.
  std::map<URLID, VisitVector> local_db_visits_;
};

}  // namespace

namespace history {

// TypedUrlSyncableServiceTest -------------------------------------------------

class TypedUrlSyncableServiceTest : public testing::Test {
 public:
  // Create a new row object and add a typed visit to the |visits| vector.
  // Note that the real history db returns visits in reverse chronological
  // order, so |visits| is treated this way. If the newest (first) visit
  // in visits does not match |last_visit|, then a typed visit for this
  // time is prepended to the front (or if |last_visit| is too old, it is
  // set equal to the time of the newest visit).
  static URLRow MakeTypedUrlRow(const char* url,
                                const char* title,
                                int typed_count,
                                int64 last_visit,
                                bool hidden,
                                VisitVector* visits);

  static void AddNewestVisit(URLRow* url,
                             VisitVector* visits,
                             content::PageTransition transition,
                             int64 visit_time);

  static void AddOldestVisit(URLRow* url,
                             VisitVector* visits,
                             content::PageTransition transition,
                             int64 visit_time);

  static bool URLsEqual(URLRow& row,
                        sync_pb::TypedUrlSpecifics& specifics) {
    return ((row.url().spec().compare(specifics.url()) == 0) &&
            (UTF16ToUTF8(row.title()).compare(specifics.title()) == 0) &&
            (row.hidden() == specifics.hidden()));
  }

  bool InitiateServerState(
      unsigned int num_typed_urls,
      unsigned int num_reload_urls,
      URLRows* rows,
      std::vector<VisitVector>* visit_vectors,
      const std::vector<const char*>& urls,
      syncer::SyncChangeList* change_list);

 protected:
  TypedUrlSyncableServiceTest() {}

  virtual ~TypedUrlSyncableServiceTest() {}

  virtual void SetUp() OVERRIDE {
    fake_history_backend_ = new TestHistoryBackend();
    typed_url_sync_service_.reset(
        new TypedUrlSyncableService(fake_history_backend_.get()));
    fake_change_processor_.reset(new TestChangeProcessor);
  }

  scoped_refptr<HistoryBackend> fake_history_backend_;
  scoped_ptr<TypedUrlSyncableService> typed_url_sync_service_;
  scoped_ptr<syncer::SyncChangeProcessor> fake_change_processor_;
};

URLRow TypedUrlSyncableServiceTest::MakeTypedUrlRow(
    const char* url,
    const char* title,
    int typed_count,
    int64 last_visit,
    bool hidden,
    VisitVector* visits) {
  DCHECK(visits->empty());

  // Give each URL a unique ID, to mimic the behavior of the real database.
  static int unique_url_id = 0;
  GURL gurl(url);
  URLRow history_url(gurl, ++unique_url_id);
  history_url.set_title(UTF8ToUTF16(title));
  history_url.set_typed_count(typed_count);
  history_url.set_hidden(hidden);

  base::Time last_visit_time = base::Time::FromInternalValue(last_visit);
  history_url.set_last_visit(last_visit_time);

  VisitVector::iterator first = visits->begin();
  if (typed_count > 0) {
    // Add a typed visit for time |last_visit|.
    visits->insert(first,
                   VisitRow(history_url.id(), last_visit_time, 0,
                            content::PAGE_TRANSITION_TYPED, 0));
  } else {
    // Add a non-typed visit for time |last_visit|.
    visits->insert(first,
                   VisitRow(history_url.id(), last_visit_time, 0,
                            content::PAGE_TRANSITION_RELOAD, 0));
  }

  history_url.set_visit_count(visits->size());
  return history_url;
}

void TypedUrlSyncableServiceTest::AddNewestVisit(
    URLRow* url,
    VisitVector* visits,
    content::PageTransition transition,
    int64 visit_time) {
  base::Time time = base::Time::FromInternalValue(visit_time);
  visits->insert(visits->begin(),
                 VisitRow(url->id(), time, 0, transition, 0));

  if (transition == content::PAGE_TRANSITION_TYPED) {
    url->set_typed_count(url->typed_count() + 1);
  }

  url->set_last_visit(time);
  url->set_visit_count(visits->size());
}

void TypedUrlSyncableServiceTest::AddOldestVisit(
    URLRow* url,
    VisitVector* visits,
    content::PageTransition transition,
    int64 visit_time) {
  base::Time time = base::Time::FromInternalValue(visit_time);
  visits->push_back(VisitRow(url->id(), time, 0, transition, 0));

  if (transition == content::PAGE_TRANSITION_TYPED) {
    url->set_typed_count(url->typed_count() + 1);
  }

  url->set_visit_count(visits->size());
}

bool TypedUrlSyncableServiceTest::InitiateServerState(
    unsigned int num_typed_urls,
    unsigned int num_reload_urls,
    URLRows* rows,
    std::vector<VisitVector>* visit_vectors,
    const std::vector<const char*>& urls,
    syncer::SyncChangeList* change_list) {
  unsigned int total_urls = num_typed_urls + num_reload_urls;
  DCHECK(urls.size() >= total_urls);
  if (!typed_url_sync_service_.get())
    return false;

  static_cast<TestChangeProcessor*>(fake_change_processor_.get())->
      SetChangeOutput(change_list);

  // Set change processor.
  syncer::SyncMergeResult result =
      typed_url_sync_service_->MergeDataAndStartSyncing(
          syncer::TYPED_URLS,
          syncer::SyncDataList(),
          fake_change_processor_.Pass(),
          scoped_ptr<syncer::SyncErrorFactory>(
              new syncer::SyncErrorFactoryMock()));
  EXPECT_FALSE(result.error().IsSet()) << result.error().message();

  if (total_urls) {
    // Create new URL rows, populate the mock backend with its visits, and
    // send to the sync service.
    URLRows changed_urls;

    for (unsigned int i = 0; i < total_urls; ++i) {
      int typed = i < num_typed_urls ? 1 : 0;
      VisitVector visits;
      visit_vectors->push_back(visits);
      rows->push_back(MakeTypedUrlRow(
          urls[i], "pie", typed, i + 3, false, &visit_vectors->back()));
      static_cast<TestHistoryBackend*>(fake_history_backend_.get())->
          SetVisitsForUrl(rows->back().id(), &visit_vectors->back());
      changed_urls.push_back(rows->back());
    }

    typed_url_sync_service_->OnUrlsModified(&changed_urls);
  }
  // Check that communication with sync was successful.
  if (num_typed_urls != change_list->size())
    return false;
  return true;
}

TEST_F(TypedUrlSyncableServiceTest, AddLocalTypedUrlAndSync) {
  // Create a local typed URL (simulate a typed visit) that is not already
  // in sync. Check that sync is sent an ADD change for the existing URL.
  syncer::SyncChangeList change_list;

  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<const char*> urls;
  urls.push_back("http://pie.com/");

  ASSERT_TRUE(
      InitiateServerState(1, 0, &url_rows, &visit_vectors, urls, &change_list));

  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  // Check change processor.
  ASSERT_EQ(1u, change_list.size());
  ASSERT_TRUE(change_list[0].IsValid());
  EXPECT_EQ(syncer::TYPED_URLS, change_list[0].sync_data().GetDataType());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change_list[0].change_type());

  // Get typed url specifics.
  sync_pb::TypedUrlSpecifics url_specifics =
      change_list[0].sync_data().GetSpecifics().typed_url();

  EXPECT_TRUE(URLsEqual(url_row, url_specifics));
  ASSERT_EQ(1, url_specifics.visits_size());
  ASSERT_EQ(static_cast<const int>(visits.size()), url_specifics.visits_size());
  EXPECT_EQ(visits[0].visit_time.ToInternalValue(), url_specifics.visits(0));
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(0));

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  typed_url_sync_service_.get()->GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(1u, sync_state.size());
  EXPECT_TRUE(sync_state.end() != sync_state.find(url_row.url()));
}

TEST_F(TypedUrlSyncableServiceTest, UpdateLocalTypedUrlAndSync) {
  syncer::SyncChangeList change_list;

  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<const char*> urls;
  urls.push_back("http://pie.com/");

  ASSERT_TRUE(
      InitiateServerState(1, 0, &url_rows, &visit_vectors, urls, &change_list));
  change_list.clear();

  // Update the URL row, adding another typed visit to the visit vector.
  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  URLRows changed_urls;
  AddNewestVisit(&url_row, &visits, content::PAGE_TRANSITION_TYPED, 7);
  static_cast<TestHistoryBackend*>(fake_history_backend_.get())->
      SetVisitsForUrl(url_row.id(), &visits);
  changed_urls.push_back(url_row);

  // Notify typed url sync service of the update.
  typed_url_sync_service_->OnUrlsModified(&changed_urls);

  ASSERT_EQ(1u, change_list.size());
  ASSERT_TRUE(change_list[0].IsValid());
  EXPECT_EQ(syncer::TYPED_URLS, change_list[0].sync_data().GetDataType());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change_list[0].change_type());

  sync_pb::TypedUrlSpecifics url_specifics =
      change_list[0].sync_data().GetSpecifics().typed_url();

  EXPECT_TRUE(URLsEqual(url_row, url_specifics));
  ASSERT_EQ(2, url_specifics.visits_size());
  ASSERT_EQ(static_cast<const int>(visits.size()), url_specifics.visits_size());

  // Check that each visit has been translated/communicated correctly.
  // Note that the specifics record visits in chronological order, and the
  // visits from the db are in reverse chronological order.
  EXPECT_EQ(visits[0].visit_time.ToInternalValue(), url_specifics.visits(1));
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(1));
  EXPECT_EQ(visits[1].visit_time.ToInternalValue(), url_specifics.visits(0));
  EXPECT_EQ(static_cast<const int>(visits[1].transition),
            url_specifics.visit_transitions(0));

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  typed_url_sync_service_.get()->GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(1u, sync_state.size());
  EXPECT_TRUE(sync_state.end() != sync_state.find(url_row.url()));
}

TEST_F(TypedUrlSyncableServiceTest, LinkVisitLocalTypedUrlAndSync) {
  syncer::SyncChangeList change_list;

  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<const char*> urls;
  urls.push_back("http://pie.com/");

  ASSERT_TRUE(
      InitiateServerState(1, 0, &url_rows, &visit_vectors, urls, &change_list));
  change_list.clear();

  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  // Update the URL row, adding a non-typed visit to the visit vector.
  AddNewestVisit(&url_row, &visits, content::PAGE_TRANSITION_LINK, 6);
  static_cast<TestHistoryBackend*>(fake_history_backend_.get())->
      SetVisitsForUrl(url_row.id(), &visits);

  content::PageTransition transition = content::PAGE_TRANSITION_LINK;
  // Notify typed url sync service of non-typed visit, expect no change.
  typed_url_sync_service_->OnUrlVisited(transition, &url_row);
  ASSERT_EQ(0u, change_list.size());
}

TEST_F(TypedUrlSyncableServiceTest, TypedVisitLocalTypedUrlAndSync) {
  syncer::SyncChangeList change_list;

  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<const char*> urls;
  urls.push_back("http://pie.com/");

  ASSERT_TRUE(
      InitiateServerState(1, 0, &url_rows, &visit_vectors, urls, &change_list));
  change_list.clear();

  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  // Update the URL row, adding another typed visit to the visit vector.
  AddOldestVisit(&url_row, &visits, content::PAGE_TRANSITION_LINK, 1);
  AddNewestVisit(&url_row, &visits, content::PAGE_TRANSITION_LINK, 6);
  AddNewestVisit(&url_row, &visits, content::PAGE_TRANSITION_TYPED, 7);
  static_cast<TestHistoryBackend*>(fake_history_backend_.get())->
      SetVisitsForUrl(url_row.id(), &visits);

  // Notify typed url sync service of typed visit.
  content::PageTransition transition = content::PAGE_TRANSITION_TYPED;
  typed_url_sync_service_->OnUrlVisited(transition, &url_row);

  ASSERT_EQ(1u, change_list.size());
  ASSERT_TRUE(change_list[0].IsValid());
  EXPECT_EQ(syncer::TYPED_URLS, change_list[0].sync_data().GetDataType());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change_list[0].change_type());

  sync_pb::TypedUrlSpecifics url_specifics =
      change_list[0].sync_data().GetSpecifics().typed_url();

  EXPECT_TRUE(URLsEqual(url_row, url_specifics));
  ASSERT_EQ(4u, visits.size());
  EXPECT_EQ(static_cast<const int>(visits.size()), url_specifics.visits_size());

  // Check that each visit has been translated/communicated correctly.
  // Note that the specifics record visits in chronological order, and the
  // visits from the db are in reverse chronological order.
  int r = url_specifics.visits_size() - 1;
  for (int i = 0; i < url_specifics.visits_size(); ++i, --r) {
    EXPECT_EQ(visits[i].visit_time.ToInternalValue(), url_specifics.visits(r));
    EXPECT_EQ(static_cast<const int>(visits[i].transition),
              url_specifics.visit_transitions(r));
  }

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  typed_url_sync_service_.get()->GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(1u, sync_state.size());
  EXPECT_TRUE(sync_state.end() != sync_state.find(url_row.url()));
}

TEST_F(TypedUrlSyncableServiceTest, DeleteLocalTypedUrlAndSync) {
  syncer::SyncChangeList change_list;

  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<const char*> urls;
  urls.push_back("http://pie.com/");
  urls.push_back("http://cake.com/");
  urls.push_back("http://google.com/");
  urls.push_back("http://foo.com/");
  urls.push_back("http://bar.com/");

  ASSERT_TRUE(
      InitiateServerState(4, 1, &url_rows, &visit_vectors, urls, &change_list));
  change_list.clear();

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  typed_url_sync_service_.get()->GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(4u, sync_state.size());

  // Simulate visit expiry of typed visit, no syncing is done
  // This is to test that sync relies on the in-memory cache to know
  // which urls were typed and synced, and should be deleted.
  url_rows[0].set_typed_count(0);
  VisitVector visits;
  static_cast<TestHistoryBackend*>(fake_history_backend_.get())->
      SetVisitsForUrl(url_rows[0].id(), &visits);

  // Delete some urls from backend and create deleted row vector.
  URLRows rows;
  for (size_t i = 0; i < 3u; ++i) {
    static_cast<TestHistoryBackend*>(fake_history_backend_.get())->
        DeleteVisitsForUrl(url_rows[i].id());
    rows.push_back(url_rows[i]);
  }

  // Notify typed url sync service.
  typed_url_sync_service_->OnUrlsDeleted(false, false, &rows);

  ASSERT_EQ(3u, change_list.size());
  for (size_t i = 0; i < change_list.size(); ++i) {
    ASSERT_TRUE(change_list[i].IsValid());
    ASSERT_EQ(syncer::TYPED_URLS, change_list[i].sync_data().GetDataType());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, change_list[i].change_type());
    sync_pb::TypedUrlSpecifics url_specifics =
        change_list[i].sync_data().GetSpecifics().typed_url();
    EXPECT_EQ(url_rows[i].url().spec(), url_specifics.url());
  }

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state_deleted;
  typed_url_sync_service_.get()->GetSyncedUrls(&sync_state_deleted);
  ASSERT_EQ(1u, sync_state_deleted.size());
  EXPECT_TRUE(sync_state_deleted.end() !=
              sync_state_deleted.find(url_rows[3].url()));
}

TEST_F(TypedUrlSyncableServiceTest, DeleteAllLocalTypedUrlAndSync) {
  syncer::SyncChangeList change_list;

  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<const char*> urls;
  urls.push_back("http://pie.com/");
  urls.push_back("http://cake.com/");
  urls.push_back("http://google.com/");
  urls.push_back("http://foo.com/");
  urls.push_back("http://bar.com/");

  ASSERT_TRUE(
      InitiateServerState(4, 1, &url_rows, &visit_vectors, urls, &change_list));
  change_list.clear();

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  typed_url_sync_service_.get()->GetSyncedUrls(&sync_state);
  EXPECT_EQ(4u, sync_state.size());

  // Delete urls from backend.
  for (size_t i = 0; i < 4u; ++ i) {
    static_cast<TestHistoryBackend*>(fake_history_backend_.get())->
        DeleteVisitsForUrl(url_rows[i].id());
  }
  // Delete urls with |all_history| flag set.
  bool all_history = true;

  // Notify typed url sync service.
  typed_url_sync_service_->OnUrlsDeleted(all_history, false, NULL);

  ASSERT_EQ(4u, change_list.size());
  for (size_t i = 0; i < change_list.size(); ++i) {
    ASSERT_TRUE(change_list[i].IsValid());
    ASSERT_EQ(syncer::TYPED_URLS, change_list[i].sync_data().GetDataType());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, change_list[i].change_type());
  }
  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state_deleted;
  typed_url_sync_service_.get()->GetSyncedUrls(&sync_state_deleted);
  EXPECT_TRUE(sync_state_deleted.empty());
}

TEST_F(TypedUrlSyncableServiceTest, MaxVisitLocalTypedUrlAndSync) {
  syncer::SyncChangeList change_list;

  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<const char*> urls;
  urls.push_back("http://pie.com/");

  ASSERT_TRUE(
      InitiateServerState(0, 1, &url_rows, &visit_vectors, urls, &change_list));

  URLRow url_row = url_rows.front();
  VisitVector visits;

  // Add |kMaxTypedUrlVisits| + 10 visits to the url. The 10 oldest
  // non-typed visits are expected to be skipped.
  int i = 1;
  for (; i <= kMaxTypedUrlVisits - 20; ++i)
    AddNewestVisit(&url_row, &visits, content::PAGE_TRANSITION_TYPED, i);
  for (; i <= kMaxTypedUrlVisits; ++i)
    AddNewestVisit(&url_row, &visits, content::PAGE_TRANSITION_LINK, i);
  for (; i <= kMaxTypedUrlVisits + 10; ++i)
    AddNewestVisit(&url_row, &visits, content::PAGE_TRANSITION_TYPED, i);

  static_cast<TestHistoryBackend*>(fake_history_backend_.get())->
      SetVisitsForUrl(url_row.id(), &visits);

  // Notify typed url sync service of typed visit.
  content::PageTransition transition = content::PAGE_TRANSITION_TYPED;
  typed_url_sync_service_->OnUrlVisited(transition, &url_row);

  ASSERT_EQ(1u, change_list.size());
  ASSERT_TRUE(change_list[0].IsValid());
  sync_pb::TypedUrlSpecifics url_specifics =
      change_list[0].sync_data().GetSpecifics().typed_url();
  ASSERT_EQ(kMaxTypedUrlVisits, url_specifics.visits_size());

  // Check that each visit has been translated/communicated correctly.
  // Note that the specifics record visits in chronological order, and the
  // visits from the db are in reverse chronological order.
  int num_typed_visits_synced = 0;
  int num_other_visits_synced = 0;
  int r = url_specifics.visits_size() - 1;
  for (int i = 0; i < url_specifics.visits_size(); ++i, --r) {
    if (url_specifics.visit_transitions(i) == content::PAGE_TRANSITION_TYPED) {
      ++num_typed_visits_synced;
    } else {
      ++num_other_visits_synced;
    }
  }
  EXPECT_EQ(kMaxTypedUrlVisits - 10, num_typed_visits_synced);
  EXPECT_EQ(10, num_other_visits_synced);
}

TEST_F(TypedUrlSyncableServiceTest, ThrottleVisitLocalTypedUrlSync) {
  syncer::SyncChangeList change_list;

  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<const char*> urls;
  urls.push_back("http://pie.com/");

  ASSERT_TRUE(
      InitiateServerState(0, 1, &url_rows, &visit_vectors, urls, &change_list));

  URLRow url_row = url_rows.front();
  VisitVector visits;

  // Add enough visits to the url so that typed count is above the throttle
  // limit, and not right on the interval that gets synced.
  for (int i = 1; i < 42; ++i)
    AddNewestVisit(&url_row, &visits, content::PAGE_TRANSITION_TYPED, i);

  static_cast<TestHistoryBackend*>(fake_history_backend_.get())->
      SetVisitsForUrl(url_row.id(), &visits);

  // Notify typed url sync service of typed visit.
  content::PageTransition transition = content::PAGE_TRANSITION_TYPED;
  typed_url_sync_service_->OnUrlVisited(transition, &url_row);

  // Should throttle, so sync and local cache should not update.
  ASSERT_EQ(0u, change_list.size());
  std::set<GURL> sync_state;
  typed_url_sync_service_.get()->GetSyncedUrls(&sync_state);
  EXPECT_TRUE(sync_state.empty());
}

}  // namespace history
