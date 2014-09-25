// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/typed_urls_helper.h"

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/history/core/browser/history_types.h"

using sync_datatype_helper::test;

namespace {

class FlushHistoryDBQueueTask : public history::HistoryDBTask {
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
  virtual ~FlushHistoryDBQueueTask() {}

  base::WaitableEvent* wait_event_;
};

class GetTypedUrlsTask : public history::HistoryDBTask {
 public:
  GetTypedUrlsTask(history::URLRows* rows, base::WaitableEvent* event)
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
  virtual ~GetTypedUrlsTask() {}

  history::URLRows* rows_;
  base::WaitableEvent* wait_event_;
};

class GetUrlTask : public history::HistoryDBTask {
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
  virtual ~GetUrlTask() {}

  GURL url_;
  history::URLRow* row_;
  base::WaitableEvent* wait_event_;
  bool* found_;
};

class GetVisitsTask : public history::HistoryDBTask {
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
  virtual ~GetVisitsTask() {}

  history::URLID id_;
  history::VisitVector* visits_;
  base::WaitableEvent* wait_event_;
};

class RemoveVisitsTask : public history::HistoryDBTask {
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
  virtual ~RemoveVisitsTask() {}

  const history::VisitVector& visits_;
  base::WaitableEvent* wait_event_;
};

// Waits for the history DB thread to finish executing its current set of
// tasks.
void WaitForHistoryDBThread(int index) {
  base::CancelableTaskTracker tracker;
  HistoryService* service = HistoryServiceFactory::GetForProfileWithoutCreating(
      test()->GetProfile(index));
  base::WaitableEvent wait_event(true, false);
  service->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(
          new FlushHistoryDBQueueTask(&wait_event)),
      &tracker);
  wait_event.Wait();
}

// Creates a URLRow in the specified HistoryService with the passed transition
// type.
void AddToHistory(HistoryService* service,
                  const GURL& url,
                  ui::PageTransition transition,
                  history::VisitSource source,
                  const base::Time& timestamp) {
  service->AddPage(url,
                   timestamp,
                   NULL, // scope
                   1234, // page_id
                   GURL(),  // referrer
                   history::RedirectList(),
                   transition,
                   source,
                   false);
  service->SetPageTitle(url, base::ASCIIToUTF16(url.spec() + " - title"));
}

history::URLRows GetTypedUrlsFromHistoryService(HistoryService* service) {
  base::CancelableTaskTracker tracker;
  history::URLRows rows;
  base::WaitableEvent wait_event(true, false);
  service->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(
          new GetTypedUrlsTask(&rows, &wait_event)),
      &tracker);
  wait_event.Wait();
  return rows;
}

bool GetUrlFromHistoryService(HistoryService* service,
                              const GURL& url, history::URLRow* row) {
  base::CancelableTaskTracker tracker;
  base::WaitableEvent wait_event(true, false);
  bool found = false;
  service->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(
          new GetUrlTask(url, row, &found, &wait_event)),
      &tracker);
  wait_event.Wait();
  return found;
}

history::VisitVector GetVisitsFromHistoryService(HistoryService* service,
                                                 history::URLID id) {
  base::CancelableTaskTracker tracker;
  base::WaitableEvent wait_event(true, false);
  history::VisitVector visits;
  service->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(
          new GetVisitsTask(id, &visits, &wait_event)),
      &tracker);
  wait_event.Wait();
  return visits;
}

void RemoveVisitsFromHistoryService(HistoryService* service,
                                    const history::VisitVector& visits) {
  base::CancelableTaskTracker tracker;
  base::WaitableEvent wait_event(true, false);
  service->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(
          new RemoveVisitsTask(visits, &wait_event)),
      &tracker);
  wait_event.Wait();
}

static base::Time* timestamp = NULL;

}  // namespace

namespace typed_urls_helper {

history::URLRows GetTypedUrlsFromClient(int index) {
  HistoryService* service = HistoryServiceFactory::GetForProfileWithoutCreating(
      test()->GetProfile(index));
  return GetTypedUrlsFromHistoryService(service);
}

bool GetUrlFromClient(int index, const GURL& url, history::URLRow* row) {
  HistoryService* service = HistoryServiceFactory::GetForProfileWithoutCreating(
      test()->GetProfile(index));
  return GetUrlFromHistoryService(service, url, row);
}

history::VisitVector GetVisitsFromClient(int index, history::URLID id) {
  HistoryService* service = HistoryServiceFactory::GetForProfileWithoutCreating(
      test()->GetProfile(index));
  return GetVisitsFromHistoryService(service, id);
}

void RemoveVisitsFromClient(int index, const history::VisitVector& visits) {
  HistoryService* service = HistoryServiceFactory::GetForProfileWithoutCreating(
      test()->GetProfile(index));
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
  AddUrlToHistoryWithTransition(index, url, ui::PAGE_TRANSITION_TYPED,
                                history::SOURCE_BROWSED);
}
void AddUrlToHistoryWithTransition(int index,
                                   const GURL& url,
                                   ui::PageTransition transition,
                                   history::VisitSource source) {
  base::Time timestamp = GetTimestamp();
  AddUrlToHistoryWithTimestamp(index, url, transition, source, timestamp);
}
void AddUrlToHistoryWithTimestamp(int index,
                                  const GURL& url,
                                  ui::PageTransition transition,
                                  history::VisitSource source,
                                  const base::Time& timestamp) {
  AddToHistory(HistoryServiceFactory::GetForProfileWithoutCreating(
                   test()->GetProfile(index)),
               url,
               transition,
               source,
               timestamp);
  if (test()->use_verifier())
    AddToHistory(
        HistoryServiceFactory::GetForProfile(test()->verifier(),
                                             Profile::IMPLICIT_ACCESS),
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
  HistoryServiceFactory::GetForProfileWithoutCreating(
      test()->GetProfile(index))->DeleteURL(url);
  if (test()->use_verifier())
    HistoryServiceFactory::GetForProfile(test()->verifier(),
                                         Profile::IMPLICIT_ACCESS)->
        DeleteURL(url);
  WaitForHistoryDBThread(index);
}

void DeleteUrlsFromHistory(int index, const std::vector<GURL>& urls) {
  HistoryServiceFactory::GetForProfileWithoutCreating(
      test()->GetProfile(index))->DeleteURLsForTest(urls);
  if (test()->use_verifier())
    HistoryServiceFactory::GetForProfile(test()->verifier(),
                                         Profile::IMPLICIT_ACCESS)->
        DeleteURLsForTest(urls);
  WaitForHistoryDBThread(index);
}

bool CheckURLRowVectorsAreEqual(const history::URLRows& left,
                                const history::URLRows& right) {
  if (left.size() != right.size())
    return false;
  for (size_t i = 0; i < left.size(); ++i) {
    // URLs could be out-of-order, so look for a matching URL in the second
    // array.
    bool found = false;
    for (size_t j = 0; j < right.size(); ++j) {
      if (left[i].url() == right[j].url()) {
        if (CheckURLRowsAreEqual(left[i], right[j])) {
          found = true;
          break;
        }
      }
    }
    if (!found)
      return false;
  }
  return true;
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

bool CheckURLRowsAreEqual(
    const history::URLRow& left, const history::URLRow& right) {
  return (left.url() == right.url()) &&
      (left.title() == right.title()) &&
      (left.visit_count() == right.visit_count()) &&
      (left.typed_count() == right.typed_count()) &&
      (left.last_visit() == right.last_visit()) &&
      (left.hidden() == right.hidden());
}

bool CheckAllProfilesHaveSameURLsAsVerifier() {
  HistoryService* verifier_service =
      HistoryServiceFactory::GetForProfile(test()->verifier(),
                                           Profile::IMPLICIT_ACCESS);
  history::URLRows verifier_urls =
      GetTypedUrlsFromHistoryService(verifier_service);
  for (int i = 0; i < test()->num_clients(); ++i) {
    history::URLRows urls = GetTypedUrlsFromClient(i);
    if (!CheckURLRowVectorsAreEqual(verifier_urls, urls))
      return false;
  }
  return true;
}

namespace {

// Helper class used in the implementation of
// AwaitCheckAllProfilesHaveSameURLsAsVerifier.
class ProfilesHaveSameURLsChecker : public MultiClientStatusChangeChecker {
 public:
  ProfilesHaveSameURLsChecker();
  virtual ~ProfilesHaveSameURLsChecker();

  virtual bool IsExitConditionSatisfied() OVERRIDE;
  virtual std::string GetDebugMessage() const OVERRIDE;
};

ProfilesHaveSameURLsChecker::ProfilesHaveSameURLsChecker()
    : MultiClientStatusChangeChecker(
        sync_datatype_helper::test()->GetSyncServices()) {}

ProfilesHaveSameURLsChecker::~ProfilesHaveSameURLsChecker() {}

bool ProfilesHaveSameURLsChecker::IsExitConditionSatisfied() {
  return CheckAllProfilesHaveSameURLsAsVerifier();
}

std::string ProfilesHaveSameURLsChecker::GetDebugMessage() const {
  return "Waiting for matching typed urls profiles";
}

}  //  namespace

bool AwaitCheckAllProfilesHaveSameURLsAsVerifier() {
  ProfilesHaveSameURLsChecker checker;
  checker.Wait();
  return !checker.TimedOut();
}

}  // namespace typed_urls_helper
