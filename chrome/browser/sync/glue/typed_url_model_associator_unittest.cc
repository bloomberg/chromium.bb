// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "sync/protocol/typed_url_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::TypedUrlModelAssociator;
using content::BrowserThread;

namespace {
class SyncTypedUrlModelAssociatorTest : public testing::Test {
 public:
  static history::URLRow MakeTypedUrlRow(const char* url,
                                         const char* title,
                                         int typed_count,
                                         int64 last_visit,
                                         bool hidden,
                                         history::VisitVector* visits) {
    GURL gurl(url);
    history::URLRow history_url(gurl);
    history_url.set_title(UTF8ToUTF16(title));
    history_url.set_typed_count(typed_count);
    history_url.set_last_visit(
        base::Time::FromInternalValue(last_visit));
    history_url.set_hidden(hidden);
    visits->push_back(history::VisitRow(
        history_url.id(), history_url.last_visit(), 0,
        content::PAGE_TRANSITION_RELOAD, 0));
    history_url.set_visit_count(visits->size());
    return history_url;
  }

  static sync_pb::TypedUrlSpecifics MakeTypedUrlSpecifics(const char* url,
                                                          const char* title,
                                                          int64 last_visit,
                                                          bool hidden) {
    sync_pb::TypedUrlSpecifics typed_url;
    typed_url.set_url(url);
    typed_url.set_title(title);
    typed_url.set_hidden(hidden);
    typed_url.add_visits(last_visit);
    typed_url.add_visit_transitions(content::PAGE_TRANSITION_TYPED);
    return typed_url;
  }

  static bool URLsEqual(history::URLRow& lhs, history::URLRow& rhs) {
    // Only compare synced fields (ignore typed_count and visit_count as those
    // are maintained by the history subsystem).
    return (lhs.url().spec().compare(rhs.url().spec()) == 0) &&
           (lhs.title().compare(rhs.title()) == 0) &&
           (lhs.hidden() == rhs.hidden());
  }
};

class TestTypedUrlModelAssociator : public TypedUrlModelAssociator {
 public:
  TestTypedUrlModelAssociator(base::WaitableEvent* startup,
                              base::WaitableEvent* aborted)
      : TypedUrlModelAssociator(&mock_, NULL, NULL),
        startup_(startup),
        aborted_(aborted) {}
  virtual bool IsAbortPending() OVERRIDE {
    // Let the main thread know that we've been started up, and block until
    // they've called Abort().
    startup_->Signal();
    EXPECT_TRUE(aborted_->TimedWait(TestTimeouts::action_timeout()));
    return TypedUrlModelAssociator::IsAbortPending();
  }
 private:
  ProfileSyncServiceMock mock_;
  base::WaitableEvent* startup_;
  base::WaitableEvent* aborted_;
};

static void CreateModelAssociator(base::WaitableEvent* startup,
                                  base::WaitableEvent* aborted,
                                  base::WaitableEvent* done,
                                  TypedUrlModelAssociator** associator) {
  // Grab the done lock - when we exit, this will be released and allow the
  // test to finish.
  *associator = new TestTypedUrlModelAssociator(startup, aborted);
  // AssociateModels should be aborted and should return false.
  syncer::SyncError error = (*associator)->AssociateModels(NULL, NULL);

  // TODO(lipalani): crbug.com/122690 fix this when fixing abort.
  // EXPECT_TRUE(error.IsSet());
  delete *associator;
  done->Signal();
}

} // namespace

TEST_F(SyncTypedUrlModelAssociatorTest, MergeUrls) {
  history::VisitVector visits1;
  history::URLRow row1(MakeTypedUrlRow("http://pie.com/", "pie",
                                       2, 3, false, &visits1));
  sync_pb::TypedUrlSpecifics specs1(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie",
                                                          3, false));
  history::URLRow new_row1(GURL("http://pie.com/"));
  std::vector<history::VisitInfo> new_visits1;
  EXPECT_TRUE(TypedUrlModelAssociator::MergeUrls(specs1, row1, &visits1,
      &new_row1, &new_visits1) == TypedUrlModelAssociator::DIFF_NONE);

  history::VisitVector visits2;
  history::URLRow row2(MakeTypedUrlRow("http://pie.com/", "pie",
                                       2, 3, false, &visits2));
  sync_pb::TypedUrlSpecifics specs2(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie",
                                                          3, true));
  history::VisitVector expected_visits2;
  history::URLRow expected2(MakeTypedUrlRow("http://pie.com/", "pie",
                                            2, 3, true, &expected_visits2));
  history::URLRow new_row2(GURL("http://pie.com/"));
  std::vector<history::VisitInfo> new_visits2;
  EXPECT_TRUE(TypedUrlModelAssociator::MergeUrls(specs2, row2, &visits2,
                                                 &new_row2, &new_visits2) ==
      TypedUrlModelAssociator::DIFF_LOCAL_ROW_CHANGED);
  EXPECT_TRUE(URLsEqual(new_row2, expected2));

  history::VisitVector visits3;
  history::URLRow row3(MakeTypedUrlRow("http://pie.com/", "pie",
                                       2, 3, false, &visits3));
  sync_pb::TypedUrlSpecifics specs3(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie2",
                                                          3, true));
  history::VisitVector expected_visits3;
  history::URLRow expected3(MakeTypedUrlRow("http://pie.com/", "pie2",
                                            2, 3, true, &expected_visits3));
  history::URLRow new_row3(GURL("http://pie.com/"));
  std::vector<history::VisitInfo> new_visits3;
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_LOCAL_ROW_CHANGED |
            TypedUrlModelAssociator::DIFF_NONE,
            TypedUrlModelAssociator::MergeUrls(specs3, row3, &visits3,
                                               &new_row3, &new_visits3));
  EXPECT_TRUE(URLsEqual(new_row3, expected3));

  // Create one node in history DB with timestamp of 3, and one node in sync
  // DB with timestamp of 4. Result should contain one new item (4).
  history::VisitVector visits4;
  history::URLRow row4(MakeTypedUrlRow("http://pie.com/", "pie",
                                       2, 3, false, &visits4));
  sync_pb::TypedUrlSpecifics specs4(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie2",
                                                          4, false));
  history::VisitVector expected_visits4;
  history::URLRow expected4(MakeTypedUrlRow("http://pie.com/", "pie2",
                                            2, 4, false, &expected_visits4));
  history::URLRow new_row4(GURL("http://pie.com/"));
  std::vector<history::VisitInfo> new_visits4;
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_UPDATE_NODE |
            TypedUrlModelAssociator::DIFF_LOCAL_ROW_CHANGED |
            TypedUrlModelAssociator::DIFF_LOCAL_VISITS_ADDED,
            TypedUrlModelAssociator::MergeUrls(specs4, row4, &visits4,
                                               &new_row4, &new_visits4));
  EXPECT_EQ(1U, new_visits4.size());
  EXPECT_EQ(specs4.visits(0), new_visits4[0].first.ToInternalValue());
  EXPECT_TRUE(URLsEqual(new_row4, expected4));
  EXPECT_EQ(2U, visits4.size());

  history::VisitVector visits5;
  history::URLRow row5(MakeTypedUrlRow("http://pie.com/", "pie",
                                       1, 4, false, &visits5));
  sync_pb::TypedUrlSpecifics specs5(MakeTypedUrlSpecifics("http://pie.com/",
                                                          "pie",
                                                          3, false));
  history::VisitVector expected_visits5;
  history::URLRow expected5(MakeTypedUrlRow("http://pie.com/", "pie",
                                            2, 3, false, &expected_visits5));
  history::URLRow new_row5(GURL("http://pie.com/"));
  std::vector<history::VisitInfo> new_visits5;

  // UPDATE_NODE should be set because row5 has a newer last_visit timestamp.
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_UPDATE_NODE |
            TypedUrlModelAssociator::DIFF_NONE,
            TypedUrlModelAssociator::MergeUrls(specs5, row5, &visits5,
                                               &new_row5, &new_visits5));
  EXPECT_TRUE(URLsEqual(new_row5, expected5));
  EXPECT_EQ(0U, new_visits5.size());
}

TEST_F(SyncTypedUrlModelAssociatorTest, MergeUrlsAfterExpiration) {
  // Tests to ensure that we don't resurrect expired URLs (URLs that have been
  // deleted from the history DB but still exist in the sync DB).

  // First, create a history row that has two visits, with timestamps 2 and 3.
  history::VisitVector(history_visits);
  history_visits.push_back(history::VisitRow(
      0, base::Time::FromInternalValue(2), 0, content::PAGE_TRANSITION_TYPED,
      0));
  history::URLRow history_url(MakeTypedUrlRow("http://pie.com/", "pie",
                                              2, 3, false, &history_visits));

  // Now, create a sync node with visits at timestamps 1, 2, 3, 4.
  sync_pb::TypedUrlSpecifics node(MakeTypedUrlSpecifics("http://pie.com/",
                                                        "pie", 1, false));
  node.add_visits(2);
  node.add_visits(3);
  node.add_visits(4);
  node.add_visit_transitions(2);
  node.add_visit_transitions(3);
  node.add_visit_transitions(4);
  history::URLRow new_history_url(history_url.url());
  std::vector<history::VisitInfo> new_visits;
  EXPECT_EQ(TypedUrlModelAssociator::DIFF_NONE |
            TypedUrlModelAssociator::DIFF_LOCAL_VISITS_ADDED,
            TypedUrlModelAssociator::MergeUrls(
                node, history_url, &history_visits, &new_history_url,
                &new_visits));
  EXPECT_TRUE(URLsEqual(history_url, new_history_url));
  EXPECT_EQ(1U, new_visits.size());
  EXPECT_EQ(4U, new_visits[0].first.ToInternalValue());
  // We should not sync the visit with timestamp #1 since it is earlier than
  // any other visit for this URL in the history DB. But we should sync visit
  // #4.
  EXPECT_EQ(3U, history_visits.size());
  EXPECT_EQ(2U, history_visits[0].visit_time.ToInternalValue());
  EXPECT_EQ(3U, history_visits[1].visit_time.ToInternalValue());
  EXPECT_EQ(4U, history_visits[2].visit_time.ToInternalValue());
}

TEST_F(SyncTypedUrlModelAssociatorTest, DiffVisitsSame) {
  history::VisitVector old_visits;
  sync_pb::TypedUrlSpecifics new_url;

  const int64 visits[] = { 1024, 2065, 65534, 1237684 };

  for (size_t c = 0; c < arraysize(visits); ++c) {
    old_visits.push_back(history::VisitRow(
        0, base::Time::FromInternalValue(visits[c]), 0,
        content::PAGE_TRANSITION_TYPED, 0));
    new_url.add_visits(visits[c]);
    new_url.add_visit_transitions(content::PAGE_TRANSITION_TYPED);
  }

  std::vector<history::VisitInfo> new_visits;
  history::VisitVector removed_visits;

  TypedUrlModelAssociator::DiffVisits(old_visits, new_url,
                                      &new_visits, &removed_visits);
  EXPECT_TRUE(new_visits.empty());
  EXPECT_TRUE(removed_visits.empty());
}

TEST_F(SyncTypedUrlModelAssociatorTest, DiffVisitsRemove) {
  history::VisitVector old_visits;
  sync_pb::TypedUrlSpecifics new_url;

  const int64 visits_left[] = { 1, 2, 1024, 1500, 2065, 6000,
                                65534, 1237684, 2237684 };
  const int64 visits_right[] = { 1024, 2065, 65534, 1237684 };

  // DiffVisits will not remove the first visit, because we never delete visits
  // from the start of the array (since those visits can get truncated by the
  // size-limiting code).
  const int64 visits_removed[] = { 1500, 6000, 2237684 };

  for (size_t c = 0; c < arraysize(visits_left); ++c) {
    old_visits.push_back(history::VisitRow(
        0, base::Time::FromInternalValue(visits_left[c]), 0,
        content::PAGE_TRANSITION_TYPED, 0));
  }

  for (size_t c = 0; c < arraysize(visits_right); ++c) {
    new_url.add_visits(visits_right[c]);
    new_url.add_visit_transitions(content::PAGE_TRANSITION_TYPED);
  }

  std::vector<history::VisitInfo> new_visits;
  history::VisitVector removed_visits;

  TypedUrlModelAssociator::DiffVisits(old_visits, new_url,
                                      &new_visits, &removed_visits);
  EXPECT_TRUE(new_visits.empty());
  ASSERT_EQ(removed_visits.size(), arraysize(visits_removed));
  for (size_t c = 0; c < arraysize(visits_removed); ++c) {
    EXPECT_EQ(removed_visits[c].visit_time.ToInternalValue(),
              visits_removed[c]);
  }
}

TEST_F(SyncTypedUrlModelAssociatorTest, DiffVisitsAdd) {
  history::VisitVector old_visits;
  sync_pb::TypedUrlSpecifics new_url;

  const int64 visits_left[] = { 1024, 2065, 65534, 1237684 };
  const int64 visits_right[] = { 1, 1024, 1500, 2065, 6000,
                                65534, 1237684, 2237684 };

  const int64 visits_added[] = { 1, 1500, 6000, 2237684 };

  for (size_t c = 0; c < arraysize(visits_left); ++c) {
    old_visits.push_back(history::VisitRow(
        0, base::Time::FromInternalValue(visits_left[c]), 0,
        content::PAGE_TRANSITION_TYPED, 0));
  }

  for (size_t c = 0; c < arraysize(visits_right); ++c) {
    new_url.add_visits(visits_right[c]);
    new_url.add_visit_transitions(content::PAGE_TRANSITION_TYPED);
  }

  std::vector<history::VisitInfo> new_visits;
  history::VisitVector removed_visits;

  TypedUrlModelAssociator::DiffVisits(old_visits, new_url,
                                      &new_visits, &removed_visits);
  EXPECT_TRUE(removed_visits.empty());
  ASSERT_TRUE(new_visits.size() == arraysize(visits_added));
  for (size_t c = 0; c < arraysize(visits_added); ++c) {
    EXPECT_EQ(new_visits[c].first.ToInternalValue(),
              visits_added[c]);
    EXPECT_EQ(new_visits[c].second, content::PAGE_TRANSITION_TYPED);
  }
}

static history::VisitRow CreateVisit(content::PageTransition type,
                                     int64 timestamp) {
  return history::VisitRow(0, base::Time::FromInternalValue(timestamp), 0,
                           type, 0);
}

TEST_F(SyncTypedUrlModelAssociatorTest, WriteTypedUrlSpecifics) {
  history::VisitVector visits;
  visits.push_back(CreateVisit(content::PAGE_TRANSITION_TYPED, 1));
  visits.push_back(CreateVisit(content::PAGE_TRANSITION_RELOAD, 2));
  visits.push_back(CreateVisit(content::PAGE_TRANSITION_LINK, 3));

  history::URLRow url(MakeTypedUrlRow("http://pie.com/", "pie",
                                      1, 100, false, &visits));
  sync_pb::TypedUrlSpecifics typed_url;
  TypedUrlModelAssociator::WriteToTypedUrlSpecifics(url, visits, &typed_url);
  // RELOAD visits should be removed.
  EXPECT_EQ(2, typed_url.visits_size());
  EXPECT_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  EXPECT_EQ(1, typed_url.visits(0));
  EXPECT_EQ(3, typed_url.visits(1));
  EXPECT_EQ(content::PAGE_TRANSITION_TYPED,
      static_cast<content::PageTransition>(typed_url.visit_transitions(0)));
  EXPECT_EQ(content::PAGE_TRANSITION_LINK,
      static_cast<content::PageTransition>(typed_url.visit_transitions(1)));
}

TEST_F(SyncTypedUrlModelAssociatorTest, TooManyVisits) {
  history::VisitVector visits;
  int64 timestamp = 1000;
  visits.push_back(CreateVisit(content::PAGE_TRANSITION_TYPED, timestamp++));
  for (int i = 0 ; i < 100; ++i) {
    visits.push_back(CreateVisit(content::PAGE_TRANSITION_LINK, timestamp++));
  }
  history::URLRow url(MakeTypedUrlRow("http://pie.com/", "pie",
                                      1, timestamp++, false, &visits));
  sync_pb::TypedUrlSpecifics typed_url;
  TypedUrlModelAssociator::WriteToTypedUrlSpecifics(url, visits, &typed_url);
  // # visits should be capped at 100.
  EXPECT_EQ(100, typed_url.visits_size());
  EXPECT_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  EXPECT_EQ(1000, typed_url.visits(0));
  // Visit with timestamp of 1001 should be omitted since we should have
  // skipped that visit to stay under the cap.
  EXPECT_EQ(1002, typed_url.visits(1));
  EXPECT_EQ(content::PAGE_TRANSITION_TYPED,
      static_cast<content::PageTransition>(typed_url.visit_transitions(0)));
  EXPECT_EQ(content::PAGE_TRANSITION_LINK,
      static_cast<content::PageTransition>(typed_url.visit_transitions(1)));
}

TEST_F(SyncTypedUrlModelAssociatorTest, TooManyTypedVisits) {
  history::VisitVector visits;
  int64 timestamp = 1000;
  for (int i = 0 ; i < 102; ++i) {
    visits.push_back(CreateVisit(content::PAGE_TRANSITION_TYPED, timestamp++));
    visits.push_back(CreateVisit(content::PAGE_TRANSITION_LINK, timestamp++));
    visits.push_back(CreateVisit(content::PAGE_TRANSITION_RELOAD, timestamp++));
  }
  history::URLRow url(MakeTypedUrlRow("http://pie.com/", "pie",
                                      1, timestamp++, false, &visits));
  sync_pb::TypedUrlSpecifics typed_url;
  TypedUrlModelAssociator::WriteToTypedUrlSpecifics(url, visits, &typed_url);
  // # visits should be capped at 100.
  EXPECT_EQ(100, typed_url.visits_size());
  EXPECT_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  // First two typed visits should be skipped.
  EXPECT_EQ(1006, typed_url.visits(0));

  // Ensure there are no non-typed visits since that's all that should fit.
  for (int i = 0; i < typed_url.visits_size(); ++i) {
    EXPECT_EQ(content::PAGE_TRANSITION_TYPED,
        static_cast<content::PageTransition>(typed_url.visit_transitions(i)));
  }
}

TEST_F(SyncTypedUrlModelAssociatorTest, NoTypedVisits) {
  history::VisitVector visits;
  history::URLRow url(MakeTypedUrlRow("http://pie.com/", "pie",
                                      1, 1000, false, &visits));
  sync_pb::TypedUrlSpecifics typed_url;
  TypedUrlModelAssociator::WriteToTypedUrlSpecifics(url, visits, &typed_url);
  // URLs with no typed URL visits should be translated to a URL with one
  // reload visit.
  EXPECT_EQ(1, typed_url.visits_size());
  EXPECT_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  // First two typed visits should be skipped.
  EXPECT_EQ(1000, typed_url.visits(0));
  EXPECT_EQ(content::PAGE_TRANSITION_RELOAD,
      static_cast<content::PageTransition>(typed_url.visit_transitions(0)));
}

// This test verifies that we can abort model association from the UI thread.
// We start up the model associator on the DB thread, block until we abort the
// association on the UI thread, then ensure that AssociateModels() returns
// false.
TEST_F(SyncTypedUrlModelAssociatorTest, TestAbort) {
  content::TestBrowserThread db_thread(BrowserThread::DB);
  base::WaitableEvent startup(false, false);
  base::WaitableEvent aborted(false, false);
  base::WaitableEvent done(false, false);
  TypedUrlModelAssociator* associator;
  // Fire off to the DB thread to create the model associator and start
  // model association.
  db_thread.Start();
  base::Closure callback = base::Bind(
      &CreateModelAssociator, &startup, &aborted, &done, &associator);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE, callback);
  // Wait for the model associator to get created and start assocation.
  ASSERT_TRUE(startup.TimedWait(TestTimeouts::action_timeout()));
  // Abort the model assocation - this should be callable from any thread.
  associator->AbortAssociation();
  // Tell the remote thread to continue.
  aborted.Signal();
  // Block until CreateModelAssociator() exits.
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));
}
