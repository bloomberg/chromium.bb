// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/typed_urls_helper.h"

#include "base/compiler_specific.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "content/browser/cancelable_request.h"

using sync_datatype_helper::test;

namespace {

class FlushHistoryDBQueueTask : public HistoryDBTask {
 public:
  explicit FlushHistoryDBQueueTask(base::WaitableEvent* event)
      : wait_event_(event) {}
  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    wait_event_->Signal();
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {}
 private:
  base::WaitableEvent* wait_event_;
};

class GetTypedUrlsTask : public HistoryDBTask {
 public:
  GetTypedUrlsTask(std::vector<history::URLRow>* rows,
                   base::WaitableEvent* event)
      : rows_(rows), wait_event_(event) {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    // Fetch the typed URLs.
    backend->GetAllTypedURLs(rows_);
    wait_event_->Signal();
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {}
 private:
  std::vector<history::URLRow>* rows_;
  base::WaitableEvent* wait_event_;
};

class GetUrlTask : public HistoryDBTask {
 public:
  GetUrlTask(const GURL& url,
             history::URLRow* row,
             bool* found,
             base::WaitableEvent* event)
      : url_(url), row_(row), wait_event_(event), found_(found) {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    // Fetch the typed URLs.
    *found_ = backend->GetURL(url_, row_);
    wait_event_->Signal();
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {}
 private:
  GURL url_;
  history::URLRow* row_;
  base::WaitableEvent* wait_event_;
  bool* found_;
};

class GetVisitsTask : public HistoryDBTask {
 public:
  GetVisitsTask(history::URLID id,
                history::VisitVector* visits,
                base::WaitableEvent* event)
      : id_(id), visits_(visits), wait_event_(event) {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    // Fetch the visits.
    backend->GetVisitsForURL(id_, visits_);
    wait_event_->Signal();
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {}
 private:
  history::URLID id_;
  history::VisitVector* visits_;
  base::WaitableEvent* wait_event_;
};

class RemoveVisitsTask : public HistoryDBTask {
 public:
  RemoveVisitsTask(const history::VisitVector& visits,
                   base::WaitableEvent* event)
      : visits_(visits), wait_event_(event) {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    // Fetch the visits.
    backend->RemoveVisits(visits_);
    wait_event_->Signal();
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {}
 private:
  const history::VisitVector& visits_;
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

// Creates a URLRow in the specified HistoryService with the passed transition
// type.
void AddToHistory(HistoryService* service,
                  const GURL& url,
                  content::PageTransition transition,
                  history::VisitSource source,
                  const base::Time& timestamp) {
  service->AddPage(url,
                   timestamp,
                   NULL, // scope
                   1234, // page_id
                   GURL(),  // referrer
                   transition,
                   history::RedirectList(),
                   source,
                   false);
  service->SetPageTitle(url, ASCIIToUTF16(url.spec() + " - title"));
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

bool GetUrlFromHistoryService(HistoryService* service,
                              const GURL& url, history::URLRow* row) {
  CancelableRequestConsumer cancelable_consumer;
  base::WaitableEvent wait_event(true, false);
  bool found;
  service->ScheduleDBTask(new GetUrlTask(url, row, &found, &wait_event),
                          &cancelable_consumer);
  wait_event.Wait();
  return found;
}

history::VisitVector GetVisitsFromHistoryService(HistoryService* service,
                                                 history::URLID id) {
  CancelableRequestConsumer cancelable_consumer;
  base::WaitableEvent wait_event(true, false);
  history::VisitVector visits;
  service->ScheduleDBTask(new GetVisitsTask(id, &visits, &wait_event),
                          &cancelable_consumer);
  wait_event.Wait();
  return visits;
}

void RemoveVisitsFromHistoryService(HistoryService* service,
                                    const history::VisitVector& visits) {
  CancelableRequestConsumer cancelable_consumer;
  base::WaitableEvent wait_event(true, false);
  service->ScheduleDBTask(new RemoveVisitsTask(visits, &wait_event),
                          &cancelable_consumer);
  wait_event.Wait();
}

static base::Time* timestamp = NULL;

}  // namespace

namespace typed_urls_helper {

std::vector<history::URLRow> GetTypedUrlsFromClient(int index) {
  HistoryService* service =
      test()->GetProfile(index)->GetHistoryServiceWithoutCreating();
  return GetTypedUrlsFromHistoryService(service);
}

bool GetUrlFromClient(int index, const GURL& url, history::URLRow* row) {
  HistoryService* service =
      test()->GetProfile(index)->GetHistoryServiceWithoutCreating();
  return GetUrlFromHistoryService(service, url, row);
}

history::VisitVector GetVisitsFromClient(int index, history::URLID id) {
  HistoryService* service =
      test()->GetProfile(index)->GetHistoryServiceWithoutCreating();
  return GetVisitsFromHistoryService(service, id);
}

void RemoveVisitsFromClient(int index, const history::VisitVector& visits) {
  HistoryService* service =
      test()->GetProfile(index)->GetHistoryServiceWithoutCreating();
  RemoveVisitsFromHistoryService(service, visits);
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
  AddUrlToHistoryWithTransition(index, url, content::PAGE_TRANSITION_TYPED,
                                history::SOURCE_BROWSED);
}
void AddUrlToHistoryWithTransition(int index,
                                   const GURL& url,
                                   content::PageTransition transition,
                                   history::VisitSource source) {
  base::Time timestamp = GetTimestamp();
  AddUrlToHistoryWithTimestamp(index, url, transition, source, timestamp);
}
void AddUrlToHistoryWithTimestamp(int index,
                                  const GURL& url,
                                  content::PageTransition transition,
                                  history::VisitSource source,
                                  const base::Time& timestamp) {
  AddToHistory(test()->GetProfile(index)->GetHistoryServiceWithoutCreating(),
               url,
               transition,
               source,
               timestamp);
  if (test()->use_verifier())
    AddToHistory(
        test()->verifier()->GetHistoryService(Profile::IMPLICIT_ACCESS),
        url,
        transition,
        source,
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

bool AreVisitsEqual(const history::VisitVector& visit1,
                    const history::VisitVector& visit2) {
  if (visit1.size() != visit2.size())
    return false;
  for (size_t i = 0; i < visit1.size(); ++i) {
    if (visit1[i].transition != visit2[i].transition)
      return false;
    if (visit1[i].visit_time != visit2[i].visit_time)
      return false;
  }
  return true;
}

bool AreVisitsUnique(const history::VisitVector& visits) {
  base::Time t = base::Time::FromInternalValue(0);
  for (size_t i = 0; i < visits.size(); ++i) {
    if (t == visits[i].visit_time)
      return false;
    t = visits[i].visit_time;
  }
  return true;
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
