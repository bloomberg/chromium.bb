// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper_delegate.h"
#include "chrome/common/render_messages.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/render_view_host_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_util.h"

namespace prerender {

namespace {

// Default maximum permitted elements to prerender.
const unsigned int kDefaultMaxPrerenderElements = 1;

// Default maximum amount of private memory that may be used per
// PrerenderContents, in MB.
const unsigned int kDefaultMaxPrerenderMemoryMB = 100;

// Default maximum age a prerendered element may have, in seconds.
const int kDefaultMaxPrerenderAgeSeconds = 30;

// Time window for which we will record windowed PLT's from the last
// observed link rel=prefetch tag.
const int kWindowDurationSeconds = 30;

// Time interval at which periodic cleanups are performed.
const int kPeriodicCleanupIntervalMs = 1000;

// Time interval before a new prerender is allowed.
const int kMinTimeBetweenPrerendersMs = 500;

// Valid HTTP methods for prerendering.
const char* const kValidHttpMethods[] = {
  "OPTIONS",
  "GET",
  "HEAD",
  "TRACE",
};

}  // namespace

// static
int PrerenderManager::prerenders_per_session_count_ = 0;

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

// static
bool PrerenderManager::IsValidHttpMethod(const std::string& method) {
  // method has been canonicalized to upper case at this point so we can just
  // compare them.
  DCHECK_EQ(method, StringToUpperASCII(method));
  for (size_t i = 0; i < arraysize(kValidHttpMethods); ++i) {
    if (method.compare(kValidHttpMethods[i]) == 0)
      return true;
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
  PendingContentsData(const GURL& url,
                      const GURL& referrer)
      : url_(url), referrer_(referrer) { }
  ~PendingContentsData() {}
  GURL url_;
  GURL referrer_;
};

void HandlePrefetchTag(
    const base::WeakPtr<PrerenderManager>& prerender_manager_weak_ptr,
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const GURL& referrer,
    bool make_pending) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrerenderManager* prerender_manager = prerender_manager_weak_ptr.get();
  if (!prerender_manager || !prerender_manager->is_enabled())
    return;
  prerender_manager->RecordPrefetchTagObserved();

  std::pair<int, int> child_route_id_pair = std::make_pair(render_process_id,
                                                           render_view_id);
  // TODO(cbentzel): Should the decision to make pending be done on the
  //                 UI thread rather than the IO thread? The page may have
  //                 become activated at this point.
  if (make_pending)
    prerender_manager->AddPendingPreload(child_route_id_pair, url, referrer);
  else
    prerender_manager->AddPreload(child_route_id_pair, url, referrer);
}

void DestroyPreloadForRenderView(
    const base::WeakPtr<PrerenderManager>& prerender_manager_weak_ptr,
    int render_process_id,
    int render_view_id,
    FinalStatus final_status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrerenderManager* prerender_manager = prerender_manager_weak_ptr.get();
  if (!prerender_manager)
    return;

  prerender_manager->DestroyPreloadForChildRouteIdPair(
      std::make_pair(render_process_id, render_view_id),
      final_status);
}

PrerenderManager::PrerenderManager(Profile* profile)
    : rate_limit_enabled_(true),
      enabled_(true),
      profile_(profile),
      max_prerender_age_(base::TimeDelta::FromSeconds(
          kDefaultMaxPrerenderAgeSeconds)),
      max_prerender_memory_mb_(kDefaultMaxPrerenderMemoryMB),
      max_elements_(kDefaultMaxPrerenderElements),
      prerender_contents_factory_(PrerenderContents::CreateFactory()),
      last_prerender_start_time_(GetCurrentTimeTicks() -
          base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs)),
      runnable_method_factory_(this) {
  // There are some assumptions that the PrerenderManager is on the UI thread.
  // Any other checks simply make sure that the PrerenderManager is accessed on
  // the same thread that it was created on.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

PrerenderManager::~PrerenderManager() {
  DeleteOldTabContents();
  while (!prerender_list_.empty()) {
    PrerenderContentsData data = prerender_list_.front();
    prerender_list_.pop_front();
    data.contents_->Destroy(FINAL_STATUS_MANAGER_SHUTDOWN);
  }
  DeletePendingDeleteEntries();
}

void PrerenderManager::SetPrerenderContentsFactory(
    PrerenderContents::Factory* prerender_contents_factory) {
  DCHECK(CalledOnValidThread());
  prerender_contents_factory_.reset(prerender_contents_factory);
}

bool PrerenderManager::AddPreload(
    const std::pair<int, int>& child_route_id_pair,
    const GURL& url_arg,
    const GURL& referrer) {
  DCHECK(CalledOnValidThread());
  DeleteOldEntries();
  DeletePendingDeleteEntries();

  GURL url = url_arg;
  GURL alias_url;
  if (IsControlGroup() &&
      PrerenderManager::MaybeGetQueryStringBasedAliasURL(
          url, &alias_url)) {
    url = alias_url;
  }

  if (FindEntry(url))
    return false;

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

  RenderViewHost* source_render_view_host = NULL;
  // This test should fail only during unit tests.
  if (child_route_id_pair.first != -1) {
    source_render_view_host =
        RenderViewHost::FromID(child_route_id_pair.first,
                               child_route_id_pair.second);
    // Don't prerender page if parent RenderViewHost no longer exists, or it has
    // no view.  The latter should only happen when the RenderView has closed.
    if (!source_render_view_host || !source_render_view_host->view()) {
      RecordFinalStatus(FINAL_STATUS_SOURCE_RENDER_VIEW_CLOSED);
      return false;
    }
  }

  PrerenderContents* prerender_contents =
      CreatePrerenderContents(url, referrer);
  if (!prerender_contents || !prerender_contents->Init())
    return false;

  // TODO(cbentzel): Move invalid checks here instead of PrerenderContents?
  PrerenderContentsData data(prerender_contents, GetCurrentTime());

  prerender_list_.push_back(data);
  if (IsControlGroup()) {
    data.contents_->set_final_status(FINAL_STATUS_CONTROL_GROUP);
  } else {
    last_prerender_start_time_ = GetCurrentTimeTicks();
    data.contents_->StartPrerendering(source_render_view_host);
  }
  while (prerender_list_.size() > max_elements_) {
    data = prerender_list_.front();
    prerender_list_.pop_front();
    data.contents_->Destroy(FINAL_STATUS_EVICTED);
  }
  StartSchedulingPeriodicCleanups();
  return true;
}

void PrerenderManager::AddPendingPreload(
    const std::pair<int, int>& child_route_id_pair,
    const GURL& url,
    const GURL& referrer) {
  DCHECK(CalledOnValidThread());
  // Check if this is coming from a valid prerender RenderViewHost.
  bool is_valid_prerender =
      (FindPrerenderContentsForChildRouteIdPair(child_route_id_pair) !=
       prerender_list_.end());

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

  it->second.push_back(PendingContentsData(url, referrer));
}

std::list<PrerenderManager::PrerenderContentsData>::iterator
    PrerenderManager::FindPrerenderContentsForChildRouteIdPair(
        const std::pair<int, int>& child_route_id_pair) {
  std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
  for (; it != prerender_list_.end(); ++it) {
    PrerenderContents* prerender_contents = it->contents_;

    int child_id;
    int route_id;
    bool has_child_id = prerender_contents->GetChildId(&child_id);
    bool has_route_id = has_child_id &&
                        prerender_contents->GetRouteId(&route_id);

    if (has_child_id && has_route_id &&
        child_id == child_route_id_pair.first &&
        route_id == child_route_id_pair.second) {
      break;
    }
  }
  return it;
}

void PrerenderManager::DestroyPreloadForChildRouteIdPair(
    const std::pair<int, int>& child_route_id_pair,
    FinalStatus final_status) {
  DCHECK(CalledOnValidThread());
  std::list<PrerenderContentsData>::iterator it =
      FindPrerenderContentsForChildRouteIdPair(child_route_id_pair);
  if (it != prerender_list_.end()) {
    PrerenderContents* prerender_contents = it->contents_;
    prerender_contents->Destroy(final_status);
  }
}

void PrerenderManager::DeleteOldEntries() {
  DCHECK(CalledOnValidThread());
  while (!prerender_list_.empty()) {
    PrerenderContentsData data = prerender_list_.front();
    if (IsPrerenderElementFresh(data.start_time_))
      return;
    prerender_list_.pop_front();
    data.contents_->set_final_status(FINAL_STATUS_TIMED_OUT);
    delete data.contents_;
  }
  MaybeStopSchedulingPeriodicCleanups();
}

PrerenderContents* PrerenderManager::GetEntryButNotSpecifiedTC(
    const GURL& url,
    TabContents *tc) {
  DCHECK(CalledOnValidThread());
  DeleteOldEntries();
  DeletePendingDeleteEntries();
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    PrerenderContents* prerender_contents = it->contents_;
    if (prerender_contents->MatchesURL(url)) {
      if (!prerender_contents->prerender_contents() ||
          !tc ||
          prerender_contents->prerender_contents()->tab_contents() != tc) {
        prerender_list_.erase(it);
        return prerender_contents;
      }
    }
  }
  // Entry not found.
  return NULL;
}

PrerenderContents* PrerenderManager::GetEntry(const GURL& url) {
  return GetEntryButNotSpecifiedTC(url, NULL);
}

bool PrerenderManager::MaybeUsePreloadedPageOld(TabContents* tab_contents,
                                                const GURL& url) {
  DCHECK(CalledOnValidThread());
  scoped_ptr<PrerenderContents> prerender_contents(GetEntry(url));
  if (prerender_contents.get() == NULL)
    return false;

  // If we are just in the control group (which can be detected by noticing
  // that prerendering hasn't even started yet), record that |tab_contents| now
  // would be showing a prerendered contents, but otherwise, don't do anything.
  if (!prerender_contents->prerendering_has_started()) {
    MarkTabContentsAsWouldBePrerendered(tab_contents);
    return false;
  }

  if (!prerender_contents->load_start_time().is_null())
    RecordTimeUntilUsed(GetCurrentTimeTicks() -
                        prerender_contents->load_start_time());

  UMA_HISTOGRAM_COUNTS("Prerender.PrerendersPerSessionCount",
                       ++prerenders_per_session_count_);
  prerender_contents->set_final_status(FINAL_STATUS_USED);

  int child_id;
  int route_id;
  CHECK(prerender_contents->GetChildId(&child_id));
  CHECK(prerender_contents->GetRouteId(&route_id));

  RenderViewHost* render_view_host =
      prerender_contents->render_view_host_mutable();
  prerender_contents->set_render_view_host(NULL);

  // RenderViewHosts in PrerenderContents start out hidden.
  // Since we are actually using it now, restore it.
  render_view_host->WasRestored();
  render_view_host->Send(
      new ViewMsg_SetIsPrerendering(render_view_host->routing_id(), false));
  tab_contents->SwapInRenderViewHost(render_view_host);
  MarkTabContentsAsPrerendered(tab_contents);

  // See if we have any pending prerender requests for this routing id and start
  // the preload if we do.
  std::pair<int, int> child_route_pair = std::make_pair(child_id, route_id);
  PendingPrerenderList::iterator pending_it =
      pending_prerender_list_.find(child_route_pair);
  if (pending_it != pending_prerender_list_.end()) {
    for (std::vector<PendingContentsData>::iterator content_it =
            pending_it->second.begin();
         content_it != pending_it->second.end(); ++content_it) {
      AddPreload(pending_it->first, content_it->url_, content_it->referrer_);
    }
    pending_prerender_list_.erase(pending_it);
  }

  RenderViewHostDelegate* render_view_host_delegate =
      static_cast<RenderViewHostDelegate*>(tab_contents);
  ViewHostMsg_FrameNavigate_Params* params =
      prerender_contents->navigate_params();
  if (params != NULL)
    render_view_host_delegate->DidNavigate(render_view_host, *params);

  string16 title = prerender_contents->title();
  if (!title.empty()) {
    render_view_host_delegate->UpdateTitle(render_view_host,
                                           prerender_contents->page_id(),
                                           UTF16ToWideHack(title));
  }

  GURL icon_url = prerender_contents->icon_url();
  if (!icon_url.is_empty()) {
    std::vector<FaviconURL> urls;
    urls.push_back(FaviconURL(icon_url, FaviconURL::FAVICON));
    // TODO(avi): move prerendering to use TCWs; remove this.
    TabContentsWrapper* wrapper =
        TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
    wrapper->favicon_tab_helper()->OnUpdateFaviconURL(
        prerender_contents->page_id(),
        urls);
  }

  if (prerender_contents->has_stopped_loading())
    render_view_host_delegate->DidStopLoading();

  return true;
}

bool PrerenderManager::MaybeUsePreloadedPage(TabContents* tab_contents,
                                             const GURL& url) {
  if (!PrerenderContents::UseTabContents()) {
    VLOG(1) << "Checking for prerender with LEGACY code";
    return PrerenderManager::MaybeUsePreloadedPageOld(tab_contents, url);
  }
  VLOG(1) << "Checking for prerender with NEW code";
  DCHECK(CalledOnValidThread());
  scoped_ptr<PrerenderContents> prerender_contents(
      GetEntryButNotSpecifiedTC(url, tab_contents));
  if (prerender_contents.get() == NULL)
    return false;

  // If we are just in the control group (which can be detected by noticing
  // that prerendering hasn't even started yet), record that |tab_contents| now
  // would be showing a prerendered contents, but otherwise, don't do anything.
  if (!prerender_contents->prerendering_has_started()) {
    MarkTabContentsAsWouldBePrerendered(tab_contents);
    return false;
  }

  if (!prerender_contents->load_start_time().is_null())
    RecordTimeUntilUsed(GetCurrentTimeTicks() -
                        prerender_contents->load_start_time());

  UMA_HISTOGRAM_COUNTS("Prerender.PrerendersPerSessionCount",
                       ++prerenders_per_session_count_);
  prerender_contents->set_final_status(FINAL_STATUS_USED);

  int child_id;
  int route_id;
  CHECK(prerender_contents->GetChildId(&child_id));
  CHECK(prerender_contents->GetRouteId(&route_id));

  RenderViewHost* render_view_host =
      prerender_contents->prerender_contents()->render_view_host();
  DCHECK(render_view_host);
  render_view_host->Send(
      new ViewMsg_SetIsPrerendering(render_view_host->routing_id(), false));

  TabContentsWrapper* new_tab_contents =
      prerender_contents->ReleasePrerenderContents();
  TabContentsWrapper* old_tab_contents =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  DCHECK(new_tab_contents);
  DCHECK(old_tab_contents);

  MarkTabContentsAsPrerendered(new_tab_contents->tab_contents());

  // Merge the browsing history.
  new_tab_contents->controller().CopyStateFromAndPrune(
      &old_tab_contents->controller(),
      false);
  old_tab_contents->delegate()->SwapTabContents(old_tab_contents,
                                                new_tab_contents);

  // See if we have any pending prerender requests for this routing id and start
  // the preload if we do.
  std::pair<int, int> child_route_pair = std::make_pair(child_id, route_id);
  PendingPrerenderList::iterator pending_it =
      pending_prerender_list_.find(child_route_pair);
  if (pending_it != pending_prerender_list_.end()) {
    for (std::vector<PendingContentsData>::iterator content_it =
            pending_it->second.begin();
         content_it != pending_it->second.end(); ++content_it) {
      AddPreload(pending_it->first, content_it->url_, content_it->referrer_);
    }
    pending_prerender_list_.erase(pending_it);
  }

  old_tab_contents_list_.push_back(old_tab_contents);
  // Destroy the old TabContents relatively promptly to reduce resource usage.
  PostCleanupTask();
  return true;
}

void PrerenderManager::MoveEntryToPendingDelete(PrerenderContents* entry) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsPendingDelete(entry));
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->contents_ == entry) {
      RemovePendingPreload(entry);
      prerender_list_.erase(it);
      break;
    }
  }
  pending_delete_list_.push_back(entry);

  // Destroy the old TabContents relatively promptly to reduce resource usage,
  // and in the case of HTML5 media, reduce the change of playing any sound.
  PostCleanupTask();
}

bool PrerenderManager::IsPendingDelete(PrerenderContents* entry) const {
  DCHECK(CalledOnValidThread());
  for (std::list<PrerenderContents*>::const_iterator it =
          pending_delete_list_.begin();
       it != pending_delete_list_.end();
       ++it) {
    if (*it == entry)
      return true;
  }

  return false;
}

base::Time PrerenderManager::GetCurrentTime() const {
  return base::Time::Now();
}

base::TimeTicks PrerenderManager::GetCurrentTimeTicks() const {
  return base::TimeTicks::Now();
}

bool PrerenderManager::IsPrerenderElementFresh(const base::Time start) const {
  DCHECK(CalledOnValidThread());
  base::Time now = GetCurrentTime();
  return (now - start < max_prerender_age_);
}

PrerenderContents* PrerenderManager::CreatePrerenderContents(
    const GURL& url,
    const GURL& referrer) {
  DCHECK(CalledOnValidThread());
  return prerender_contents_factory_->CreatePrerenderContents(
      this, profile_, url, referrer);
}

void PrerenderManager::DeletePendingDeleteEntries() {
  while (!pending_delete_list_.empty()) {
    PrerenderContents* contents = pending_delete_list_.front();
    pending_delete_list_.pop_front();
    delete contents;
  }
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrerenderManager* prerender_manager =
      tab_contents->profile()->GetPrerenderManager();
  if (!prerender_manager)
    return;
  if (!prerender_manager->is_enabled())
    return;
  bool within_window = prerender_manager->WithinWindow();
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
  DCHECK(CalledOnValidThread());
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Prerender.TimeUntilUsed",
      time_until_used,
      base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromSeconds(kDefaultMaxPrerenderAgeSeconds),
      50);
}

base::TimeDelta PrerenderManager::max_prerender_age() const {
  DCHECK(CalledOnValidThread());
  return max_prerender_age_;
}

void PrerenderManager::set_max_prerender_age(base::TimeDelta max_age) {
  DCHECK(CalledOnValidThread());
  max_prerender_age_ = max_age;
}

size_t PrerenderManager::max_prerender_memory_mb() const {
  DCHECK(CalledOnValidThread());
  return max_prerender_memory_mb_;
}

void PrerenderManager::set_max_prerender_memory_mb(size_t max_memory_mb) {
  DCHECK(CalledOnValidThread());
  max_prerender_memory_mb_ = max_memory_mb;
}

unsigned int PrerenderManager::max_elements() const {
  DCHECK(CalledOnValidThread());
  return max_elements_;
}

void PrerenderManager::set_max_elements(unsigned int max_elements) {
  DCHECK(CalledOnValidThread());
  max_elements_ = max_elements;
}

bool PrerenderManager::is_enabled() const {
  DCHECK(CalledOnValidThread());
  return enabled_;
}

void PrerenderManager::set_enabled(bool enabled) {
  DCHECK(CalledOnValidThread());
  enabled_ = enabled;
}

PrerenderContents* PrerenderManager::FindEntry(const GURL& url) {
  DCHECK(CalledOnValidThread());
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
  DCHECK(CalledOnValidThread());
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

void PrerenderManager::RecordPrefetchTagObserved() {
  DCHECK(CalledOnValidThread());

  // If we observe multiple tags within the 30 second window, we will still
  // reset the window to begin at the most recent occurrence, so that we will
  // always be in a window in the 30 seconds from each occurrence.
  last_prefetch_seen_time_ = base::TimeTicks::Now();
}

void PrerenderManager::RemovePendingPreload(PrerenderContents* entry) {
  DCHECK(CalledOnValidThread());
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

bool PrerenderManager::WithinWindow() const {
  DCHECK(CalledOnValidThread());
  if (last_prefetch_seen_time_.is_null())
    return false;
  base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - last_prefetch_seen_time_;
  return elapsed_time <= base::TimeDelta::FromSeconds(kWindowDurationSeconds);
}

bool PrerenderManager::DoesRateLimitAllowPrerender() const {
  DCHECK(CalledOnValidThread());
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
  DCHECK(CalledOnValidThread());
  if (repeating_timer_.IsRunning())
    return;
  repeating_timer_.Start(
      base::TimeDelta::FromMilliseconds(kPeriodicCleanupIntervalMs),
      this,
      &PrerenderManager::PeriodicCleanup);
}

void PrerenderManager::MaybeStopSchedulingPeriodicCleanups() {
  if (!prerender_list_.empty())
    return;

  DCHECK(CalledOnValidThread());
  repeating_timer_.Stop();
}

void PrerenderManager::DeleteOldTabContents() {
  while (!old_tab_contents_list_.empty()) {
    TabContentsWrapper* tab_contents = old_tab_contents_list_.front();
    old_tab_contents_list_.pop_front();
    // TODO(dominich): should we use Instant Unload Handler here?
    delete tab_contents;
  }
}

void PrerenderManager::PeriodicCleanup() {
  DCHECK(CalledOnValidThread());
  DeleteOldTabContents();
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

  DeletePendingDeleteEntries();
}

void PrerenderManager::PostCleanupTask() {
  DCHECK(CalledOnValidThread());
  MessageLoop::current()->PostTask(
      FROM_HERE,
      runnable_method_factory_.NewRunnableMethod(
          &PrerenderManager::PeriodicCleanup));
}

bool PrerenderManager::IsTabContentsPrerendering(
    TabContents* tab_contents) const {
  DCHECK(CalledOnValidThread());
  for (std::list<PrerenderContentsData>::const_iterator it =
           prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    TabContentsWrapper* prerender_tab_contents_wrapper =
        it->contents_->prerender_contents();
    if (prerender_tab_contents_wrapper &&
        prerender_tab_contents_wrapper->tab_contents() == tab_contents) {
      return true;
    }
  }

  // Also look through the pending-deletion list.
  for (std::list<PrerenderContents*>::const_iterator it =
           pending_delete_list_.begin();
       it != pending_delete_list_.end();
       ++it) {
    TabContentsWrapper* prerender_tab_contents_wrapper =
        (*it)->prerender_contents();
    if (prerender_tab_contents_wrapper &&
        prerender_tab_contents_wrapper->tab_contents() == tab_contents)
      return true;
  }

  return false;
}

void PrerenderManager::MarkTabContentsAsPrerendered(TabContents* tab_contents) {
  DCHECK(CalledOnValidThread());
  prerendered_tab_contents_set_.insert(tab_contents);
}

void PrerenderManager::MarkTabContentsAsWouldBePrerendered(
    TabContents* tab_contents) {
  DCHECK(CalledOnValidThread());
  would_be_prerendered_tab_contents_set_.insert(tab_contents);
}

void PrerenderManager::MarkTabContentsAsNotPrerendered(
    TabContents* tab_contents) {
  DCHECK(CalledOnValidThread());
  prerendered_tab_contents_set_.erase(tab_contents);
  would_be_prerendered_tab_contents_set_.erase(tab_contents);
}

bool PrerenderManager::IsTabContentsPrerendered(
    TabContents* tab_contents) const {
  DCHECK(CalledOnValidThread());
  return prerendered_tab_contents_set_.count(tab_contents) > 0;
}

bool PrerenderManager::WouldTabContentsBePrerendered(
    TabContents* tab_contents) const {
  DCHECK(CalledOnValidThread());
  return would_be_prerendered_tab_contents_set_.count(tab_contents) > 0;
}

}  // namespace prerender
