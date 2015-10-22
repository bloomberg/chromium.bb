// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/data_provider.h"
#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/android/history_report/delta_file_commons.h"
#include "chrome/browser/android/history_report/delta_file_service.h"
#include "chrome/browser/android/history_report/get_all_urls_from_history_task.h"
#include "chrome/browser/android/history_report/historic_visits_migration_task.h"
#include "chrome/browser/android/history_report/usage_reports_buffer_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"

using bookmarks::BookmarkModel;

namespace {
static bool g_is_debug = false;

typedef base::hash_map<std::string, BookmarkModel::URLAndTitle*> BookmarkMap;

struct Context {
  history::HistoryService* history_service;
  base::CancelableTaskTracker* history_task_tracker;
  base::WaitableEvent finished;

  Context(history::HistoryService* hservice,
          base::CancelableTaskTracker* tracker)
      : history_service(hservice),
        history_task_tracker(tracker),
        finished(false, false) {}
};

void UpdateUrl(Context* context,
               size_t position,
               std::vector<history_report::DeltaFileEntryWithData>* urls,
               bool success,
               const history::URLRow& url,
               const history::VisitVector& visits) {
  history_report::DeltaFileEntryWithData* entry = &((*urls)[position]);
  if (success) {
    entry->SetData(url);
  } else if (g_is_debug){
    LOG(WARNING) << "DB not initialized or no data for url " << entry->Url();
  }
  if (position + 1 == urls->size()) {
    context->finished.Signal();
  } else {
    context->history_service->QueryURL(GURL((*urls)[position + 1].Url()),
                                       false,
                                       base::Bind(&UpdateUrl,
                                                  base::Unretained(context),
                                                  position + 1,
                                                  base::Unretained(urls)),
                                       context->history_task_tracker);
  }
}

void QueryUrlsHistoryInUiThread(
    Context* context,
    std::vector<history_report::DeltaFileEntryWithData>* urls) {
  context->history_task_tracker->TryCancelAll();
  // TODO(haaawk): change history service so that all this data can be
  //               obtained with a single call to history service.
  context->history_service->QueryURL(GURL((*urls)[0].Url()),
                                     false,
                                     base::Bind(&UpdateUrl,
                                                base::Unretained(context),
                                                0,
                                                base::Unretained(urls)),
                                     context->history_task_tracker);
}

void StartVisitMigrationToUsageBufferUiThread(
    history::HistoryService* history_service,
    history_report::UsageReportsBufferService* buffer_service,
    base::WaitableEvent* finished,
    base::CancelableTaskTracker* task_tracker) {
  history_service->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(
          new history_report::HistoricVisitsMigrationTask(finished,
                                                          buffer_service)),
      task_tracker);
}

}  // namespace

namespace history_report {

DataProvider::DataProvider(Profile* profile,
                           DeltaFileService* delta_file_service,
                           BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model),
      delta_file_service_(delta_file_service) {
  history_service_ = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
}

DataProvider::~DataProvider() {}

scoped_ptr<std::vector<DeltaFileEntryWithData> > DataProvider::Query(
    int64 last_seq_no,
    int32 limit) {
  if (last_seq_no == 0)
    RecreateLog();
  scoped_ptr<std::vector<DeltaFileEntryWithData> > entries;
  scoped_ptr<std::vector<DeltaFileEntryWithData> > valid_entries;
  do {
    entries = delta_file_service_->Query(last_seq_no, limit);
    if (!entries->empty()) {
      Context context(history_service_,
                      &history_task_tracker_);
      content::BrowserThread::PostTask(
          content::BrowserThread::UI,
          FROM_HERE,
          base::Bind(&QueryUrlsHistoryInUiThread,
                     base::Unretained(&context),
                     base::Unretained(entries.get())));
      std::vector<BookmarkModel::URLAndTitle> bookmarks;
      bookmark_model_->BlockTillLoaded();
      bookmark_model_->GetBookmarks(&bookmarks);
      BookmarkMap bookmark_map;
      for (size_t i = 0; i < bookmarks.size(); ++i) {
        bookmark_map.insert(
            make_pair(bookmarks[i].url.spec(), &bookmarks[i]));
      }
      context.finished.Wait();
      for (size_t i = 0; i < entries->size(); ++i) {
        BookmarkMap::iterator bookmark =
            bookmark_map.find((*entries)[i].Url());
        if (bookmark != bookmark_map.end())
          (*entries)[i].MarkAsBookmark(*(bookmark->second));
      }
    }

    valid_entries.reset(new std::vector<DeltaFileEntryWithData>());
    valid_entries->reserve(entries->size());
    for (size_t i = 0; i < entries->size(); ++i) {
      const DeltaFileEntryWithData& entry = (*entries)[i];
      if (entry.Valid()) valid_entries->push_back(entry);
      if (entry.SeqNo() > last_seq_no) last_seq_no = entry.SeqNo();
    }
  } while (!entries->empty() && valid_entries->empty());
  return valid_entries.Pass();
}

void DataProvider::StartVisitMigrationToUsageBuffer(
    UsageReportsBufferService* buffer_service) {
  base::WaitableEvent finished(false, false);
  buffer_service->Clear();
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&StartVisitMigrationToUsageBufferUiThread,
                 base::Unretained(history_service_),
                 buffer_service,
                 base::Unretained(&finished),
                 base::Unretained(&history_task_tracker_)));
  finished.Wait();
}

void DataProvider::RecreateLog() {
  std::vector<std::string> urls;
  {
    base::WaitableEvent finished(false, false);

    scoped_ptr<history::HistoryDBTask> task =
        scoped_ptr<history::HistoryDBTask>(new GetAllUrlsFromHistoryTask(
          &finished, &urls));
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(base::IgnoreResult(&history::HistoryService::ScheduleDBTask),
                   base::Unretained(history_service_), base::Passed(&task),
                   base::Unretained(&history_task_tracker_)));
    finished.Wait();
  }

  std::vector<BookmarkModel::URLAndTitle> bookmarks;
  bookmark_model_->BlockTillLoaded();
  bookmark_model_->GetBookmarks(&bookmarks);
  urls.reserve(urls.size() + bookmarks.size());
  for (size_t i = 0; i < bookmarks.size(); i++) {
    urls.push_back(bookmarks[i].url.spec());
  }
  delta_file_service_->Recreate(urls);
}

}  // namespace history_report
