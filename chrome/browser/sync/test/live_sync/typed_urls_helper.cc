// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/live_sync/typed_urls_helper.h"

#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/live_sync/live_sync_test.h"
#include "chrome/browser/sync/test/live_sync/sync_datatype_helper.h"
#include "content/browser/cancelable_request.h"

using sync_datatype_helper::test;

namespace {

class FlushHistoryDBQueueTask : public HistoryDBTask {
 public:
  explicit FlushHistoryDBQueueTask(base::WaitableEvent* event)
      : wait_event_(event) {}
  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) {
    wait_event_->Signal();
    return true;
  }

  virtual void DoneRunOnMainThread() {}
 private:
  base::WaitableEvent* wait_event_;
};

class GetTypedUrlsTask : public HistoryDBTask {
 public:
  GetTypedUrlsTask(std::vector<history::URLRow>* rows,
                   base::WaitableEvent* event)
      : rows_(rows), wait_event_(event) {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) {
    // Fetch the typed URLs.
    backend->GetAllTypedURLs(rows_);
    wait_event_->Signal();
    return true;
  }

  virtual void DoneRunOnMainThread() {}
 private:
  std::vector<history::URLRow>* rows_;
  base::WaitableEvent* wait_event_;
};

// Waits for the history DB thread to finish executing its current set of
// tasks.
void WaitForHistoryDBThread(int index) {
  CancelableRequestConsumer cancelable_consumer;
  HistoryService* service =
      test()->GetProfile(index)->GetHistoryServiceWithoutCreating();
  base::WaitableEvent wait_event(true, false);
  service->ScheduleDBTask(new FlushHistoryDBQueueTask(&wait_event),
                          &cancelable_consumer);
  wait_event.Wait();
}

// Creates a URLRow in the specified HistoryService.
void AddToHistory(HistoryService* service,
                  const GURL& url,
                  const base::Time& timestamp) {
  service->AddPage(url,
                   timestamp,
                   NULL, // scope
                   1234, // page_id
                   GURL(),  // referrer
                   PageTransition::TYPED,
                   history::RedirectList(),
                   history::SOURCE_BROWSED,
                   false);
}

std::vector<history::URLRow> GetTypedUrlsFromHistoryService(
    HistoryService* service) {
  CancelableRequestConsumer cancelable_consumer;
  std::vector<history::URLRow> rows;
  base::WaitableEvent wait_event(true, false);
  service->ScheduleDBTask(new GetTypedUrlsTask(&rows, &wait_event),
                          &cancelable_consumer);
  wait_event.Wait();
  return rows;
}

static base::Time* timestamp = NULL;

}  // namespace

namespace typed_urls_helper {

std::vector<history::URLRow> GetTypedUrlsFromClient(int index) {
  HistoryService* service =
      test()->GetProfile(index)->GetHistoryServiceWithoutCreating();
  return GetTypedUrlsFromHistoryService(service);
}

base::Time GetTimestamp() {
  // The history subsystem doesn't like identical timestamps for page visits,
  // and it will massage the visit timestamps if we try to use identical
  // values, which can lead to spurious errors. So make sure all timestamps
  // are unique.
  if (!::timestamp)
    ::timestamp = new base::Time(base::Time::Now());
  base::Time original = *::timestamp;
  *::timestamp += base::TimeDelta::FromMilliseconds(1);
  return original;
}

void AddUrlToHistory(int index, const GURL& url) {
  base::Time timestamp = GetTimestamp();
  AddToHistory(test()->GetProfile(index)->GetHistoryServiceWithoutCreating(),
               url,
               timestamp);
  if (test()->use_verifier())
    AddToHistory(
        test()->verifier()->GetHistoryService(Profile::IMPLICIT_ACCESS),
        url,
        timestamp);

  // Wait until the AddPage() request has completed so we know the change has
  // filtered down to the sync observers (don't need to wait for the
  // verifier profile since it doesn't sync).
  WaitForHistoryDBThread(index);
}

void DeleteUrlFromHistory(int index, const GURL& url) {
  test()->GetProfile(index)->GetHistoryServiceWithoutCreating()->DeleteURL(url);
  if (test()->use_verifier())
    test()->verifier()->GetHistoryService(Profile::IMPLICIT_ACCESS)->
        DeleteURL(url);
  WaitForHistoryDBThread(index);
}

void AssertURLRowVectorsAreEqual(
    const std::vector<history::URLRow>& left,
    const std::vector<history::URLRow>& right) {
  ASSERT_EQ(left.size(), right.size());
  for (size_t i = 0; i < left.size(); ++i) {
    // URLs could be out-of-order, so look for a matching URL in the second
    // array.
    bool found = false;
    for (size_t j = 0; j < right.size(); ++j) {
      if (left[i].url() == right[j].url()) {
        AssertURLRowsAreEqual(left[i], right[j]);
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
  }
}

void AssertURLRowsAreEqual(
    const history::URLRow& left, const history::URLRow& right) {
  ASSERT_EQ(left.url(), right.url());
  ASSERT_EQ(left.title(), right.title());
  ASSERT_EQ(left.visit_count(), right.visit_count());
  ASSERT_EQ(left.typed_count(), right.typed_count());
  ASSERT_EQ(left.last_visit(), right.last_visit());
  ASSERT_EQ(left.hidden(), right.hidden());
}

void AssertAllProfilesHaveSameURLsAsVerifier() {
  HistoryService* verifier_service =
      test()->verifier()->GetHistoryService(Profile::IMPLICIT_ACCESS);
  std::vector<history::URLRow> verifier_urls =
      GetTypedUrlsFromHistoryService(verifier_service);
  for (int i = 0; i < test()->num_clients(); ++i) {
    std::vector<history::URLRow> urls = GetTypedUrlsFromClient(i);
    AssertURLRowVectorsAreEqual(verifier_urls, urls);
  }
}

}  // namespace typed_urls_helper
