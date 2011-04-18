// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/render_view_host_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_util.h"

namespace prerender {

// static
int PrerenderManager::prerenders_per_session_count_ = 0;

// static
base::TimeTicks PrerenderManager::last_prefetch_seen_time_;

// static
PrerenderManager::PrerenderManagerMode PrerenderManager::mode_ =
    PRERENDER_MODE_ENABLED;

// static
PrerenderManager::PrerenderManagerMode PrerenderManager::GetMode() {
  return mode_;
}

// static
void PrerenderManager::SetMode(PrerenderManagerMode mode) {
  mode_ = mode;
}

// static
bool PrerenderManager::IsPrerenderingPossible() {
  return
      GetMode() == PRERENDER_MODE_ENABLED ||
      GetMode() == PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP ||
      GetMode() == PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP;
}

// static
bool PrerenderManager::IsControlGroup() {
  return GetMode() == PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP;
}

// static
bool PrerenderManager::MaybeGetQueryStringBasedAliasURL(
    const GURL& url, GURL* alias_url) {
  DCHECK(alias_url);
  url_parse::Parsed parsed;
  url_parse::ParseStandardURL(url.spec().c_str(), url.spec().length(),
                              &parsed);
  url_parse::Component query = parsed.query;
  url_parse::Component key, value;
  while (url_parse::ExtractQueryKeyValue(url.spec().c_str(), &query, &key,
                                         &value)) {
    if (key.len != 3 || strncmp(url.spec().c_str() + key.begin, "url", key.len))
      continue;
    // We found a url= query string component.
    if (value.len < 1)
      continue;
    url_canon::RawCanonOutputW<1024> decoded_url;
    url_util::DecodeURLEscapeSequences(url.spec().c_str() + value.begin,
                                       value.len, &decoded_url);
    GURL new_url(string16(decoded_url.data(), decoded_url.length()));
    if (!new_url.is_empty() && new_url.is_valid()) {
      *alias_url = new_url;
      return true;
    }
    return false;
  }
  return false;
}

struct PrerenderManager::PrerenderContentsData {
  PrerenderContents* contents_;
  base::Time start_time_;
  PrerenderContentsData(PrerenderContents* contents, base::Time start_time)
      : contents_(contents),
        start_time_(start_time) {
  }
};

struct PrerenderManager::PendingContentsData {
  PendingContentsData(const GURL& url, const std::vector<GURL>& alias_urls,
                      const GURL& referrer)
      : url_(url), alias_urls_(alias_urls), referrer_(referrer) { }
  ~PendingContentsData() {}
  GURL url_;
  std::vector<GURL> alias_urls_;
  GURL referrer_;
};


PrerenderManager::PrerenderManager(Profile* profile)
    : rate_limit_enabled_(true),
      enabled_(true),
      profile_(profile),
      max_prerender_age_(base::TimeDelta::FromSeconds(
          kDefaultMaxPrerenderAgeSeconds)),
      max_elements_(kDefaultMaxPrerenderElements),
      prerender_contents_factory_(PrerenderContents::CreateFactory()),
      last_prerender_start_time_(GetCurrentTimeTicks() -
          base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs)) {
}

PrerenderManager::~PrerenderManager() {
  while (!prerender_list_.empty()) {
    PrerenderContentsData data = prerender_list_.front();
    prerender_list_.pop_front();
    data.contents_->set_final_status(FINAL_STATUS_MANAGER_SHUTDOWN);
    delete data.contents_;
  }
}

void PrerenderManager::SetPrerenderContentsFactory(
    PrerenderContents::Factory* prerender_contents_factory) {
  prerender_contents_factory_.reset(prerender_contents_factory);
}

bool PrerenderManager::AddPreload(const GURL& url,
                                  const std::vector<GURL>& alias_urls,
                                  const GURL& referrer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DeleteOldEntries();
  if (FindEntry(url))
    return false;

  // Local copy, since we may have to add an additional entry to it.
  std::vector<GURL> all_alias_urls = alias_urls;

  GURL additional_alias_url;
  if (IsControlGroup() &&
      PrerenderManager::MaybeGetQueryStringBasedAliasURL(
          url, &additional_alias_url))
    all_alias_urls.push_back(additional_alias_url);

  // Do not prerender if there are too many render processes, and we would
  // have to use an existing one.  We do not want prerendering to happen in
  // a shared process, so that we can always reliably lower the CPU
  // priority for prerendering.
  // In single-process mode, ShouldTryToUseExistingProcessHost() always returns
  // true, so that case needs to be explicitly checked for.
  // TODO(tburkard): Figure out how to cancel prerendering in the opposite
  // case, when a new tab is added to a process used for prerendering.
  if (RenderProcessHost::ShouldTryToUseExistingProcessHost() &&
      !RenderProcessHost::run_renderer_in_process()) {
    // Only record the status if we are not in the control group.
    if (!IsControlGroup())
      RecordFinalStatus(FINAL_STATUS_TOO_MANY_PROCESSES);
    return false;
  }

  // Check if enough time has passed since the last prerender.
  if (!DoesRateLimitAllowPrerender()) {
    // Cancel the prerender. We could add it to the pending prerender list but
    // this doesn't make sense as the next prerender request will be triggered
    // by a navigation and is unlikely to be the same site.
    RecordFinalStatus(FINAL_STATUS_RATE_LIMIT_EXCEEDED);

    return false;
  }

  // TODO(cbentzel): Move invalid checks here instead of PrerenderContents?
  PrerenderContentsData data(CreatePrerenderContents(url, all_alias_urls,
                                                     referrer),
                             GetCurrentTime());

  prerender_list_.push_back(data);
  if (IsControlGroup()) {
    data.contents_->set_final_status(FINAL_STATUS_CONTROL_GROUP);
  } else {
    last_prerender_start_time_ = GetCurrentTimeTicks();
    data.contents_->StartPrerendering();
  }
  while (prerender_list_.size() > max_elements_) {
    data = prerender_list_.front();
    prerender_list_.pop_front();
    data.contents_->set_final_status(FINAL_STATUS_EVICTED);
    delete data.contents_;
  }
  StartSchedulingPeriodicCleanups();
  return true;
}

void PrerenderManager::AddPendingPreload(
    const std::pair<int,int>& child_route_id_pair,
    const GURL& url,
    const std::vector<GURL>& alias_urls,
    const GURL& referrer) {
  // Check if this is coming from a valid prerender rvh.
  bool is_valid_prerender = false;
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end(); ++it) {
    PrerenderContents* pc = it->contents_;

    int child_id;
    int route_id;
    bool has_child_id = pc->GetChildId(&child_id);
    bool has_route_id = has_child_id && pc->GetRouteId(&route_id);

    if (has_child_id && has_route_id &&
        child_id == child_route_id_pair.first &&
        route_id == child_route_id_pair.second) {
      is_valid_prerender = true;
      break;
    }
  }

  // If not, we could check to see if the RenderViewHost specified by the
  // child_route_id_pair exists and if so just start prerendering, as this
  // suggests that the link was clicked, though this might prerender something
  // that the user has already navigated away from. For now, we'll be
  // conservative and skip the prerender which will mean some prerender requests
  // from prerendered pages will be missed if the user navigates quickly.
  if (!is_valid_prerender) {
    RecordFinalStatus(FINAL_STATUS_PENDING_SKIPPED);
    return;
  }

  PendingPrerenderList::iterator it =
      pending_prerender_list_.find(child_route_id_pair);
  if (it == pending_prerender_list_.end()) {
    PendingPrerenderList::value_type el = std::make_pair(child_route_id_pair,
                                            std::vector<PendingContentsData>());
    it = pending_prerender_list_.insert(el).first;
  }

  it->second.push_back(PendingContentsData(url, alias_urls, referrer));
}

void PrerenderManager::DeleteOldEntries() {
  while (!prerender_list_.empty()) {
    PrerenderContentsData data = prerender_list_.front();
    if (IsPrerenderElementFresh(data.start_time_))
      return;
    prerender_list_.pop_front();
    data.contents_->set_final_status(FINAL_STATUS_TIMED_OUT);
    delete data.contents_;
  }
  if (prerender_list_.empty())
    StopSchedulingPeriodicCleanups();
}

PrerenderContents* PrerenderManager::GetEntry(const GURL& url) {
  DeleteOldEntries();
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    PrerenderContents* pc = it->contents_;
    if (pc->MatchesURL(url)) {
      prerender_list_.erase(it);
      return pc;
    }
  }
  // Entry not found.
  return NULL;
}

bool PrerenderManager::MaybeUsePreloadedPage(TabContents* tc, const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<PrerenderContents> pc(GetEntry(url));
  if (pc.get() == NULL)
    return false;

  // If we are just in the control group (which can be detected by noticing
  // that prerendering hasn't even started yet), record that this TC now would
  // be showing a prerendered contents, but otherwise, don't do anything.
  if (!pc->prerendering_has_started()) {
    MarkTabContentsAsWouldBePrerendered(tc);
    return false;
  }

  if (!pc->load_start_time().is_null())
    RecordTimeUntilUsed(GetCurrentTimeTicks() - pc->load_start_time());

  UMA_HISTOGRAM_COUNTS("Prerender.PrerendersPerSessionCount",
                       ++prerenders_per_session_count_);
  pc->set_final_status(FINAL_STATUS_USED);

  int child_id;
  int route_id;
  CHECK(pc->GetChildId(&child_id));
  CHECK(pc->GetRouteId(&route_id));

  RenderViewHost* rvh = pc->render_view_host();
  // RenderViewHosts in PrerenderContents start out hidden.
  // Since we are actually using it now, restore it.
  rvh->WasRestored();
  pc->set_render_view_host(NULL);
  rvh->Send(new ViewMsg_DisplayPrerenderedPage(rvh->routing_id()));
  tc->SwapInRenderViewHost(rvh);
  MarkTabContentsAsPrerendered(tc);

  // See if we have any pending prerender requests for this routing id and start
  // the preload if we do.
  std::pair<int, int> child_route_pair = std::make_pair(child_id, route_id);
  PendingPrerenderList::iterator pending_it =
      pending_prerender_list_.find(child_route_pair);
  if (pending_it != pending_prerender_list_.end()) {
    for (std::vector<PendingContentsData>::iterator content_it =
            pending_it->second.begin();
         content_it != pending_it->second.end(); ++content_it) {
      AddPreload(content_it->url_, content_it->alias_urls_,
                 content_it->referrer_);
    }
    pending_prerender_list_.erase(pending_it);
  }

  NotificationService::current()->Notify(
      NotificationType::PRERENDER_CONTENTS_USED,
      Source<std::pair<int, int> >(&child_route_pair),
      NotificationService::NoDetails());

  ViewHostMsg_FrameNavigate_Params* p = pc->navigate_params();
  if (p != NULL)
    tc->DidNavigate(rvh, *p);

  string16 title = pc->title();
  if (!title.empty())
    tc->UpdateTitle(rvh, pc->page_id(), UTF16ToWideHack(title));

  GURL icon_url = pc->icon_url();
  if (!icon_url.is_empty()) {
    std::vector<FaviconURL> urls;
    urls.push_back(FaviconURL(icon_url, FaviconURL::FAVICON));
    tc->favicon_helper().OnUpdateFaviconURL(pc->page_id(), urls);
  }

  if (pc->has_stopped_loading())
    tc->DidStopLoading();

  return true;
}

void PrerenderManager::RemoveEntry(PrerenderContents* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->contents_ == entry) {
      RemovePendingPreload(entry);
      prerender_list_.erase(it);
      break;
    }
  }
  DeleteOldEntries();
}

base::Time PrerenderManager::GetCurrentTime() const {
  return base::Time::Now();
}

base::TimeTicks PrerenderManager::GetCurrentTimeTicks() const {
  return base::TimeTicks::Now();
}

bool PrerenderManager::IsPrerenderElementFresh(const base::Time start) const {
  base::Time now = GetCurrentTime();
  return (now - start < max_prerender_age_);
}

PrerenderContents* PrerenderManager::CreatePrerenderContents(
    const GURL& url,
    const std::vector<GURL>& alias_urls,
    const GURL& referrer) {
  return prerender_contents_factory_->CreatePrerenderContents(
      this, profile_, url, alias_urls, referrer);
}

// Helper macro for histograms.
#define RECORD_PLT(tag, perceived_page_load_time) { \
    UMA_HISTOGRAM_CUSTOM_TIMES( \
        base::FieldTrial::MakeName(std::string("Prerender.") + tag, \
                                   "Prefetch"), \
        perceived_page_load_time, \
        base::TimeDelta::FromMilliseconds(10), \
        base::TimeDelta::FromSeconds(60), \
        100); \
  }

// static
void PrerenderManager::RecordPerceivedPageLoadTime(
    base::TimeDelta perceived_page_load_time,
    TabContents* tab_contents) {
  bool within_window = WithinWindow();
  PrerenderManager* prerender_manager =
      tab_contents->profile()->GetPrerenderManager();
  if (!prerender_manager)
    return;
  if (!prerender_manager->is_enabled())
    return;
  RECORD_PLT("PerceivedPLT", perceived_page_load_time);
  if (within_window)
    RECORD_PLT("PerceivedPLTWindowed", perceived_page_load_time);
  if (prerender_manager &&
      ((mode_ == PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP &&
        prerender_manager->WouldTabContentsBePrerendered(tab_contents)) ||
       (mode_ == PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP &&
        prerender_manager->IsTabContentsPrerendered(tab_contents)))) {
    RECORD_PLT("PerceivedPLTMatched", perceived_page_load_time);
  } else {
    if (within_window)
      RECORD_PLT("PerceivedPLTWindowNotMatched", perceived_page_load_time);
  }
}

void PrerenderManager::RecordTimeUntilUsed(base::TimeDelta time_until_used) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Prerender.TimeUntilUsed",
      time_until_used,
      base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromSeconds(kDefaultMaxPrerenderAgeSeconds),
      50);
}

bool PrerenderManager::is_enabled() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return enabled_;
}

void PrerenderManager::set_enabled(bool enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  enabled_ = enabled;
}

PrerenderContents* PrerenderManager::FindEntry(const GURL& url) {
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->contents_->MatchesURL(url))
      return it->contents_;
  }
  // Entry not found.
  return NULL;
}

PrerenderManager::PendingContentsData*
    PrerenderManager::FindPendingEntry(const GURL& url) {
  for (PendingPrerenderList::iterator map_it = pending_prerender_list_.begin();
       map_it != pending_prerender_list_.end();
       ++map_it) {
    for (std::vector<PendingContentsData>::iterator content_it =
            map_it->second.begin();
         content_it != map_it->second.end();
         ++content_it) {
      if (content_it->url_ == url) {
        return &(*content_it);
      }
    }
  }

  return NULL;
}

// static
void PrerenderManager::RecordPrefetchTagObserved() {
  // Ensure that we are in the UI thread, and post to the UI thread if
  // necessary.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableFunction(
            &PrerenderManager::RecordPrefetchTagObservedOnUIThread));
  } else {
    RecordPrefetchTagObservedOnUIThread();
  }
}

// static
void PrerenderManager::RecordPrefetchTagObservedOnUIThread() {
  // Once we get here, we have to be on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If we observe multiple tags within the 30 second window, we will still
  // reset the window to begin at the most recent occurrence, so that we will
  // always be in a window in the 30 seconds from each occurrence.
  last_prefetch_seen_time_ = base::TimeTicks::Now();
}

void PrerenderManager::RemovePendingPreload(PrerenderContents* entry) {
  int child_id;
  int route_id;
  bool has_child_id = entry->GetChildId(&child_id);
  bool has_route_id = has_child_id && entry->GetRouteId(&route_id);

  // If the entry doesn't have a RenderViewHost then it didn't start
  // prerendering and there shouldn't be any pending preloads to remove.
  if (has_child_id && has_route_id) {
    std::pair<int, int> child_route_pair = std::make_pair(child_id, route_id);
    pending_prerender_list_.erase(child_route_pair);
  }
}

// static
bool PrerenderManager::WithinWindow() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (last_prefetch_seen_time_.is_null())
    return false;
  base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - last_prefetch_seen_time_;
  return elapsed_time <= base::TimeDelta::FromSeconds(kWindowDurationSeconds);
}

bool PrerenderManager::DoesRateLimitAllowPrerender() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::TimeDelta elapsed_time =
      GetCurrentTimeTicks() - last_prerender_start_time_;
  UMA_HISTOGRAM_TIMES("Prerender.TimeBetweenPrerenderRequests",
                      elapsed_time);
  if (!rate_limit_enabled_)
    return true;
  return elapsed_time >
      base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs);
}

void PrerenderManager::StartSchedulingPeriodicCleanups() {
  if (repeating_timer_.IsRunning())
    return;
  repeating_timer_.Start(
      base::TimeDelta::FromMilliseconds(kPeriodicCleanupIntervalMs),
      this,
      &PrerenderManager::PeriodicCleanup);
}

void PrerenderManager::StopSchedulingPeriodicCleanups() {
  repeating_timer_.Stop();
}

void PrerenderManager::PeriodicCleanup() {
  DeleteOldEntries();
  // Grab a copy of the current PrerenderContents pointers, so that we
  // will not interfere with potential deletions of the list.
  std::vector<PrerenderContents*> prerender_contents;
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    prerender_contents.push_back(it->contents_);
  }
  for (std::vector<PrerenderContents*>::iterator it =
           prerender_contents.begin();
       it != prerender_contents.end();
       ++it) {
    (*it)->DestroyWhenUsingTooManyResources();
  }
}

void PrerenderManager::MarkTabContentsAsPrerendered(TabContents* tc) {
  prerendered_tc_set_.insert(tc);
}

void PrerenderManager::MarkTabContentsAsWouldBePrerendered(TabContents* tc) {
  would_be_prerendered_tc_set_.insert(tc);
}

void PrerenderManager::MarkTabContentsAsNotPrerendered(TabContents* tc) {
  prerendered_tc_set_.erase(tc);
  would_be_prerendered_tc_set_.erase(tc);
}

bool PrerenderManager::IsTabContentsPrerendered(TabContents* tc) const {
  return prerendered_tc_set_.count(tc) > 0;
}

bool PrerenderManager::WouldTabContentsBePrerendered(TabContents* tc) const {
  return would_be_prerendered_tc_set_.count(tc) > 0;
}

}  // namespace prerender
