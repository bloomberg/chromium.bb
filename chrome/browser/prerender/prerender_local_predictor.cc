// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_local_predictor.h"

#include "base/timer.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_database.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace prerender {

namespace {

// Task to lookup the URL for a given URLID.
class GetURLForURLIDTask : public HistoryDBTask {
 public:
  GetURLForURLIDTask(PrerenderLocalPredictor* local_predictor,
                     history::URLID url_id)
      : local_predictor_(local_predictor),
        url_id_(url_id),
        success_(false) {
  }

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    history::URLRow url_row;
    success_ = db->GetURLRow(url_id_, &url_row);
    if (success_)
      url_ = url_row.url();
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {
    if (success_)
      local_predictor_->OnLookupURL(url_id_, url_);
  }

 private:
  virtual ~GetURLForURLIDTask() {}

  PrerenderLocalPredictor* local_predictor_;
  history::URLID url_id_;
  bool success_;
  GURL url_;
  DISALLOW_COPY_AND_ASSIGN(GetURLForURLIDTask);
};

// Task to load history from the visit database on startup.
class GetVisitHistoryTask : public HistoryDBTask {
 public:
  GetVisitHistoryTask(PrerenderLocalPredictor* local_predictor,
                      int max_visits)
      : local_predictor_(local_predictor),
        max_visits_(max_visits),
        visit_history_(new std::vector<history::BriefVisitInfo>) {
  }

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    db->GetBriefVisitInfoOfMostRecentVisits(max_visits_, visit_history_.get());
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {
    local_predictor_->OnGetInitialVisitHistory(visit_history_.Pass());
  }

 private:
  virtual ~GetVisitHistoryTask() {}

  PrerenderLocalPredictor* local_predictor_;
  int max_visits_;
  scoped_ptr<std::vector<history::BriefVisitInfo> > visit_history_;
  DISALLOW_COPY_AND_ASSIGN(GetVisitHistoryTask);
};

// Maximum visit history to retrieve from the visit database.
const int kMaxVisitHistory = 100 * 1000;

}  // namespace

PrerenderLocalPredictor::PrerenderLocalPredictor(
    PrerenderManager* prerender_manager)
    : prerender_manager_(prerender_manager),
      visit_history_initialized_(false) {
  if (MessageLoop::current()) {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(kInitDelayMs),
                 this,
                 &PrerenderLocalPredictor::Init);
  }
}

PrerenderLocalPredictor::~PrerenderLocalPredictor() {
  if (observing_history_service_.get())
    observing_history_service_->RemoveVisitDatabaseObserver(this);
}

void PrerenderLocalPredictor::OnAddVisit(const history::BriefVisitInfo& info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  HistoryService* history = GetHistoryIfExists();
  if (history) {
    history->ScheduleDBTask(
        new GetURLForURLIDTask(this, info.url_id),
        &history_db_consumer_);
  }
}

void PrerenderLocalPredictor::OnLookupURL(history::URLID url_id,
                                          const GURL& url) {
}

void PrerenderLocalPredictor::OnGetInitialVisitHistory(
    scoped_ptr<std::vector<history::BriefVisitInfo> > visit_history) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!visit_history_initialized_);
  visit_history_.reset(visit_history.release());
  visit_history_initialized_ = true;
}

HistoryService* PrerenderLocalPredictor::GetHistoryIfExists() const {
  Profile* profile = prerender_manager_->profile();
  if (!profile)
    return NULL;
  return profile->GetHistoryServiceWithoutCreating();
}

void PrerenderLocalPredictor::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  HistoryService* history = GetHistoryIfExists();
  if (!history) {
    // TODO(tburkard): Record this somewhere (eg histogram) and/or try again
    // later.
    return;
  }
  history->ScheduleDBTask(
      new GetVisitHistoryTask(this, kMaxVisitHistory),
      &history_db_consumer_);
  observing_history_service_ = history;
  observing_history_service_->AddVisitDatabaseObserver(this);
}

}  // namespace prerender
