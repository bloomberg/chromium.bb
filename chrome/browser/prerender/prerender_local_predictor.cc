// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_local_predictor.h"

#include <ctype.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/timer.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/page_transition_types.h"
#include "crypto/secure_hash.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;
using content::PageTransition;
using history::URLID;

namespace prerender {

namespace {

static const size_t kURLHashSize = 5;

// Task to lookup the URL for a given URLID.
class GetURLForURLIDTask : public history::HistoryDBTask {
 public:
  GetURLForURLIDTask(URLID url_id, base::Callback<void(const GURL&)> callback)
      : url_id_(url_id),
        success_(false),
        callback_(callback),
        start_time_(base::Time::Now()) {
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
    if (success_) {
      callback_.Run(url_);
      UMA_HISTOGRAM_CUSTOM_TIMES("Prerender.LocalPredictorURLLookupTime",
                                 base::Time::Now() - start_time_,
                                 base::TimeDelta::FromMilliseconds(10),
                                 base::TimeDelta::FromSeconds(10),
                                 50);
    }
  }

 private:
  virtual ~GetURLForURLIDTask() {}

  URLID url_id_;
  bool success_;
  base::Callback<void(const GURL&)> callback_;
  base::Time start_time_;
  GURL url_;
  DISALLOW_COPY_AND_ASSIGN(GetURLForURLIDTask);
};

// Task to load history from the visit database on startup.
class GetVisitHistoryTask : public history::HistoryDBTask {
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
  return haystack.find(needle) != std::string::npos;
}

bool IsExtendedRootURL(const GURL& url) {
  const std::string& path = url.path();
  return path == "/index.html" || path == "/home.html" ||
      path == "/main.html" ||
      path == "/index.htm" || path == "/home.htm" || path == "/main.htm" ||
      path == "/index.php" || path == "/home.php" || path == "/main.php" ||
      path == "/index.asp" || path == "/home.asp" || path == "/main.asp" ||
      path == "/index.py" || path == "/home.py" || path == "/main.py" ||
      path == "/index.pl" || path == "/home.pl" || path == "/main.pl";
}

bool IsRootPageURL(const GURL& url) {
  return (url.path() == "/" || url.path() == "" || IsExtendedRootURL(url)) &&
      (!url.has_query()) && (!url.has_ref());
}

int64 URLHashToInt64(const unsigned char* data) {
  COMPILE_ASSERT(kURLHashSize < sizeof(int64), url_hash_must_fit_in_int64);
  int64 value = 0;
  memcpy(&value, data, kURLHashSize);
  return value;
}

int64 GetInt64URLHashForURL(const GURL& url) {
  COMPILE_ASSERT(kURLHashSize < sizeof(int64), url_hash_must_fit_in_int64);
  scoped_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  int64 hash_value = 0;
  const char* url_string = url.spec().c_str();
  hash->Update(url_string, strlen(url_string));
  hash->Finish(&hash_value, kURLHashSize);
  return hash_value;
}

}  // namespace

struct PrerenderLocalPredictor::PrerenderData {
  PrerenderData(URLID url_id, const GURL& url, double priority,
                base::Time start_time)
      : url_id(url_id),
        url(url),
        priority(priority),
        start_time(start_time) {
  }

  URLID url_id;
  GURL url;
  double priority;
  // For expiration purposes, this is a synthetic start time consisting either
  // of the actual start time, or of the last time the page was re-requested
  // for prerendering - 10 seconds (unless the original request came after
  // that).  This is to emulate the effect of re-prerendering a page that is
  // about to expire, because it was re-requested for prerendering a second
  // time after the actual prerender being kept around.
  base::Time start_time;
  // The actual time this page was last requested for prerendering.
  base::Time actual_start_time;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PrerenderData);
};

PrerenderLocalPredictor::PrerenderLocalPredictor(
    PrerenderManager* prerender_manager)
    : prerender_manager_(prerender_manager),
      is_visit_database_observer_(false) {
  RecordEvent(EVENT_CONSTRUCTED);
  if (MessageLoop::current()) {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(kInitDelayMs),
                 this,
                 &PrerenderLocalPredictor::Init);
    RecordEvent(EVENT_INIT_SCHEDULED);
  }

  static const size_t kChecksumHashSize = 32;
  base::RefCountedStaticMemory* url_whitelist_data =
      ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          IDR_PRERENDER_URL_WHITELIST);
  size_t size = url_whitelist_data->size();
  const unsigned char* front = url_whitelist_data->front();
  if (size < kChecksumHashSize ||
      (size - kChecksumHashSize) % kURLHashSize != 0) {
    RecordEvent(EVENT_URL_WHITELIST_ERROR);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hash->Update(front + kChecksumHashSize, size - kChecksumHashSize);
  char hash_value[kChecksumHashSize];
  hash->Finish(hash_value, kChecksumHashSize);
  if (memcmp(hash_value, front, kChecksumHashSize)) {
    RecordEvent(EVENT_URL_WHITELIST_ERROR);
    return;
  }
  for (const unsigned char* p = front + kChecksumHashSize;
       p < front + size;
       p += kURLHashSize) {
    url_whitelist_.insert(URLHashToInt64(p));
  }
  RecordEvent(EVENT_URL_WHITELIST_OK);
}

PrerenderLocalPredictor::~PrerenderLocalPredictor() {
  Shutdown();
}

void PrerenderLocalPredictor::Shutdown() {
  timer_.Stop();
  if (is_visit_database_observer_) {
    HistoryService* history = GetHistoryIfExists();
    CHECK(history);
    history->RemoveVisitDatabaseObserver(this);
    is_visit_database_observer_ = false;
  }
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
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Prerender.LocalPredictorTimeUntilUsed",
        GetCurrentTime() - current_prerender_->actual_start_time,
        base::TimeDelta::FromMilliseconds(10),
        base::TimeDelta::FromMilliseconds(kMaxLocalPredictionTimeMs),
        50);
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
    if (ShouldReplaceCurrentPrerender(priority)) {
      RecordEvent(EVENT_START_URL_LOOKUP);
      HistoryService* history = GetHistoryIfExists();
      if (history) {
        history->ScheduleDBTask(
            new GetURLForURLIDTask(
                best_next_url,
                base::Bind(&PrerenderLocalPredictor::OnLookupURL,
                           base::Unretained(this),
                           best_next_url,
                           priority)),
            &history_db_consumer_);
      }
    }
  }
}

void PrerenderLocalPredictor::OnLookupURL(history::URLID url_id,
                                          double priority,
                                          const GURL& url) {
  RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT);

  base::Time current_time = GetCurrentTime();
  if (ShouldReplaceCurrentPrerender(priority)) {
    if (IsRootPageURL(url)) {
      RecordEvent(EVENT_ADD_VISIT_PRERENDERING);
      if (current_prerender_.get() && current_prerender_->url_id == url_id) {
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
      } else {
        current_prerender_.reset(
            new PrerenderData(url_id, url, priority, current_time));
      }
      current_prerender_->actual_start_time = current_time;
    } else {
      RecordEvent(EVENT_ADD_VISIT_NOT_ROOTPAGE);
    }
  }

  if (url_whitelist_.count(GetInt64URLHashForURL(url)) > 0) {
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT_ON_WHITELIST);
    if (IsRootPageURL(url))
      RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT_ON_WHITELIST_ROOT_PAGE);
  }
  if (IsRootPageURL(url))
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT_ROOT_PAGE);
  if (IsExtendedRootURL(url))
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT_EXTENDED_ROOT_PAGE);
  if (IsRootPageURL(url) && url.SchemeIs("http"))
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT_ROOT_PAGE_HTTP);
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
  return HistoryServiceFactory::GetForProfileWithoutCreating(profile);
}

void PrerenderLocalPredictor::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RecordEvent(EVENT_INIT_STARTED);
  HistoryService* history = GetHistoryIfExists();
  if (history) {
    CHECK(!is_visit_database_observer_);
    history->ScheduleDBTask(
        new GetVisitHistoryTask(this, kMaxVisitHistory),
        &history_db_consumer_);
    history->AddVisitDatabaseObserver(this);
    is_visit_database_observer_ = true;
  } else {
    RecordEvent(EVENT_INIT_FAILED_NO_HISTORY);
  }
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
    UMA_HISTOGRAM_CUSTOM_TIMES("Prerender.SimulatedLocalBrowsingBaselinePLT",
                               page_load_time,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromSeconds(60),
                               100);

    base::TimeDelta prerender_age = GetCurrentTime() - prerender->start_time;
    if (prerender_age > page_load_time) {
      base::TimeDelta new_plt;
      if (prerender_age <  2 * page_load_time)
        new_plt = 2 * page_load_time - prerender_age;
      UMA_HISTOGRAM_CUSTOM_TIMES("Prerender.SimulatedLocalBrowsingPLT",
                                 new_plt,
                                 base::TimeDelta::FromMilliseconds(10),
                                 base::TimeDelta::FromSeconds(60),
                                 100);
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

void PrerenderLocalPredictor::RecordEvent(
    PrerenderLocalPredictor::Event event) const {
  UMA_HISTOGRAM_ENUMERATION("Prerender.LocalPredictorEvent",
      event, PrerenderLocalPredictor::EVENT_MAX_VALUE);
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

bool PrerenderLocalPredictor::ShouldReplaceCurrentPrerender(
    double priority) const {
  base::TimeDelta max_age =
      base::TimeDelta::FromMilliseconds(kMaxLocalPredictionTimeMs);
  return (!current_prerender_.get()) ||
    current_prerender_->priority < priority ||
    current_prerender_->start_time < GetCurrentTime() - max_age;
}

}  // namespace prerender
