// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_local_predictor.h"

#include <algorithm>
#include <map>
#include <set>

#include "base/timer.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/page_transition_types.h"

using content::BrowserThread;
using content::PageTransition;
using history::URLID;

namespace prerender {

namespace {

// Task to lookup the URL for a given URLID.
class GetURLForURLIDTask : public HistoryDBTask {
 public:
  GetURLForURLIDTask(PrerenderLocalPredictor* local_predictor,
                     URLID url_id)
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
  URLID url_id_;
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

// Visit history size at which to trigger pruning, and number of items to prune.
const int kVisitHistoryPruneThreshold = 120 * 1000;
const int kVisitHistoryPruneAmount = 20 * 1000;

const int kMaxLocalPredictionTimeMs = 300 * 1000;
const int kMinLocalPredictionTimeMs = 500;

bool IsBackForward(PageTransition transition) {
  return (transition & content::PAGE_TRANSITION_FORWARD_BACK) != 0;
}

bool IsHomePage(PageTransition transition) {
  return (transition & content::PAGE_TRANSITION_HOME_PAGE) != 0;
}

bool IsIntermediateRedirect(PageTransition transition) {
  return (transition & content::PAGE_TRANSITION_CHAIN_END) == 0;
}

bool ShouldExcludeTransitionForPrediction(PageTransition transition) {
  return IsBackForward(transition) || IsHomePage(transition) ||
      IsIntermediateRedirect(transition);
}

base::Time GetCurrentTime() {
  return base::Time::Now();
}

bool StrCaseStr(std::string haystack, std::string needle) {
  std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
  std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
  return (haystack.find(needle)!=std::string::npos);
}

}  // namespace

struct PrerenderLocalPredictor::PrerenderData {
  // For expiration purposes, this is a synthetic start time consisting either
  // of the actual start time, or of the last time the page was re-requested
  // for prerendering - 10 seconds (unless the original request came after
  // that).  This is to emulate the effect of re-prerendering a page that is
  // about to expire, because it was re-requested for prerendering a second
  // time after the actual prerender being kept around.
  base::Time start_time;
  // The actual time this page was last requested for prerendering.
  base::Time actual_start_time;
  URLID url_id;
  double priority;
  GURL url;
};

PrerenderLocalPredictor::PrerenderLocalPredictor(
    PrerenderManager* prerender_manager)
    : prerender_manager_(prerender_manager) {
  RecordEvent(EVENT_CONSTRUCTED);
  if (MessageLoop::current()) {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(kInitDelayMs),
                 this,
                 &PrerenderLocalPredictor::Init);
    RecordEvent(EVENT_INIT_SCHEDULED);
  }
}

PrerenderLocalPredictor::~PrerenderLocalPredictor() {
  if (observing_history_service_.get())
    observing_history_service_->RemoveVisitDatabaseObserver(this);
}

void PrerenderLocalPredictor::OnAddVisit(const history::BriefVisitInfo& info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RecordEvent(EVENT_ADD_VISIT);
  if (!visit_history_.get())
    return;
  visit_history_->push_back(info);
  if (static_cast<int>(visit_history_->size()) > kVisitHistoryPruneThreshold) {
    visit_history_->erase(visit_history_->begin(),
                          visit_history_->begin() + kVisitHistoryPruneAmount);
  }
  RecordEvent(EVENT_ADD_VISIT_INITIALIZED);
  if (current_prerender_.get() &&
      current_prerender_->url_id == info.url_id &&
      IsPrerenderStillValid(current_prerender_.get())) {
    prerender_manager_->histograms()->RecordLocalPredictorTimeUntilUsed(
        GetCurrentTime() - current_prerender_->actual_start_time,
        base::TimeDelta::FromMilliseconds(kMaxLocalPredictionTimeMs));
    last_swapped_in_prerender_.reset(current_prerender_.release());
    RecordEvent(EVENT_ADD_VISIT_PRERENDER_IDENTIFIED);
  }
  if (ShouldExcludeTransitionForPrediction(info.transition))
    return;
  RecordEvent(EVENT_ADD_VISIT_RELEVANT_TRANSITION);
  base::TimeDelta max_age =
      base::TimeDelta::FromMilliseconds(kMaxLocalPredictionTimeMs);
  base::TimeDelta min_age =
      base::TimeDelta::FromMilliseconds(kMinLocalPredictionTimeMs);
  std::set<URLID> next_urls_currently_found;
  std::map<URLID, int> next_urls_num_found;
  int num_occurrences_of_current_visit = 0;
  base::Time last_visited;
  URLID best_next_url = 0;
  int best_next_url_count = 0;
  const std::vector<history::BriefVisitInfo>& visits = *(visit_history_.get());
  for (int i = 0; i < static_cast<int>(visits.size()); i++) {
    if (!ShouldExcludeTransitionForPrediction(visits[i].transition)) {
      if (visits[i].url_id == info.url_id) {
        last_visited = visits[i].time;
        num_occurrences_of_current_visit++;
        next_urls_currently_found.clear();
        continue;
      }
      if (!last_visited.is_null() &&
          last_visited > visits[i].time - max_age &&
          last_visited < visits[i].time - min_age) {
        next_urls_currently_found.insert(visits[i].url_id);
      }
    }
    if (i == static_cast<int>(visits.size()) - 1 ||
        visits[i+1].url_id == info.url_id) {
      for (std::set<URLID>::iterator it = next_urls_currently_found.begin();
           it != next_urls_currently_found.end();
           ++it) {
        std::pair<std::map<URLID, int>::iterator, bool> insert_ret =
            next_urls_num_found.insert(std::pair<URLID, int>(*it, 0));
        std::map<URLID, int>::iterator num_found_it = insert_ret.first;
        num_found_it->second++;
        if (num_found_it->second > best_next_url_count) {
          best_next_url_count = num_found_it->second;
          best_next_url = *it;
        }
      }
    }
  }

  // Only consider a candidate next page for prerendering if it was viewed
  // at least twice, and at least 10% of the time.
  if (num_occurrences_of_current_visit > 0 &&
      best_next_url_count > 1 &&
      best_next_url_count * 10 >= num_occurrences_of_current_visit) {
    RecordEvent(EVENT_ADD_VISIT_IDENTIFIED_PRERENDER_CANDIDATE);
    double priority = static_cast<double>(best_next_url_count) /
        static_cast<double>(num_occurrences_of_current_visit);
    base::Time current_time = GetCurrentTime();
    if (!current_prerender_.get() ||
        current_prerender_->priority < priority ||
        current_prerender_->start_time < current_time - max_age) {
      RecordEvent(EVENT_ADD_VISIT_PRERENDERING);
      if (!current_prerender_.get() ||
          current_prerender_->url_id != best_next_url) {
        current_prerender_.reset(new PrerenderData());
        current_prerender_->url_id = best_next_url;
        current_prerender_->priority = priority;
        current_prerender_->start_time = current_time;
      } else {
        RecordEvent(EVENT_ADD_VISIT_PRERENDERING_EXTENDED);
        if (priority > current_prerender_->priority)
          current_prerender_->priority = priority;
        // If the prerender already existed, we want to extend it.  However,
        // we do not want to set its start_time to the current time to
        // disadvantage PLT computations when the prerender is swapped in.
        // So we set the new start time to current_time - 10s (since the vast
        // majority of PLTs are < 10s), provided that is not before the actual
        // time the prerender was started (so as to not artificially advantage
        // the PLT computation).
        base::Time simulated_new_start_time =
            current_time - base::TimeDelta::FromSeconds(10);
        if (simulated_new_start_time > current_prerender_->start_time)
          current_prerender_->start_time = simulated_new_start_time;
      }
      current_prerender_->actual_start_time = current_time;
      if (current_prerender_->url.is_empty()) {
        HistoryService* history = GetHistoryIfExists();
        if (history) {
          history->ScheduleDBTask(
              new GetURLForURLIDTask(this, current_prerender_->url_id),
              &history_db_consumer_);
        }
      }
    }
  }
}

void PrerenderLocalPredictor::OnLookupURL(history::URLID url_id,
                                          const GURL& url) {
  if (current_prerender_.get() && current_prerender_->url_id == url_id) {
    current_prerender_->url = url;
    RecordEvent(EVENT_GOT_PRERENDER_URL);
  }
  RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT);
  if ((url.path() == "/" || url.path() == "") && (!url.has_query()))
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT_ROOT_PAGE);
  if (url.SchemeIs("http"))
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT_IS_HTTP);
  if (url.has_query())
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT_HAS_QUERY_STRING);
  if (StrCaseStr(url.spec().c_str(), "logout") ||
      StrCaseStr(url.spec().c_str(), "signout"))
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT_CONTAINS_LOGOUT);
  if (StrCaseStr(url.spec().c_str(), "login") ||
      StrCaseStr(url.spec().c_str(), "signin"))
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT_CONTAINS_LOGIN);
}

void PrerenderLocalPredictor::OnGetInitialVisitHistory(
    scoped_ptr<std::vector<history::BriefVisitInfo> > visit_history) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!visit_history_.get());
  RecordEvent(EVENT_INIT_SUCCEEDED);
  // Since the visit history has descending timestamps, we must reverse it.
  visit_history_.reset(new std::vector<history::BriefVisitInfo>(
      visit_history->rbegin(), visit_history->rend()));
}

HistoryService* PrerenderLocalPredictor::GetHistoryIfExists() const {
  Profile* profile = prerender_manager_->profile();
  if (!profile)
    return NULL;
  return HistoryServiceFactory::GetForProfileIfExists(profile);
}

void PrerenderLocalPredictor::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RecordEvent(EVENT_INIT_STARTED);
  HistoryService* history = GetHistoryIfExists();
  if (!history) {
    RecordEvent(EVENT_INIT_FAILED_NO_HISTORY);
    return;
  }
  history->ScheduleDBTask(
      new GetVisitHistoryTask(this, kMaxVisitHistory),
      &history_db_consumer_);
  observing_history_service_ = history;
  observing_history_service_->AddVisitDatabaseObserver(this);
}

void PrerenderLocalPredictor::OnPLTEventForURL(const GURL& url,
                                               base::TimeDelta page_load_time) {
  scoped_ptr<PrerenderData> prerender;
  if (DoesPrerenderMatchPLTRecord(last_swapped_in_prerender_.get(),
                                  url, page_load_time)) {
    prerender.reset(last_swapped_in_prerender_.release());
  }
  if (DoesPrerenderMatchPLTRecord(current_prerender_.get(),
                                  url, page_load_time)) {
    prerender.reset(current_prerender_.release());
  }
  if (!prerender.get())
    return;
  if (IsPrerenderStillValid(prerender.get())) {
    base::TimeDelta prerender_age = GetCurrentTime() - prerender->start_time;
    prerender_manager_->histograms()->RecordSimulatedLocalBrowsingBaselinePLT(
        page_load_time, url);
    if (prerender_age > page_load_time) {
      base::TimeDelta new_plt;
      if (prerender_age <  2 * page_load_time)
        new_plt = 2 * page_load_time - prerender_age;
      prerender_manager_->histograms()->RecordSimulatedLocalBrowsingPLT(
          new_plt, url);
    }

  }
}

bool PrerenderLocalPredictor::IsPrerenderStillValid(
    PrerenderLocalPredictor::PrerenderData* prerender) const {
  return (prerender &&
          (prerender->start_time +
           base::TimeDelta::FromMilliseconds(kMaxLocalPredictionTimeMs))
          > GetCurrentTime());
}

void PrerenderLocalPredictor::RecordEvent(PrerenderLocalPredictor::Event event)
    const {
  prerender_manager_->histograms()->RecordLocalPredictorEvent(event);
}

bool PrerenderLocalPredictor::DoesPrerenderMatchPLTRecord(
    PrerenderData* prerender, const GURL& url, base::TimeDelta plt) const {
  if (prerender && prerender->start_time < GetCurrentTime() - plt) {
    if (prerender->url.is_empty())
      RecordEvent(EVENT_ERROR_NO_PRERENDER_URL_FOR_PLT);
    return (prerender->url == url);
  } else {
    return false;
  }
}

}  // namespace prerender
