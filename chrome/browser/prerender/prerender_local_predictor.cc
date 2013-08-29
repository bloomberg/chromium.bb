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
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/page_transition_types.h"
#include "crypto/secure_hash.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/url_canon.h"

using content::BrowserThread;
using content::PageTransition;
using content::SessionStorageNamespace;
using content::WebContents;
using history::URLID;
using predictors::LoggedInPredictorTable;
using std::string;
using std::vector;

namespace prerender {

namespace {

static const size_t kURLHashSize = 5;
static const int kNumPrerenderCandidates = 5;

}  // namespace

// When considering a candidate URL to be prerendered, we need to collect the
// data in this struct to make the determination whether we should issue the
// prerender or not.
struct PrerenderLocalPredictor::LocalPredictorURLInfo {
  URLID id;
  GURL url;
  bool url_lookup_success;
  bool logged_in;
  bool logged_in_lookup_ok;
  double priority;
};

// A struct consisting of everything needed for launching a potential prerender
// on a navigation: The navigation URL (source) triggering potential prerenders,
// and a set of candidate URLs.
struct PrerenderLocalPredictor::LocalPredictorURLLookupInfo {
  LocalPredictorURLInfo source_url_;
  vector<LocalPredictorURLInfo> candidate_urls_;
  explicit LocalPredictorURLLookupInfo(URLID source_id) {
    source_url_.id = source_id;
  }
  void MaybeAddCandidateURL(URLID id, double priority) {
    // TODO(tburkard): clean up this code, potentially using a list or a heap
    LocalPredictorURLInfo info;
    info.id = id;
    info.priority = priority;
    int insert_pos = candidate_urls_.size();
    if (insert_pos < kNumPrerenderCandidates)
      candidate_urls_.push_back(info);
    while (insert_pos > 0 &&
           candidate_urls_[insert_pos - 1].priority < info.priority) {
      if (insert_pos < kNumPrerenderCandidates)
        candidate_urls_[insert_pos] = candidate_urls_[insert_pos - 1];
      insert_pos--;
    }
    if (insert_pos < kNumPrerenderCandidates)
      candidate_urls_[insert_pos] = info;
  }
};

namespace {

// Task to lookup the URL for a given URLID.
class GetURLForURLIDTask : public history::HistoryDBTask {
 public:
  GetURLForURLIDTask(
      PrerenderLocalPredictor::LocalPredictorURLLookupInfo* request,
      const base::Closure& callback)
      : request_(request),
        callback_(callback),
        start_time_(base::Time::Now()) {
  }

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    DoURLLookup(db, &request_->source_url_);
    for (int i = 0; i < static_cast<int>(request_->candidate_urls_.size()); i++)
      DoURLLookup(db, &request_->candidate_urls_[i]);
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {
    callback_.Run();
    UMA_HISTOGRAM_CUSTOM_TIMES("Prerender.LocalPredictorURLLookupTime",
                               base::Time::Now() - start_time_,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromSeconds(10),
                               50);
  }

 private:
  virtual ~GetURLForURLIDTask() {}

  void DoURLLookup(history::HistoryDatabase* db,
                   PrerenderLocalPredictor::LocalPredictorURLInfo* request) {
    history::URLRow url_row;
    request->url_lookup_success = db->GetURLRow(request->id, &url_row);
    if (request->url_lookup_success)
      request->url = url_row.url();
  }

  PrerenderLocalPredictor::LocalPredictorURLLookupInfo* request_;
  base::Closure callback_;
  base::Time start_time_;
  DISALLOW_COPY_AND_ASSIGN(GetURLForURLIDTask);
};

// Task to load history from the visit database on startup.
class GetVisitHistoryTask : public history::HistoryDBTask {
 public:
  GetVisitHistoryTask(PrerenderLocalPredictor* local_predictor,
                      int max_visits)
      : local_predictor_(local_predictor),
        max_visits_(max_visits),
        visit_history_(new vector<history::BriefVisitInfo>) {
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
  scoped_ptr<vector<history::BriefVisitInfo> > visit_history_;
  DISALLOW_COPY_AND_ASSIGN(GetVisitHistoryTask);
};

// Maximum visit history to retrieve from the visit database.
const int kMaxVisitHistory = 100 * 1000;

// Visit history size at which to trigger pruning, and number of items to prune.
const int kVisitHistoryPruneThreshold = 120 * 1000;
const int kVisitHistoryPruneAmount = 20 * 1000;

const int kMinLocalPredictionTimeMs = 500;

int GetMaxLocalPredictionTimeMs() {
  return GetLocalPredictorTTLSeconds() * 1000;
}

bool IsBackForward(PageTransition transition) {
  return (transition & content::PAGE_TRANSITION_FORWARD_BACK) != 0;
}

bool IsHomePage(PageTransition transition) {
  return (transition & content::PAGE_TRANSITION_HOME_PAGE) != 0;
}

bool IsIntermediateRedirect(PageTransition transition) {
  return (transition & content::PAGE_TRANSITION_CHAIN_END) == 0;
}

bool IsFormSubmit(PageTransition transition) {
  return (transition & content::PAGE_TRANSITION_FORM_SUBMIT) != 0;
}

bool ShouldExcludeTransitionForPrediction(PageTransition transition) {
  return IsBackForward(transition) || IsHomePage(transition) ||
      IsIntermediateRedirect(transition);
}

base::Time GetCurrentTime() {
  return base::Time::Now();
}

bool StringContainsIgnoringCase(string haystack, string needle) {
  std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
  std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
  return haystack.find(needle) != string::npos;
}

bool IsExtendedRootURL(const GURL& url) {
  const string& path = url.path();
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

bool IsLogInURL(const GURL& url) {
  return StringContainsIgnoringCase(url.spec().c_str(), "login") ||
      StringContainsIgnoringCase(url.spec().c_str(), "signin");
}

bool IsLogOutURL(const GURL& url) {
  return StringContainsIgnoringCase(url.spec().c_str(), "logout") ||
      StringContainsIgnoringCase(url.spec().c_str(), "signout");
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

bool URLsIdenticalIgnoringFragments(const GURL& url1, const GURL& url2) {
  url_canon::Replacements<char> replacement;
  replacement.ClearRef();
  GURL u1 = url1.ReplaceComponents(replacement);
  GURL u2 = url2.ReplaceComponents(replacement);
  return (u1 == u2);
}

void LookupLoggedInStatesOnDBThread(
    scoped_refptr<LoggedInPredictorTable> logged_in_predictor_table,
    PrerenderLocalPredictor::LocalPredictorURLLookupInfo* request_) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  for (int i = 0; i < static_cast<int>(request_->candidate_urls_.size()); i++) {
    PrerenderLocalPredictor::LocalPredictorURLInfo* info =
        &request_->candidate_urls_[i];
    if (info->url_lookup_success) {
      logged_in_predictor_table->HasUserLoggedIn(
          info->url, &info->logged_in, &info->logged_in_lookup_ok);
    } else {
      info->logged_in_lookup_ok = false;
    }
  }
}

}  // namespace

struct PrerenderLocalPredictor::PrerenderProperties {
  PrerenderProperties(URLID url_id, const GURL& url, double priority,
                base::Time start_time)
      : url_id(url_id),
        url(url),
        priority(priority),
        start_time(start_time),
        would_have_matched(false) {
  }

  // Default constructor for dummy element
  PrerenderProperties()
      : priority(0.0), would_have_matched(false) {
  }

  double GetCurrentDecayedPriority() {
    // If we are no longer prerendering, the priority is 0.
    if (!prerender_handle || !prerender_handle->IsPrerendering())
      return 0.0;
    int half_life_time_seconds =
        GetLocalPredictorPrerenderPriorityHalfLifeTimeSeconds();
    if (half_life_time_seconds < 1)
      return priority;
    double multiple_elapsed =
        (GetCurrentTime() - actual_start_time).InMillisecondsF() /
        base::TimeDelta::FromSeconds(half_life_time_seconds).InMillisecondsF();
    // Decay factor: 2 ^ (-multiple_elapsed)
    double decay_factor = exp(- multiple_elapsed * log(2.0));
    return priority * decay_factor;
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
  scoped_ptr<PrerenderHandle> prerender_handle;
  // Indicates whether this prerender would have matched a URL navigated to,
  // but was not swapped in for some reason.
  bool would_have_matched;
};

PrerenderLocalPredictor::PrerenderLocalPredictor(
    PrerenderManager* prerender_manager)
    : prerender_manager_(prerender_manager),
      is_visit_database_observer_(false),
      weak_factory_(this) {
  RecordEvent(EVENT_CONSTRUCTED);
  if (base::MessageLoop::current()) {
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
  for (int i = 0; i < static_cast<int>(issued_prerenders_.size()); i++) {
    PrerenderProperties* p = issued_prerenders_[i];
    DCHECK(p != NULL);
    if (p->prerender_handle)
      p->prerender_handle->OnCancel();
  }
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
        base::TimeDelta::FromMilliseconds(GetMaxLocalPredictionTimeMs()),
        50);
    last_swapped_in_prerender_.reset(current_prerender_.release());
    RecordEvent(EVENT_ADD_VISIT_PRERENDER_IDENTIFIED);
  }
  if (ShouldExcludeTransitionForPrediction(info.transition))
    return;
  RecordEvent(EVENT_ADD_VISIT_RELEVANT_TRANSITION);
  base::TimeDelta max_age =
      base::TimeDelta::FromMilliseconds(GetMaxLocalPredictionTimeMs());
  base::TimeDelta min_age =
      base::TimeDelta::FromMilliseconds(kMinLocalPredictionTimeMs);
  std::set<URLID> next_urls_currently_found;
  std::map<URLID, int> next_urls_num_found;
  int num_occurrences_of_current_visit = 0;
  base::Time last_visited;
  scoped_ptr<LocalPredictorURLLookupInfo> lookup_info(
      new LocalPredictorURLLookupInfo(info.url_id));
  const vector<history::BriefVisitInfo>& visits = *(visit_history_.get());
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
        if (!IsFormSubmit(visits[i].transition))
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
      }
    }
  }

  for (std::map<URLID, int>::const_iterator it = next_urls_num_found.begin();
       it != next_urls_num_found.end();
       ++it) {
    // Only consider a candidate next page for prerendering if it was viewed
    // at least twice, and at least 10% of the time.
    if (num_occurrences_of_current_visit > 0 &&
        it->second > 1 &&
        it->second * 10 >= num_occurrences_of_current_visit) {
      RecordEvent(EVENT_ADD_VISIT_IDENTIFIED_PRERENDER_CANDIDATE);
      double priority = static_cast<double>(it->second) /
          static_cast<double>(num_occurrences_of_current_visit);
      lookup_info->MaybeAddCandidateURL(it->first, priority);
    }
  }

  if (lookup_info->candidate_urls_.size() == 0) {
    RecordEvent(EVENT_NO_PRERENDER_CANDIDATES);
    return;
  }

  RecordEvent(EVENT_START_URL_LOOKUP);
  HistoryService* history = GetHistoryIfExists();
  if (history) {
    RecordEvent(EVENT_GOT_HISTORY_ISSUING_LOOKUP);
    LocalPredictorURLLookupInfo* lookup_info_ptr = lookup_info.get();
    history->ScheduleDBTask(
        new GetURLForURLIDTask(
            lookup_info_ptr,
            base::Bind(&PrerenderLocalPredictor::OnLookupURL,
                       base::Unretained(this),
                       base::Passed(&lookup_info))),
        &history_db_consumer_);
  }
}

void PrerenderLocalPredictor::OnLookupURL(
    scoped_ptr<LocalPredictorURLLookupInfo> info) {
  RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT);

  DCHECK_GE(static_cast<int>(info->candidate_urls_.size()), 1);

  if (!info->source_url_.url_lookup_success) {
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_FAILED);
    return;
  }

  LogCandidateURLStats(info->candidate_urls_[0].url);

  WebContents* source_web_contents = NULL;
  bool multiple_source_web_contents_candidates = false;

#if !defined(OS_ANDROID)
  // We need to figure out what tab launched the prerender. We do this by
  // comparing URLs. This may not always work: the URL may occur in two
  // tabs, and we pick the wrong one, or the tab we should have picked
  // may have navigated elsewhere. Hopefully, this doesn't happen too often,
  // so we ignore these cases for now.
  // TODO(tburkard): Reconsider this, potentially measure it, and fix this
  // in the future.
  for (TabContentsIterator it; !it.done(); it.Next()) {
    if (it->GetURL() == info->source_url_.url) {
      if (!source_web_contents)
        source_web_contents = *it;
      else
        multiple_source_web_contents_candidates = true;
    }
  }
#endif

  if (!source_web_contents) {
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_NO_SOURCE_WEBCONTENTS_FOUND);
    return;
  }

  if (multiple_source_web_contents_candidates)
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_MULTIPLE_SOURCE_WEBCONTENTS_FOUND);


  scoped_refptr<SessionStorageNamespace> session_storage_namespace =
      source_web_contents->GetController().GetDefaultSessionStorageNamespace();

  gfx::Rect container_bounds;
  source_web_contents->GetView()->GetContainerBounds(&container_bounds);
  scoped_ptr<gfx::Size> size(new gfx::Size(container_bounds.size()));

  scoped_refptr<LoggedInPredictorTable> logged_in_table =
      prerender_manager_->logged_in_predictor_table();

  if (!logged_in_table.get()) {
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_NO_LOGGED_IN_TABLE_FOUND);
    return;
  }

  RecordEvent(EVENT_PRERENDER_URL_LOOKUP_ISSUING_LOGGED_IN_LOOKUP);

  LocalPredictorURLLookupInfo* info_ptr = info.get();
  BrowserThread::PostTaskAndReply(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&LookupLoggedInStatesOnDBThread,
                 logged_in_table,
                 info_ptr),
      base::Bind(&PrerenderLocalPredictor::ContinuePrerenderCheck,
                 weak_factory_.GetWeakPtr(),
                 session_storage_namespace,
                 base::Passed(&size),
                 base::Passed(&info)));
}

void PrerenderLocalPredictor::LogCandidateURLStats(const GURL& url) const {
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
  if (IsLogOutURL(url))
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT_CONTAINS_LOGOUT);
  if (IsLogInURL(url))
    RecordEvent(EVENT_PRERENDER_URL_LOOKUP_RESULT_CONTAINS_LOGIN);
}

void PrerenderLocalPredictor::OnGetInitialVisitHistory(
    scoped_ptr<vector<history::BriefVisitInfo> > visit_history) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!visit_history_.get());
  RecordEvent(EVENT_INIT_SUCCEEDED);
  // Since the visit history has descending timestamps, we must reverse it.
  visit_history_.reset(new vector<history::BriefVisitInfo>(
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
  scoped_ptr<PrerenderProperties> prerender;
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
    PrerenderLocalPredictor::PrerenderProperties* prerender) const {
  return (prerender &&
          (prerender->start_time +
           base::TimeDelta::FromMilliseconds(GetMaxLocalPredictionTimeMs()))
          > GetCurrentTime());
}

void PrerenderLocalPredictor::RecordEvent(
    PrerenderLocalPredictor::Event event) const {
  UMA_HISTOGRAM_ENUMERATION("Prerender.LocalPredictorEvent",
      event, PrerenderLocalPredictor::EVENT_MAX_VALUE);
}

bool PrerenderLocalPredictor::DoesPrerenderMatchPLTRecord(
    PrerenderProperties* prerender,
    const GURL& url,
    base::TimeDelta plt) const {
  if (prerender && prerender->start_time < GetCurrentTime() - plt) {
    if (prerender->url.is_empty())
      RecordEvent(EVENT_ERROR_NO_PRERENDER_URL_FOR_PLT);
    return (prerender->url == url);
  } else {
    return false;
  }
}

PrerenderLocalPredictor::PrerenderProperties*
PrerenderLocalPredictor::GetIssuedPrerenderSlotForPriority(double priority) {
  int num_prerenders = GetLocalPredictorMaxConcurrentPrerenders();
  while (static_cast<int>(issued_prerenders_.size()) < num_prerenders)
    issued_prerenders_.push_back(new PrerenderProperties());
  PrerenderProperties* lowest_priority_prerender = NULL;
  for (int i = 0; i < static_cast<int>(issued_prerenders_.size()); i++) {
    PrerenderProperties* p = issued_prerenders_[i];
    DCHECK(p != NULL);
    if (!p->prerender_handle || !p->prerender_handle->IsPrerendering())
      return p;
    double decayed_priority = p->GetCurrentDecayedPriority();
    if (decayed_priority > priority)
      continue;
    if (lowest_priority_prerender == NULL ||
        lowest_priority_prerender->GetCurrentDecayedPriority() >
        decayed_priority) {
      lowest_priority_prerender = p;
    }
  }
  return lowest_priority_prerender;
}

void PrerenderLocalPredictor::ContinuePrerenderCheck(
    scoped_refptr<SessionStorageNamespace> session_storage_namespace,
    scoped_ptr<gfx::Size> size,
    scoped_ptr<LocalPredictorURLLookupInfo> info) {
  RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_STARTED);
  scoped_ptr<LocalPredictorURLInfo> url_info;
  scoped_refptr<SafeBrowsingDatabaseManager> sb_db_manager =
      g_browser_process->safe_browsing_service()->database_manager();
  PrerenderProperties* prerender_properties = NULL;

  for (int i = 0; i < static_cast<int>(info->candidate_urls_.size()); i++) {
    RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_EXAMINE_NEXT_URL);
    url_info.reset(new LocalPredictorURLInfo(info->candidate_urls_[i]));

    // We need to check whether we can issue a prerender for this URL.
    // We test a set of conditions. Each condition can either rule out
    // a prerender (in which case we reset url_info, so that it will not
    // be prerendered, and we continue, which means try the next candidate
    // URL), or it can be sufficient to issue the prerender without any
    // further checks (in which case we just break).
    // The order of the checks is critical, because it prescribes the logic
    // we use here to decide what to prerender.
    if (!url_info->url_lookup_success) {
      RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_NO_URL);
      url_info.reset(NULL);
      continue;
    }
    prerender_properties =
        GetIssuedPrerenderSlotForPriority(url_info->priority);
    if (!prerender_properties) {
      RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_PRIORITY_TOO_LOW);
      url_info.reset(NULL);
      continue;
    }
    if (!SkipLocalPredictorFragment() &&
        URLsIdenticalIgnoringFragments(info->source_url_.url,
                                       url_info->url)) {
      RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_URLS_IDENTICAL_BUT_FRAGMENT);
      url_info.reset(NULL);
      continue;
    }
    if (!SkipLocalPredictorHTTPS() && url_info->url.SchemeIs("https")) {
      RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_HTTPS);
      url_info.reset(NULL);
      continue;
    }
    if (IsRootPageURL(url_info->url)) {
      // For root pages, we assume that they are reasonably safe, and we
      // will just prerender them without any additional checks.
      RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_ROOT_PAGE);
      break;
    }
    if (IsLogOutURL(url_info->url)) {
      RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_LOGOUT_URL);
      url_info.reset(NULL);
      continue;
    }
    if (IsLogInURL(url_info->url)) {
      RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_LOGIN_URL);
      url_info.reset(NULL);
      continue;
    }
    if (!SkipLocalPredictorWhitelist() &&
        sb_db_manager->CheckSideEffectFreeWhitelistUrl(url_info->url)) {
      // If a page is on the side-effect free whitelist, we will just prerender
      // it without any additional checks.
      RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_ON_SIDE_EFFECT_FREE_WHITELIST);
      break;
    }
    if (!SkipLocalPredictorLoggedIn() &&
        !url_info->logged_in && url_info->logged_in_lookup_ok) {
      RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_NOT_LOGGED_IN);
      break;
    }
    if (!SkipLocalPredictorDefaultNoPrerender()) {
      RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_FALLTHROUGH_NOT_PRERENDERING);
      url_info.reset(NULL);
    } else {
      RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_FALLTHROUGH_PRERENDERING);
    }
  }
  if (!url_info.get())
    return;
  RecordEvent(EVENT_CONTINUE_PRERENDER_CHECK_ISSUING_PRERENDER);
  DCHECK(prerender_properties != NULL);
  if (IsLocalPredictorPrerenderLaunchEnabled()) {
    IssuePrerender(session_storage_namespace, size.Pass(),
                   url_info.Pass(), prerender_properties);
  }
}

void PrerenderLocalPredictor::IssuePrerender(
    scoped_refptr<SessionStorageNamespace> session_storage_namespace,
    scoped_ptr<gfx::Size> size,
    scoped_ptr<LocalPredictorURLInfo> info,
    PrerenderProperties* prerender_properties) {
  URLID url_id = info->id;
  const GURL& url = info->url;
  double priority = info->priority;
  base::Time current_time = GetCurrentTime();
  RecordEvent(EVENT_ISSUING_PRERENDER);

  // Issue the prerender and obtain a new handle.
  scoped_ptr<prerender::PrerenderHandle> new_prerender_handle(
      prerender_manager_->AddPrerenderFromLocalPredictor(
          url, session_storage_namespace.get(), *size));

  // Check if this is a duplicate of an existing prerender. If yes, clean up
  // the new handle.
  for (int i = 0; i < static_cast<int>(issued_prerenders_.size()); i++) {
    PrerenderProperties* p = issued_prerenders_[i];
    DCHECK(p != NULL);
    if (new_prerender_handle &&
        new_prerender_handle->RepresentingSamePrerenderAs(
            p->prerender_handle.get())) {
      new_prerender_handle->OnCancel();
      new_prerender_handle.reset(NULL);
      RecordEvent(EVENT_ISSUE_PRERENDER_ALREADY_PRERENDERING);
      break;
    }
  }

  if (new_prerender_handle.get()) {
    RecordEvent(EVENT_ISSUE_PRERENDER_NEW_PRERENDER);
    // The new prerender does not match any existing prerenders. Update
    // prerender_properties so that it reflects the new entry.
    prerender_properties->url_id = url_id;
    prerender_properties->url = url;
    prerender_properties->priority = priority;
    prerender_properties->start_time = current_time;
    prerender_properties->actual_start_time = current_time;
    prerender_properties->would_have_matched = false;
    prerender_properties->prerender_handle.swap(new_prerender_handle);
    // new_prerender_handle now represents the old previou prerender that we
    // are replacing. So we need to cancel it.
    if (new_prerender_handle) {
      new_prerender_handle->OnCancel();
      RecordEvent(EVENT_ISSUE_PRERENDER_CANCELLED_OLD_PRERENDER);
    }
  }

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
        new PrerenderProperties(url_id, url, priority, current_time));
  }
  current_prerender_->actual_start_time = current_time;
}

void PrerenderLocalPredictor::OnTabHelperURLSeen(
    const GURL& url, WebContents* web_contents) {
  RecordEvent(EVENT_TAB_HELPER_URL_SEEN);

  // If the namespace matches and the URL matches, we might be able to swap
  // in. However, the actual code initating the swapin is in the renderer
  // and is checking for other criteria (such as POSTs). There may
  // also be conditions when a swapin should happen but does not. By recording
  // the two previous events, we can keep an eye on the magnitude of the
  // discrepancy.

  PrerenderProperties* best_matched_prerender = NULL;
  bool session_storage_namespace_matches = false;
  for (int i = 0; i < static_cast<int>(issued_prerenders_.size()); i++) {
    PrerenderProperties* p = issued_prerenders_[i];
    DCHECK(p != NULL);
    if (!p->prerender_handle.get() ||
        !p->prerender_handle->Matches(url, NULL) ||
        p->would_have_matched) {
      continue;
    }
    if (!best_matched_prerender || !session_storage_namespace_matches) {
      best_matched_prerender = p;
      session_storage_namespace_matches =
          p->prerender_handle->Matches(
              url,
              web_contents->GetController().
              GetDefaultSessionStorageNamespace());
    }
  }
  if (best_matched_prerender) {
    RecordEvent(EVENT_TAB_HELPER_URL_SEEN_MATCH);
    best_matched_prerender->would_have_matched = true;
    if (session_storage_namespace_matches)
      RecordEvent(EVENT_TAB_HELPER_URL_SEEN_NAMESPACE_MATCH);
  }
}

}  // namespace prerender
