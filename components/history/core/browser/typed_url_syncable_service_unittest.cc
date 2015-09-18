// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/typed_url_syncable_service.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_types.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_change_processor_wrapper_for_test.h"
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

namespace history {

namespace {

// Constants used to limit size of visits processed. See
// equivalent constants in typed_url_syncable_service.cc for descriptions.
const int kMaxTypedUrlVisits = 100;
const int kVisitThrottleThreshold = 10;
const int kVisitThrottleMultiple = 10;

// Visits with this timestamp are treated as expired.
const int kExpiredVisit = -1;

// Helper constants for tests.
const char kTitle[] = "pie";
const char kURL[] = "http://pie.com/";

bool URLsEqual(URLRow& row, sync_pb::TypedUrlSpecifics& specifics) {
  return ((row.url().spec().compare(specifics.url()) == 0) &&
          (base::UTF16ToUTF8(row.title()).compare(specifics.title()) == 0) &&
          (row.hidden() == specifics.hidden()));
}

void AddNewestVisit(ui::PageTransition transition,
                    int64 visit_time,
                    URLRow* url,
                    VisitVector* visits) {
  base::Time time = base::Time::FromInternalValue(visit_time);
  visits->insert(visits->begin(), VisitRow(url->id(), time, 0, transition, 0));

  if (transition == ui::PAGE_TRANSITION_TYPED) {
    url->set_typed_count(url->typed_count() + 1);
  }

  url->set_last_visit(time);
  url->set_visit_count(visits->size());
}

void AddOldestVisit(ui::PageTransition transition,
                    int64 visit_time,
                    URLRow* url,
                    VisitVector* visits) {
  base::Time time = base::Time::FromInternalValue(visit_time);
  visits->push_back(VisitRow(url->id(), time, 0, transition, 0));

  if (transition == ui::PAGE_TRANSITION_TYPED) {
    url->set_typed_count(url->typed_count() + 1);
  }

  url->set_visit_count(visits->size());
}

// Create a new row object and the typed visit çorresponding with the time at
// |last_visit| in the |visits| vector.
URLRow MakeTypedUrlRow(const std::string& url,
                       const std::string& title,
                       int typed_count,
                       int64 last_visit,
                       bool hidden,
                       VisitVector* visits) {
  DCHECK(visits->empty());

  // Give each URL a unique ID, to mimic the behavior of the real database.
  static int unique_url_id = 0;
  GURL gurl(url);
  URLRow history_url(gurl, ++unique_url_id);
  history_url.set_title(base::UTF8ToUTF16(title));
  history_url.set_typed_count(typed_count);
  history_url.set_hidden(hidden);

  base::Time last_visit_time = base::Time::FromInternalValue(last_visit);
  history_url.set_last_visit(last_visit_time);

  VisitVector::iterator first = visits->begin();
  if (typed_count > 0) {
    // Add a typed visit for time |last_visit|.
    visits->insert(first, VisitRow(history_url.id(), last_visit_time, 0,
                                   ui::PAGE_TRANSITION_TYPED, 0));
  } else {
    // Add a non-typed visit for time |last_visit|.
    visits->insert(first, VisitRow(history_url.id(), last_visit_time, 0,
                                   ui::PAGE_TRANSITION_RELOAD, 0));
  }

  history_url.set_visit_count(visits->size());
  return history_url;
}

class TestHistoryBackend : public HistoryBackend {
 public:
  TestHistoryBackend() : HistoryBackend(nullptr, nullptr,
                                        base::ThreadTaskRunnerHandle::Get()) {}

  bool IsExpiredVisitTime(const base::Time& time) override {
    return time.ToInternalValue() == kExpiredVisit;
  }

  bool GetMostRecentVisitsForURL(URLID id,
                                 int max_visits,
                                 VisitVector* visits) override {
    if (local_db_visits_[id].empty())
      return false;

    visits->insert(visits->end(),
                   local_db_visits_[id].begin(),
                   local_db_visits_[id].end());
    return true;
  }

  bool GetAllTypedURLs(URLRows* urls) override {
    for (std::map<GURL, URLRow>::const_iterator it = local_typed_urls_.begin();
         it != local_typed_urls_.end(); ++it) {
      urls->push_back(it->second);
    }
    return true;
  }

  bool GetURL(const GURL& url, history::URLRow* url_row) override;
  size_t UpdateURLs(const history::URLRows& urls) override;
  bool AddVisits(const GURL& url,
                 const std::vector<history::VisitInfo>& visits,
                 history::VisitSource visit_source) override;
  bool RemoveVisits(const VisitVector& visits) override;

  // Helpers.
  void SetBackendTypedUrls(URLRows* urls) {
    for (URLRows::const_iterator url = urls->begin(); url != urls->end();
         ++url) {
      local_typed_urls_[url->url()] = *url;
    }
  }

  void SetVisitsForUrl(URLID id, VisitVector* visits) {
    if (!local_db_visits_[id].empty()) {
      local_db_visits_[id].clear();
    }

    local_db_visits_[id].insert(local_db_visits_[id].end(),
                                visits->begin(),
                                visits->end());
  }

  void DeleteVisitsForUrl(const URLID& id) { local_db_visits_.erase(id); }

  const std::map<URLID, URLRow>& updated_urls() const { return updated_urls_; }

  const std::map<GURL, std::vector<history::VisitInfo>>& added_visits() const {
    return added_visits_;
  }

 private:
  ~TestHistoryBackend() override {}

  // Mock of urls in local db.
  std::map<GURL, URLRow> local_typed_urls_;
  // Mock of visit table in local db.
  std::map<URLID, history::VisitVector> local_db_visits_;

  std::map<GURL, URLRow> added_urls_;
  std::map<URLID, URLRow> updated_urls_;
  std::map<GURL, std::vector<history::VisitInfo>> added_visits_;
  history::VisitVector removed_visits_;
};

bool TestHistoryBackend::GetURL(const GURL& url, history::URLRow* url_row) {
  std::map<GURL, URLRow>::iterator it = local_typed_urls_.find(url);
  if (it != local_typed_urls_.end()) {
    url_row = &it->second;
    return true;
  }
  return false;
}

size_t TestHistoryBackend::UpdateURLs(const history::URLRows& urls) {
  for (URLRows::const_iterator it = urls.begin(); it != urls.end(); ++it) {
    updated_urls_[it->id()] = (*it);
  }

  return urls.size();
}

bool TestHistoryBackend::AddVisits(
    const GURL& url,
    const std::vector<history::VisitInfo>& visits,
    history::VisitSource visit_source) {
  DCHECK_EQ(visit_source, history::SOURCE_SYNCED);
  added_visits_[url] = visits;
  return true;
}

bool TestHistoryBackend::RemoveVisits(const VisitVector& visits) {
  removed_visits_.clear();
  removed_visits_.insert(removed_visits_.end(), visits.begin(), visits.end());
  return true;
}

}  // namespace

class TypedUrlSyncableServiceTest : public testing::Test {
 public:
  TypedUrlSyncableServiceTest() {}
  ~TypedUrlSyncableServiceTest() override {}

  void SetUp() override {
    fake_history_backend_ = new TestHistoryBackend();
    typed_url_sync_service_.reset(
        new TypedUrlSyncableService(fake_history_backend_.get()));
    fake_change_processor_.reset(new syncer::FakeSyncChangeProcessor);
  }

  // Starts sync for |typed_url_sync_service_| with |initial_data| as the
  // initial sync data.
  void StartSyncing(const syncer::SyncDataList& initial_data);

  // Builds a set of url rows and visit vectors based on |num_typed_urls| and
  // |num_reload_urls|, and |urls|. The rows are stored into |rows|, the visit
  // vectors in |visit_vectors|, and the changes are pushed into the history
  // backend.
  // Returns true if sync receives the proper number of changes, false
  // otherwise.
  bool BuildAndPushLocalChanges(unsigned int num_typed_urls,
                                unsigned int num_reload_urls,
                                const std::vector<std::string>& urls,
                                URLRows* rows,
                                std::vector<VisitVector>* visit_vectors);

  // Fills |urls| with the set of synced urls within |typed_url_sync_service_|.
  void GetSyncedUrls(std::set<GURL>* urls) const;

  // Fills |specifics| with the sync data for |url| and |visits|.
  static void WriteToTypedUrlSpecifics(const URLRow& url,
                                       const VisitVector& visits,
                                       sync_pb::TypedUrlSpecifics* specifics);

  // Helper to call TypedUrlSyncableService's MergeURLs method.
  static TypedUrlSyncableService::MergeResult MergeUrls(
      const sync_pb::TypedUrlSpecifics& typed_url,
      const history::URLRow& url,
      history::VisitVector* visits,
      history::URLRow* new_url,
      std::vector<history::VisitInfo>* new_visits);

 protected:
  base::MessageLoop message_loop_;
  scoped_refptr<TestHistoryBackend> fake_history_backend_;
  scoped_ptr<TypedUrlSyncableService> typed_url_sync_service_;
  scoped_ptr<syncer::FakeSyncChangeProcessor> fake_change_processor_;
};

void TypedUrlSyncableServiceTest::StartSyncing(
    const syncer::SyncDataList& initial_data) {
  DCHECK(fake_change_processor_.get());

  // Set change processor.
  syncer::SyncMergeResult result =
      typed_url_sync_service_->MergeDataAndStartSyncing(
          syncer::TYPED_URLS, initial_data,
          scoped_ptr<syncer::SyncChangeProcessor>(
              new syncer::SyncChangeProcessorWrapperForTest(
                  fake_change_processor_.get())),
          scoped_ptr<syncer::SyncErrorFactory>(
              new syncer::SyncErrorFactoryMock()));
  EXPECT_FALSE(result.error().IsSet()) << result.error().message();
}

bool TypedUrlSyncableServiceTest::BuildAndPushLocalChanges(
    unsigned int num_typed_urls,
    unsigned int num_reload_urls,
    const std::vector<std::string>& urls,
    URLRows* rows,
    std::vector<VisitVector>* visit_vectors) {
  unsigned int total_urls = num_typed_urls + num_reload_urls;
  DCHECK(urls.size() >= total_urls);
  if (!typed_url_sync_service_.get())
    return false;

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
      fake_history_backend_->SetVisitsForUrl(rows->back().id(),
                                             &visit_vectors->back());
      changed_urls.push_back(rows->back());
    }

    typed_url_sync_service_->OnUrlsModified(&changed_urls);
  }

  // Check that communication with sync was successful.
  if (num_typed_urls != fake_change_processor_->changes().size())
    return false;
  return true;
}

void TypedUrlSyncableServiceTest::GetSyncedUrls(std::set<GURL>* urls) const {
  return typed_url_sync_service_->GetSyncedUrls(urls);
}

// Static.
void TypedUrlSyncableServiceTest::WriteToTypedUrlSpecifics(
    const URLRow& url,
    const VisitVector& visits,
    sync_pb::TypedUrlSpecifics* specifics) {
  TypedUrlSyncableService::WriteToTypedUrlSpecifics(url, visits, specifics);
}

// Static.
TypedUrlSyncableService::MergeResult TypedUrlSyncableServiceTest::MergeUrls(
    const sync_pb::TypedUrlSpecifics& typed_url,
    const history::URLRow& url,
    history::VisitVector* visits,
    history::URLRow* new_url,
    std::vector<history::VisitInfo>* new_visits) {
  return TypedUrlSyncableService::MergeUrls(typed_url, url, visits, new_url,
                                            new_visits);
}

// Create a local typed URL with one TYPED visit after sync has started. Check
// that sync is sent an ADD change for the new URL.
TEST_F(TypedUrlSyncableServiceTest, AddLocalTypedUrl) {
  // Create a local typed URL (simulate a typed visit) that is not already
  // in sync. Check that sync is sent an ADD change for the existing URL.
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));

  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  // Check change processor.
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  EXPECT_EQ(syncer::TYPED_URLS, changes[0].sync_data().GetDataType());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, changes[0].change_type());

  // Get typed url specifics.
  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();

  EXPECT_TRUE(URLsEqual(url_row, url_specifics));
  ASSERT_EQ(1, url_specifics.visits_size());
  ASSERT_EQ(static_cast<const int>(visits.size()), url_specifics.visits_size());
  EXPECT_EQ(visits[0].visit_time.ToInternalValue(), url_specifics.visits(0));
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(0));

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(1U, sync_state.size());
  EXPECT_TRUE(sync_state.end() != sync_state.find(url_row.url()));
}

// Update a local typed URL that is already synced. Check that sync is sent an
// UPDATE for the existing url, but RELOAD visits aren't synced.
TEST_F(TypedUrlSyncableServiceTest, UpdateLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  changes.clear();

  // Update the URL row, adding another typed visit to the visit vector.
  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  URLRows changed_urls;
  AddNewestVisit(ui::PAGE_TRANSITION_TYPED, 7, &url_row, &visits);
  AddNewestVisit(ui::PAGE_TRANSITION_RELOAD, 8, &url_row, &visits);
  AddNewestVisit(ui::PAGE_TRANSITION_LINK, 9, &url_row, &visits);
  fake_history_backend_->SetVisitsForUrl(url_row.id(), &visits);
  changed_urls.push_back(url_row);

  // Notify typed url sync service of the update.
  typed_url_sync_service_->OnUrlsModified(&changed_urls);

  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  EXPECT_EQ(syncer::TYPED_URLS, changes[0].sync_data().GetDataType());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, changes[0].change_type());

  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();

  EXPECT_TRUE(URLsEqual(url_row, url_specifics));
  ASSERT_EQ(3, url_specifics.visits_size());
  ASSERT_EQ(static_cast<const int>(visits.size()) - 1,
            url_specifics.visits_size());

  // Check that each visit has been translated/communicated correctly.
  // Note that the specifics record visits in chronological order, and the
  // visits from the db are in reverse chronological order.
  EXPECT_EQ(visits[0].visit_time.ToInternalValue(), url_specifics.visits(2));
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(2));
  EXPECT_EQ(visits[2].visit_time.ToInternalValue(), url_specifics.visits(1));
  EXPECT_EQ(static_cast<const int>(visits[2].transition),
            url_specifics.visit_transitions(0));
  EXPECT_EQ(visits[3].visit_time.ToInternalValue(), url_specifics.visits(0));
  EXPECT_EQ(static_cast<const int>(visits[3].transition),
            url_specifics.visit_transitions(0));

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(1U, sync_state.size());
  EXPECT_TRUE(sync_state.end() != sync_state.find(url_row.url()));
}

// Append a RELOAD visit to a typed url that is already synced. Check that sync
// does not receive any updates.
TEST_F(TypedUrlSyncableServiceTest, ReloadVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  changes.clear();

  // Update the URL row, adding another typed visit to the visit vector.
  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  URLRows changed_urls;
  AddNewestVisit(ui::PAGE_TRANSITION_RELOAD, 7, &url_row, &visits);
  fake_history_backend_->SetVisitsForUrl(url_row.id(), &visits);
  changed_urls.push_back(url_row);

  // Notify typed url sync service of the update.
  typed_url_sync_service_->OnUrlVisited(ui::PAGE_TRANSITION_RELOAD, &url_row);

  ASSERT_EQ(0U, changes.size());

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(1U, sync_state.size());
  EXPECT_TRUE(sync_state.end() != sync_state.find(url_row.url()));
}

// Appends a LINK visit to an existing typed url. Check that sync does not
// receive any changes.
TEST_F(TypedUrlSyncableServiceTest, LinkVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  changes.clear();

  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  // Update the URL row, adding a non-typed visit to the visit vector.
  AddNewestVisit(ui::PAGE_TRANSITION_LINK, 6, &url_row, &visits);
  fake_history_backend_->SetVisitsForUrl(url_row.id(), &visits);

  ui::PageTransition transition = ui::PAGE_TRANSITION_LINK;
  // Notify typed url sync service of non-typed visit, expect no change.
  typed_url_sync_service_->OnUrlVisited(transition, &url_row);
  ASSERT_EQ(0u, changes.size());
}

// Appends a series of LINK visits followed by a TYPED one to an existing typed
// url. Check that sync receives an UPDATE with the newest visit data.
TEST_F(TypedUrlSyncableServiceTest, TypedVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  changes.clear();

  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  // Update the URL row, adding another typed visit to the visit vector.
  AddOldestVisit(ui::PAGE_TRANSITION_LINK, 1, &url_row, &visits);
  AddNewestVisit(ui::PAGE_TRANSITION_LINK, 6, &url_row, &visits);
  AddNewestVisit(ui::PAGE_TRANSITION_TYPED, 7, &url_row, &visits);
  fake_history_backend_->SetVisitsForUrl(url_row.id(), &visits);

  // Notify typed url sync service of typed visit.
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  typed_url_sync_service_->OnUrlVisited(transition, &url_row);

  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  EXPECT_EQ(syncer::TYPED_URLS, changes[0].sync_data().GetDataType());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, changes[0].change_type());

  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();

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
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(1U, sync_state.size());
  EXPECT_TRUE(sync_state.end() != sync_state.find(url_row.url()));
}

// Delete several (but not all) local typed urls. Check that sync receives the
// DELETE changes, and the non-deleted urls remain synced.
TEST_F(TypedUrlSyncableServiceTest, DeleteLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back("http://pie.com/");
  urls.push_back("http://cake.com/");
  urls.push_back("http://google.com/");
  urls.push_back("http://foo.com/");
  urls.push_back("http://bar.com/");

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(4, 1, urls, &url_rows, &visit_vectors));
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  changes.clear();

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(4u, sync_state.size());

  // Simulate visit expiry of typed visit, no syncing is done
  // This is to test that sync relies on the in-memory cache to know
  // which urls were typed and synced, and should be deleted.
  url_rows[0].set_typed_count(0);
  VisitVector visits;
  fake_history_backend_->SetVisitsForUrl(url_rows[0].id(), &visits);

  // Delete some urls from backend and create deleted row vector.
  URLRows rows;
  for (size_t i = 0; i < 3u; ++i) {
    fake_history_backend_->DeleteVisitsForUrl(url_rows[i].id());
    rows.push_back(url_rows[i]);
  }

  // Notify typed url sync service.
  typed_url_sync_service_->OnUrlsDeleted(false, false, &rows);

  ASSERT_EQ(3u, changes.size());
  for (size_t i = 0; i < changes.size(); ++i) {
    ASSERT_TRUE(changes[i].IsValid());
    ASSERT_EQ(syncer::TYPED_URLS, changes[i].sync_data().GetDataType());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, changes[i].change_type());
    sync_pb::TypedUrlSpecifics url_specifics =
        changes[i].sync_data().GetSpecifics().typed_url();
    EXPECT_EQ(url_rows[i].url().spec(), url_specifics.url());
  }

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state_deleted;
  GetSyncedUrls(&sync_state_deleted);
  ASSERT_EQ(1u, sync_state_deleted.size());
  EXPECT_TRUE(sync_state_deleted.end() !=
              sync_state_deleted.find(url_rows[3].url()));
}

// Delete all local typed urls. Check that sync receives them all the DELETE
// changes, and that the sync state afterwards is empty.
TEST_F(TypedUrlSyncableServiceTest, DeleteAllLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back("http://pie.com/");
  urls.push_back("http://cake.com/");
  urls.push_back("http://google.com/");
  urls.push_back("http://foo.com/");
  urls.push_back("http://bar.com/");

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(4, 1, urls, &url_rows, &visit_vectors));
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  changes.clear();

  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_EQ(4u, sync_state.size());

  // Delete urls from backend.
  for (size_t i = 0; i < 4u; ++ i) {
    fake_history_backend_->DeleteVisitsForUrl(url_rows[i].id());
  }
  // Delete urls with |all_history| flag set.
  bool all_history = true;

  // Notify typed url sync service.
  typed_url_sync_service_->OnUrlsDeleted(all_history, false, NULL);

  ASSERT_EQ(4u, changes.size());
  for (size_t i = 0; i < changes.size(); ++i) {
    ASSERT_TRUE(changes[i].IsValid());
    ASSERT_EQ(syncer::TYPED_URLS, changes[i].sync_data().GetDataType());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, changes[i].change_type());
  }
  // Check that in-memory representation of sync state is accurate.
  std::set<GURL> sync_state_deleted;
  GetSyncedUrls(&sync_state_deleted);
  EXPECT_TRUE(sync_state_deleted.empty());
}

// Saturate the visits for a typed url with both TYPED and LINK navigations.
// Check that no more than kMaxTypedURLVisits are synced, and that LINK visits
// are dropped rather than TYPED ones.
TEST_F(TypedUrlSyncableServiceTest, MaxVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(0, 1, urls, &url_rows, &visit_vectors));

  URLRow url_row = url_rows.front();
  VisitVector visits;

  // Add |kMaxTypedUrlVisits| + 10 visits to the url. The 10 oldest
  // non-typed visits are expected to be skipped.
  int i = 1;
  for (; i <= kMaxTypedUrlVisits - 20; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_TYPED, i, &url_row, &visits);
  for (; i <= kMaxTypedUrlVisits; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_LINK, i, &url_row, &visits);
  for (; i <= kMaxTypedUrlVisits + 10; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_TYPED, i, &url_row, &visits);

  fake_history_backend_->SetVisitsForUrl(url_row.id(), &visits);

  // Notify typed url sync service of typed visit.
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  typed_url_sync_service_->OnUrlVisited(transition, &url_row);

  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  ASSERT_EQ(syncer::SyncChange::ACTION_ADD, changes[0].change_type());
  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();
  ASSERT_EQ(kMaxTypedUrlVisits, url_specifics.visits_size());

  // Check that each visit has been translated/communicated correctly.
  // Note that the specifics records visits in chronological order, and the
  // visits from the db are in reverse chronological order.
  int num_typed_visits_synced = 0;
  int num_other_visits_synced = 0;
  int r = url_specifics.visits_size() - 1;
  for (int i = 0; i < url_specifics.visits_size(); ++i, --r) {
    if (url_specifics.visit_transitions(i) == ui::PAGE_TRANSITION_TYPED) {
      ++num_typed_visits_synced;
    } else {
      ++num_other_visits_synced;
    }
  }
  EXPECT_EQ(kMaxTypedUrlVisits - 10, num_typed_visits_synced);
  EXPECT_EQ(10, num_other_visits_synced);
}

// Add enough visits to trigger throttling of updates to a typed url. Check that
// sync does not receive an update until the proper throttle interval has been
// reached.
TEST_F(TypedUrlSyncableServiceTest, ThrottleVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(BuildAndPushLocalChanges(0, 1, urls, &url_rows, &visit_vectors));

  URLRow url_row = url_rows.front();
  VisitVector visits;

  // Add enough visits to the url so that typed count is above the throttle
  // limit, and not right on the interval that gets synced.
  int i = 1;
  for (; i < kVisitThrottleThreshold + kVisitThrottleMultiple / 2; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_TYPED, i, &url_row, &visits);
  fake_history_backend_->SetVisitsForUrl(url_row.id(), &visits);

  // Notify typed url sync service of typed visit.
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  typed_url_sync_service_->OnUrlVisited(transition, &url_row);

  // Should throttle, so sync and local cache should not update.
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  ASSERT_EQ(0u, changes.size());
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_TRUE(sync_state.empty());

  for (; i % kVisitThrottleMultiple != 1; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_TYPED, i, &url_row, &visits);
  --i;  // Account for the increment before the condition ends.
  fake_history_backend_->SetVisitsForUrl(url_row.id(), &visits);

  // Notify typed url sync service of typed visit.
  typed_url_sync_service_->OnUrlVisited(transition, &url_row);

  ASSERT_EQ(1u, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  ASSERT_EQ(syncer::SyncChange::ACTION_ADD, changes[0].change_type());
  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();
  ASSERT_EQ(i, url_specifics.visits_size());

  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
}

// Add a typed url locally and one to sync with the same data. Starting sync
// should result in no changes.
TEST_F(TypedUrlSyncableServiceTest, MergeUrlNoChange) {
  // Add a url to backend.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  URLRows url_rows;
  url_rows.push_back(row);
  fake_history_backend_->SetVisitsForUrl(row.id(), &visits);
  fake_history_backend_->SetBackendTypedUrls(&url_rows);

  // Create the same data in sync.
  syncer::SyncDataList initial_sync_data;
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(row, visits, typed_url);
  syncer::SyncData data =
      syncer::SyncData::CreateLocalData(kURL, kTitle, entity_specifics);

  initial_sync_data.push_back(data);
  StartSyncing(initial_sync_data);
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  EXPECT_TRUE(changes.empty());

  // Check that the local cache was is still correct.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(sync_state.count(row.url()), 1U);
}

// Starting sync with no sync data should just push the local url to sync.
TEST_F(TypedUrlSyncableServiceTest, MergeUrlEmptySync) {
  // Add a url to backend.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  URLRows url_rows;
  url_rows.push_back(row);
  fake_history_backend_->SetVisitsForUrl(row.id(), &visits);
  fake_history_backend_->SetBackendTypedUrls(&url_rows);

  StartSyncing(syncer::SyncDataList());

  // Check that the local cache is still correct.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(sync_state.count(row.url()), 1U);

  // Check that the server was updated correctly.
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, changes[0].change_type());
  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();
  ASSERT_EQ(1, url_specifics.visits_size());
  EXPECT_EQ(3, url_specifics.visits(0));
  ASSERT_EQ(1, url_specifics.visit_transitions_size());
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(0));
}

// Starting sync with no local data should just push the synced url into the
// backend.
TEST_F(TypedUrlSyncableServiceTest, MergeUrlEmptyLocal) {
  // Create the sync data.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  syncer::SyncDataList initial_sync_data;
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(row, visits, typed_url);
  syncer::SyncData data =
      syncer::SyncData::CreateLocalData(kURL, kTitle, entity_specifics);

  initial_sync_data.push_back(data);
  StartSyncing(initial_sync_data);
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  EXPECT_TRUE(changes.empty());

  // Check that the local cache is updated correctly.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());
  EXPECT_EQ(sync_state.count(row.url()), 1U);

  // Check that the backend was updated correctly.
  base::Time server_time = base::Time::FromInternalValue(3);
  const std::map<GURL, std::vector<history::VisitInfo>>& added_visits =
      fake_history_backend_->added_visits();
  ASSERT_EQ(1U, added_visits.size());
  ASSERT_NE(added_visits.end(), added_visits.find(GURL(kURL)));
  ASSERT_EQ(1U, added_visits.find(GURL(kURL))->second.size());
  EXPECT_EQ(server_time, added_visits.find(GURL(kURL))->second[0].first);
  EXPECT_EQ(visits[0].transition,
            added_visits.find(GURL(kURL))->second[0].second);
}

// Add a url to the local and sync data before sync begins, with the sync data
// having more recent visits. Check that starting sync updates the backend
// with the sync visit, while the older local visit is not pushed to sync.
// The title should be updated to the sync version due to the more recent
// timestamp.
TEST_F(TypedUrlSyncableServiceTest, MergeUrlOldLocal) {
  const char kTitle2[] = "pie2";
  // Add a url to backend.
  VisitVector visits;
  URLRow local_row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  URLRows url_rows;
  url_rows.push_back(local_row);
  fake_history_backend_->SetVisitsForUrl(local_row.id(), &visits);
  fake_history_backend_->SetBackendTypedUrls(&url_rows);

  // Create sync data for the same url with a more recent visit.
  VisitVector server_visits;
  URLRow server_row =
      MakeTypedUrlRow(kURL, kTitle2, 1, 6, false, &server_visits);
  syncer::SyncDataList initial_sync_data;
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(server_row, server_visits, typed_url);
  syncer::SyncData data =
      syncer::SyncData::CreateLocalData(kURL, kTitle2, entity_specifics);

  initial_sync_data.push_back(data);
  StartSyncing(initial_sync_data);

  // Check that the local cache was updated correctly.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());

  // Check that the backend was updated correctly.
  base::Time server_time = base::Time::FromInternalValue(6);
  const std::map<GURL, std::vector<history::VisitInfo>>& added_visits =
      fake_history_backend_->added_visits();
  ASSERT_EQ(1U, added_visits.size());
  ASSERT_NE(added_visits.end(), added_visits.find(GURL(kURL)));
  ASSERT_EQ(1U, added_visits.find(GURL(kURL))->second.size());
  EXPECT_EQ(server_time, added_visits.find(GURL(kURL))->second[0].first);
  EXPECT_EQ(server_visits[0].transition,
            added_visits.find(GURL(kURL))->second[0].second);
  EXPECT_EQ(kTitle2, base::UTF16ToUTF8(fake_history_backend_->updated_urls()
                                           .find(local_row.id())
                                           ->second.title()));

  // Check that the server was updated correctly.
  // The local history visit should not be added to sync because it is older
  // than sync's oldest visit.
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();
  ASSERT_EQ(1, url_specifics.visits_size());
  EXPECT_EQ(6, url_specifics.visits(0));
  ASSERT_EQ(1, url_specifics.visit_transitions_size());
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(0));
}

// Add a url to the local and sync data before sync begins, with the local data
// having more recent visits. Check that starting sync updates the sync
// with the local visits, while the older sync visit is not pushed to the
// backend. Sync's title should be updated to the local version due to the more
// recent timestamp.
TEST_F(TypedUrlSyncableServiceTest, MergeUrlOldSync) {
  const char kTitle2[] = "pie2";

  // Add a url to backend.
  VisitVector visits;
  URLRow local_row = MakeTypedUrlRow(kURL, kTitle2, 1, 3, false, &visits);
  URLRows url_rows;
  url_rows.push_back(local_row);
  fake_history_backend_->SetVisitsForUrl(local_row.id(), &visits);
  fake_history_backend_->SetBackendTypedUrls(&url_rows);

  // Create sync data for the same url with an older visit.
  VisitVector server_visits;
  URLRow server_row =
      MakeTypedUrlRow(kURL, kTitle, 1, 2, false, &server_visits);
  syncer::SyncDataList initial_sync_data;
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(server_row, server_visits, typed_url);
  syncer::SyncData data =
      syncer::SyncData::CreateLocalData(kURL, kTitle, entity_specifics);

  initial_sync_data.push_back(data);
  StartSyncing(initial_sync_data);

  // Check that the local cache was updated correctly.
  std::set<GURL> sync_state;
  GetSyncedUrls(&sync_state);
  EXPECT_FALSE(sync_state.empty());

  // Check that the backend was not updated.
  const std::map<GURL, std::vector<history::VisitInfo>>& added_visits =
      fake_history_backend_->added_visits();
  ASSERT_EQ(0U, added_visits.size());

  // Check that the server was updated correctly.
  // The local history visit should not be added to sync because it is older
  // than sync's oldest visit.
  syncer::SyncChangeList& changes = fake_change_processor_->changes();
  ASSERT_EQ(1U, changes.size());
  ASSERT_TRUE(changes[0].IsValid());
  sync_pb::TypedUrlSpecifics url_specifics =
      changes[0].sync_data().GetSpecifics().typed_url();
  ASSERT_EQ(1, url_specifics.visits_size());
  EXPECT_EQ(3, url_specifics.visits(0));
  EXPECT_EQ(kTitle2, url_specifics.title());
  ASSERT_EQ(1, url_specifics.visit_transitions_size());
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(0));
}

}  // namespace history
