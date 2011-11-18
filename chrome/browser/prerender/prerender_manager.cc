// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/prerender/prerender_condition.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_history.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "content/browser/cancelable_request.h"
#include "content/browser/debugger/render_view_devtools_agent_host.h"
#include "content/browser/in_process_webkit/session_storage_namespace.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/render_view_host_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;

namespace prerender {

namespace {

// Time window for which we will record windowed PLT's from the last
// observed link rel=prefetch tag.
const int kWindowDurationSeconds = 30;

// Time interval at which periodic cleanups are performed.
const int kPeriodicCleanupIntervalMs = 1000;

// Time interval before a new prerender is allowed.
const int kMinTimeBetweenPrerendersMs = 500;

// Time window for which we record old navigations, in milliseconds.
const int kNavigationRecordWindowMs = 5000;

// Valid HTTP methods for prerendering.
const char* const kValidHttpMethods[] = {
  "GET",
  "HEAD",
  "OPTIONS",
  "POST",
  "TRACE",
};

// Length of prerender history, for display in chrome://net-internals
const int kHistoryLength = 100;

// Indicates whether a Prerender has been cancelled such that we need
// a dummy replacement for the purpose of recording the correct PPLT for
// the Match Complete case.

bool NeedMatchCompleteDummyForFinalStatus(FinalStatus final_status) {
  return final_status != FINAL_STATUS_USED &&
      final_status != FINAL_STATUS_TIMED_OUT &&
      final_status != FINAL_STATUS_EVICTED &&
      final_status != FINAL_STATUS_MANAGER_SHUTDOWN &&
      final_status != FINAL_STATUS_APP_TERMINATING &&
      final_status != FINAL_STATUS_RENDERER_CRASHED &&
      final_status != FINAL_STATUS_WINDOW_OPENER &&
      final_status != FINAL_STATUS_FRAGMENT_MISMATCH &&
      final_status != FINAL_STATUS_CACHE_OR_HISTORY_CLEARED &&
      final_status != FINAL_STATUS_CANCELLED &&
      final_status != FINAL_STATUS_MATCH_COMPLETE_DUMMY;
}

}  // namespace

class PrerenderManager::OnCloseTabContentsDeleter
    : public TabContentsDelegate,
      public base::SupportsWeakPtr<
          PrerenderManager::OnCloseTabContentsDeleter> {
 public:
  OnCloseTabContentsDeleter(PrerenderManager* manager,
                            TabContentsWrapper* tab)
      : manager_(manager),
        tab_(tab) {
    tab_->tab_contents()->set_delegate(this);
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        base::Bind(&OnCloseTabContentsDeleter::ScheduleTabContentsForDeletion,
                   this->AsWeakPtr(), true), kDeleteWithExtremePrejudiceTimeMs);
  }

  virtual void CloseContents(TabContents* source) OVERRIDE {
    DCHECK_EQ(tab_->tab_contents(), source);
    ScheduleTabContentsForDeletion(false);
  }

  virtual void SwappedOut(TabContents* source) OVERRIDE {
    DCHECK_EQ(tab_->tab_contents(), source);
    ScheduleTabContentsForDeletion(false);
  }

  virtual bool ShouldSuppressDialogs() OVERRIDE {
    return true;
  }

 private:
  static const int kDeleteWithExtremePrejudiceTimeMs = 3000;

  void ScheduleTabContentsForDeletion(bool timeout) {
    tab_->tab_contents()->set_delegate(NULL);
    manager_->ScheduleDeleteOldTabContents(tab_.release(), this);
    UMA_HISTOGRAM_BOOLEAN("Prerender.TabContentsDeleterTimeout", timeout);
  }

  PrerenderManager* manager_;
  scoped_ptr<TabContentsWrapper> tab_;

  DISALLOW_COPY_AND_ASSIGN(OnCloseTabContentsDeleter);
};

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
  return GetMode() == PRERENDER_MODE_ENABLED ||
         GetMode() == PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP ||
         GetMode() == PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP ||
         GetMode() == PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP;
}

// static
bool PrerenderManager::ActuallyPrerendering() {
  return IsPrerenderingPossible() && !IsControlGroup();
}

// static
bool PrerenderManager::IsControlGroup() {
  return GetMode() == PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP;
}

// static
bool PrerenderManager::IsNoUseGroup() {
  return GetMode() == PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP;
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
    CHECK(contents);
  }
};

struct PrerenderManager::NavigationRecord {
  GURL url_;
  base::TimeTicks time_;
  NavigationRecord(const GURL& url, base::TimeTicks time)
      : url_(url),
        time_(time) {
  }
};

class PrerenderManager::MostVisitedSites
    : public content::NotificationObserver {
 public:
  explicit MostVisitedSites(Profile* profile) :
      profile_(profile) {
    history::TopSites* top_sites = GetTopSites();
    if (top_sites) {
      registrar_.Add(this, chrome::NOTIFICATION_TOP_SITES_CHANGED,
                     content::Source<history::TopSites>(top_sites));
    }

    UpdateMostVisited();
  }

  void UpdateMostVisited() {
    history::TopSites* top_sites = GetTopSites();
    if (top_sites) {
      top_sites->GetMostVisitedURLs(
          &topsites_consumer_,
          base::Bind(&prerender::PrerenderManager::MostVisitedSites::
              OnMostVisitedURLsAvailable, base::Unretained(this)));
    }
  }

  void OnMostVisitedURLsAvailable(const history::MostVisitedURLList& data) {
    urls_.clear();
    for (int i = 0; i < static_cast<int>(data.size()); i++)
      urls_.insert(data[i].url);
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) {
    DCHECK_EQ(type, chrome::NOTIFICATION_TOP_SITES_CHANGED);
    UpdateMostVisited();
  }

  bool IsTopSite(const GURL& url) const {
    return (urls_.count(url) > 0);
  }

 private:
  history::TopSites* GetTopSites() const {
    if (profile_)
      return profile_->GetTopSites();
    return NULL;
  }

  CancelableRequestConsumer topsites_consumer_;
  Profile* profile_;
  content::NotificationRegistrar registrar_;
  std::set<GURL> urls_;
};

bool PrerenderManager::IsTopSite(const GURL& url) {
  if (!most_visited_.get())
    most_visited_.reset(new MostVisitedSites(profile_));
  return most_visited_->IsTopSite(url);
}

bool PrerenderManager::IsPendingEntry(const GURL& url) const {
  DCHECK(CalledOnValidThread());
  for (std::list<PrerenderContentsData>::const_iterator it =
           prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->contents_->IsPendingEntry(url))
      return true;
  }
  return false;
}

PrerenderManager::PrerenderManager(Profile* profile,
                                   PrerenderTracker* prerender_tracker)
    : enabled_(true),
      profile_(profile),
      prerender_tracker_(prerender_tracker),
      prerender_contents_factory_(PrerenderContents::CreateFactory()),
      last_prerender_start_time_(GetCurrentTimeTicks() -
          base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs)),
      weak_factory_(this),
      prerender_history_(new PrerenderHistory(kHistoryLength)),
      histograms_(new PrerenderHistograms()) {
  // There are some assumptions that the PrerenderManager is on the UI thread.
  // Any other checks simply make sure that the PrerenderManager is accessed on
  // the same thread that it was created on.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

PrerenderManager::~PrerenderManager() {
}

void PrerenderManager::Shutdown() {
  DoShutdown();
}

void PrerenderManager::SetPrerenderContentsFactory(
    PrerenderContents::Factory* prerender_contents_factory) {
  DCHECK(CalledOnValidThread());
  prerender_contents_factory_.reset(prerender_contents_factory);
}

bool PrerenderManager::AddPrerenderFromLinkRelPrerender(int process_id,
                                                        int route_id,
                                                        const GURL& url,
                                                        const GURL& referrer) {
  std::pair<int, int> child_route_id_pair = std::make_pair(process_id,
                                                           route_id);

  return AddPrerender(ORIGIN_LINK_REL_PRERENDER, child_route_id_pair,
                      url, referrer, NULL);
}

bool PrerenderManager::AddPrerenderFromOmnibox(
    const GURL& url,
    SessionStorageNamespace* session_storage_namespace) {
  DCHECK(session_storage_namespace);
  if (!IsOmniboxEnabled(profile_))
    return false;

  Origin origin = ORIGIN_MAX;
  switch (GetOmniboxHeuristicToUse()) {
    case OMNIBOX_HEURISTIC_ORIGINAL:
      origin = ORIGIN_OMNIBOX_ORIGINAL;
      break;

    case OMNIBOX_HEURISTIC_CONSERVATIVE:
      origin = ORIGIN_OMNIBOX_CONSERVATIVE;
      break;

    case OMNIBOX_HEURISTIC_EXACT:
      origin = ORIGIN_OMNIBOX_EXACT;
      break;

    default:
      NOTREACHED();
      break;
  };

  return AddPrerender(origin, std::make_pair(-1, -1), url, GURL(),
                      session_storage_namespace);
}

bool PrerenderManager::AddPrerender(
    Origin origin,
    const std::pair<int, int>& child_route_id_pair,
    const GURL& url_arg,
    const GURL& referrer,
    SessionStorageNamespace* session_storage_namespace) {
  DCHECK(CalledOnValidThread());

  if (origin == ORIGIN_LINK_REL_PRERENDER && IsGoogleSearchResultURL(referrer))
    origin = ORIGIN_GWS_PRERENDER;

  histograms_->RecordPrerender(origin, url_arg);

  // If the referring page is prerendering, defer the prerender.
  std::list<PrerenderContentsData>::iterator source_prerender =
      FindPrerenderContentsForChildRouteIdPair(child_route_id_pair);
  if (source_prerender != prerender_list_.end()) {
    source_prerender->contents_->AddPendingPrerender(
        origin, url_arg, referrer);
    return true;
  }

  DeleteOldEntries();
  DeletePendingDeleteEntries();

  GURL url = url_arg;
  GURL alias_url;
  if (IsControlGroup() && MaybeGetQueryStringBasedAliasURL(url, &alias_url))
    url = alias_url;

  if (FindEntry(url))
    return false;

  uint8 experiment = GetQueryStringBasedExperiment(url_arg);

  // Do not prerender if there are too many render processes, and we would
  // have to use an existing one.  We do not want prerendering to happen in
  // a shared process, so that we can always reliably lower the CPU
  // priority for prerendering.
  // In single-process mode, ShouldTryToUseExistingProcessHost() always returns
  // true, so that case needs to be explicitly checked for.
  // TODO(tburkard): Figure out how to cancel prerendering in the opposite
  // case, when a new tab is added to a process used for prerendering.
  if (content::RenderProcessHost::ShouldTryToUseExistingProcessHost() &&
      !content::RenderProcessHost::run_renderer_in_process()) {
    RecordFinalStatus(origin, experiment, FINAL_STATUS_TOO_MANY_PROCESSES);
    return false;
  }

  // Check if enough time has passed since the last prerender.
  if (!DoesRateLimitAllowPrerender()) {
    // Cancel the prerender. We could add it to the pending prerender list but
    // this doesn't make sense as the next prerender request will be triggered
    // by a navigation and is unlikely to be the same site.
    RecordFinalStatus(origin, experiment, FINAL_STATUS_RATE_LIMIT_EXCEEDED);
    return false;
  }

  RenderViewHost* source_render_view_host = NULL;
  if (child_route_id_pair.first != -1) {
    source_render_view_host =
        RenderViewHost::FromID(child_route_id_pair.first,
                               child_route_id_pair.second);
    // Don't prerender page if parent RenderViewHost no longer exists, or it has
    // no view.  The latter should only happen when the RenderView has closed.
    if (!source_render_view_host || !source_render_view_host->view()) {
      RecordFinalStatus(origin, experiment,
                        FINAL_STATUS_SOURCE_RENDER_VIEW_CLOSED);
      return false;
    }
  }

  if (!session_storage_namespace && source_render_view_host) {
    session_storage_namespace =
        source_render_view_host->session_storage_namespace();
  }

  PrerenderContents* prerender_contents = CreatePrerenderContents(
      url, referrer, origin, experiment);
  if (!prerender_contents || !prerender_contents->Init())
    return false;

  // TODO(cbentzel): Move invalid checks here instead of PrerenderContents?
  PrerenderContentsData data(prerender_contents, GetCurrentTime());

  prerender_list_.push_back(data);

  if (IsControlGroup()) {
    data.contents_->set_final_status(FINAL_STATUS_CONTROL_GROUP);
  } else {
    last_prerender_start_time_ = GetCurrentTimeTicks();
    data.contents_->StartPrerendering(source_render_view_host,
                                      session_storage_namespace);
  }
  while (prerender_list_.size() > config_.max_elements) {
    data = prerender_list_.front();
    prerender_list_.pop_front();
    data.contents_->Destroy(FINAL_STATUS_EVICTED);
  }
  StartSchedulingPeriodicCleanups();
  return true;
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

void PrerenderManager::DestroyPrerenderForRenderView(
    int process_id, int view_id, FinalStatus final_status) {
  DCHECK(CalledOnValidThread());
  std::list<PrerenderContentsData>::iterator it =
      FindPrerenderContentsForChildRouteIdPair(
          std::make_pair(process_id, view_id));
  if (it != prerender_list_.end()) {
    PrerenderContents* prerender_contents = it->contents_;
    prerender_contents->Destroy(final_status);
  }
}

void PrerenderManager::CancelAllPrerenders() {
  DCHECK(CalledOnValidThread());
  while (!prerender_list_.empty()) {
    PrerenderContentsData data = prerender_list_.front();
    DCHECK(data.contents_);
    data.contents_->Destroy(FINAL_STATUS_CANCELLED);
  }
}

void PrerenderManager::DeleteOldEntries() {
  DCHECK(CalledOnValidThread());
  while (!prerender_list_.empty()) {
    PrerenderContentsData data = prerender_list_.front();
    if (IsPrerenderElementFresh(data.start_time_))
      return;
    data.contents_->Destroy(FINAL_STATUS_TIMED_OUT);
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
    if (prerender_contents->MatchesURL(url, NULL)) {
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

bool PrerenderManager::MaybeUsePrerenderedPage(TabContents* tab_contents,
                                               const GURL& url,
                                               const GURL& opener_url) {
  DCHECK(CalledOnValidThread());
  RecordNavigation(url);

  scoped_ptr<PrerenderContents> prerender_contents(
      GetEntryButNotSpecifiedTC(url, tab_contents));
  if (prerender_contents.get() == NULL)
    return false;

  // Do not use the prerendered version if the opener url corresponding to the
  // window.opener property has the same origin as the url.
  // NOTE: This is broken in the cases where the document domain is modified
  // using the javascript property for "document.domain".
  if (opener_url.GetOrigin() == url.GetOrigin()) {
    prerender_contents.release()->Destroy(FINAL_STATUS_WINDOW_OPENER);
    return false;
  }

  // Even if we match, the location.hash might be different. Record this as a
  // separate final status.
  GURL matching_url;
  bool url_matches = prerender_contents->MatchesURL(url, &matching_url);
  DCHECK(url_matches);
  if (url_matches && url.ref() != matching_url.ref()) {
    prerender_contents.release()->Destroy(FINAL_STATUS_FRAGMENT_MISMATCH);
    return false;
  }

  // If we are just in the control group (which can be detected by noticing
  // that prerendering hasn't even started yet), record that |tab_contents| now
  // would be showing a prerendered contents, but otherwise, don't do anything.
  if (!prerender_contents->prerendering_has_started()) {
    MarkTabContentsAsWouldBePrerendered(tab_contents);
    return false;
  }

  if (prerender_contents->starting_page_id() <=
      tab_contents->GetMaxPageID()) {
    prerender_contents.release()->Destroy(FINAL_STATUS_PAGE_ID_CONFLICT);
    return false;
  }

  // Don't use prerendered pages if debugger is attached to the tab.
  // See http://crbug.com/98541
  if (RenderViewDevToolsAgentHost::IsDebuggerAttached(tab_contents)) {
    prerender_contents.release()->Destroy(FINAL_STATUS_DEVTOOLS_ATTACHED);
    return false;
  }

  // If the prerendered page is in the middle of a cross-site navigation,
  // don't swap it in because there isn't a good way to merge histories.
  if (prerender_contents->IsCrossSiteNavigationPending()) {
    prerender_contents.release()->Destroy(
        FINAL_STATUS_CROSS_SITE_NAVIGATION_PENDING);
    return false;
  }

  // If the session storage namespaces don't match, cancel the prerender.
  RenderViewHost* old_render_view_host = tab_contents->render_view_host();
  RenderViewHost* new_render_view_host =
      prerender_contents->prerender_contents()->render_view_host();
  DCHECK(old_render_view_host);
  DCHECK(new_render_view_host);
  if (old_render_view_host->session_storage_namespace() !=
      new_render_view_host->session_storage_namespace()) {
    prerender_contents.release()->Destroy(
        FINAL_STATUS_SESSION_STORAGE_NAMESPACE_MISMATCH);
    return false;
  }

  // If we don't want to use prerenders at all, we are done.
  // For bookkeeping purposes, we need to mark this TabContents to
  // reflect that it would have been prerendered.
  if (GetMode() == PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP) {
    MarkTabContentsAsWouldBePrerendered(tab_contents);
    prerender_contents.release()->Destroy(FINAL_STATUS_NO_USE_GROUP);
    return false;
  }

  int child_id, route_id;
  CHECK(prerender_contents->GetChildId(&child_id));
  CHECK(prerender_contents->GetRouteId(&route_id));

  // Try to set the prerendered page as used, so any subsequent attempts to
  // cancel on other threads will fail.  If this fails because the prerender
  // was already cancelled, possibly on another thread, fail.
  if (!prerender_tracker_->TryUse(child_id, route_id))
    return false;

  if (!prerender_contents->load_start_time().is_null()) {
    histograms_->RecordTimeUntilUsed(GetCurrentTimeTicks() -
                                     prerender_contents->load_start_time(),
                                     config_.max_age);
  }

  histograms_->RecordPerSessionCount(++prerenders_per_session_count_);
  prerender_contents->set_final_status(FINAL_STATUS_USED);

  new_render_view_host->Send(
      new ChromeViewMsg_SetIsPrerendering(new_render_view_host->routing_id(),
                                          false));

  TabContentsWrapper* new_tab_contents =
      prerender_contents->ReleasePrerenderContents();
  TabContentsWrapper* old_tab_contents =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  DCHECK(new_tab_contents);
  DCHECK(old_tab_contents);

  MarkTabContentsAsPrerendered(new_tab_contents->tab_contents());

  // Merge the browsing history.
  new_tab_contents->controller().CopyStateFromAndPrune(
      &old_tab_contents->controller());
  old_tab_contents->delegate()->SwapTabContents(old_tab_contents,
                                                new_tab_contents);
  prerender_contents->CommitHistory(new_tab_contents);

  GURL icon_url = prerender_contents->icon_url();
  if (!icon_url.is_empty()) {
    std::vector<FaviconURL> urls;
    urls.push_back(FaviconURL(icon_url, FaviconURL::FAVICON));
    new_tab_contents->favicon_tab_helper()->OnUpdateFaviconURL(
        prerender_contents->page_id(),
        urls);
  }

  // Update PPLT metrics:
  // If the tab has finished loading, record a PPLT of 0.
  // If the tab is still loading, reset its start time to the current time.
  PrerenderTabHelper* prerender_tab_helper =
      new_tab_contents->prerender_tab_helper();
  DCHECK(prerender_tab_helper != NULL);
  prerender_tab_helper->PrerenderSwappedIn();

  // Start pending prerender requests from the PrerenderContents, if there are
  // any.
  prerender_contents->StartPendingPrerenders();

  if (old_tab_contents->tab_contents()->NeedToFireBeforeUnload()) {
    // Schedule the delete to occur after the tab has run its unload handlers.
    on_close_tab_contents_deleters_.push_back(
        new OnCloseTabContentsDeleter(this, old_tab_contents));
    old_tab_contents->render_view_host()->FirePageBeforeUnload(false);
  } else {
    // No unload handler to run, so delete asap.
    ScheduleDeleteOldTabContents(old_tab_contents, NULL);
  }

  // TODO(cbentzel): Should prerender_contents move to the pending delete
  //                 list, instead of deleting directly here?
  AddToHistory(prerender_contents.get());
  return true;
}

void PrerenderManager::MoveEntryToPendingDelete(PrerenderContents* entry,
                                                FinalStatus final_status) {
  DCHECK(CalledOnValidThread());
  DCHECK(entry);
  DCHECK(!IsPendingDelete(entry));

  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->contents_ == entry) {
      bool swapped_in_dummy_replacement = false;

      // If this PrerenderContents is being deleted due to a cancellation,
      // we need to create a dummy replacement for PPLT accounting purposes
      // for the Match Complete group.
      // This is the case if the cancellation is for any reason that would not
      // occur in the control group case.
      if (NeedMatchCompleteDummyForFinalStatus(final_status)) {
        // TODO(tburkard): I'd like to DCHECK that we are actually prerendering.
        // However, what if new conditions are added and
        // NeedMatchCompleteDummyForFinalStatus, is not being updated.  Not sure
        // what's the best thing to do here.  For now, I will just check whether
        // we are actually prerendering.
        if (ActuallyPrerendering()) {
          PrerenderContents* dummy_replacement_prerender_contents =
              CreatePrerenderContents(
                  entry->prerender_url(),
                  entry->referrer(),
                  entry->origin(),
                  entry->experiment_id());
          if (dummy_replacement_prerender_contents &&
              dummy_replacement_prerender_contents->Init()) {
            dummy_replacement_prerender_contents->
                AddAliasURLsFromOtherPrerenderContents(entry);
            it->contents_ = dummy_replacement_prerender_contents;
            it->contents_->set_final_status(FINAL_STATUS_MATCH_COMPLETE_DUMMY);
            swapped_in_dummy_replacement = true;
          }
        }
      }
      if (!swapped_in_dummy_replacement)
        prerender_list_.erase(it);
      break;
    }
  }
  AddToHistory(entry);
  pending_delete_list_.push_back(entry);

  // Destroy the old TabContents relatively promptly to reduce resource usage,
  // and in the case of HTML5 media, reduce the change of playing any sound.
  PostCleanupTask();
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
  return (now - start < config_.max_age);
}

PrerenderContents* PrerenderManager::CreatePrerenderContents(
    const GURL& url,
    const GURL& referrer,
    Origin origin,
    uint8 experiment_id) {
  DCHECK(CalledOnValidThread());
  return prerender_contents_factory_->CreatePrerenderContents(
      this, prerender_tracker_, profile_, url,
      referrer, origin, experiment_id);
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

void PrerenderManager::DeletePendingDeleteEntries() {
  while (!pending_delete_list_.empty()) {
    PrerenderContents* contents = pending_delete_list_.front();
    pending_delete_list_.pop_front();
    delete contents;
  }
}

// static
void PrerenderManager::RecordPerceivedPageLoadTime(
    base::TimeDelta perceived_page_load_time,
    TabContents* tab_contents,
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForProfile(
          Profile::FromBrowserContext(tab_contents->browser_context()));
  if (!prerender_manager)
    return;
  if (!prerender_manager->is_enabled())
    return;
  bool was_prerender =
      prerender_manager->IsTabContentsPrerendered(tab_contents);
  bool was_complete_prerender = was_prerender ||
      prerender_manager->WouldTabContentsBePrerendered(tab_contents);
  prerender_manager->histograms_->RecordPerceivedPageLoadTime(
      perceived_page_load_time, was_prerender, was_complete_prerender, url);
}

bool PrerenderManager::is_enabled() const {
  DCHECK(CalledOnValidThread());
  if (!enabled_)
    return false;
  for (std::list<const PrerenderCondition*>::const_iterator it =
           prerender_conditions_.begin();
       it != prerender_conditions_.end();
       ++it) {
    const PrerenderCondition* condition = *it;
    if (!condition->CanPrerender())
      return false;
  }
  return true;
}

void PrerenderManager::set_enabled(bool enabled) {
  DCHECK(CalledOnValidThread());
  enabled_ = enabled;
}

void PrerenderManager::AddCondition(const PrerenderCondition* condition) {
  prerender_conditions_.push_back(condition);
}

PrerenderContents* PrerenderManager::FindEntry(const GURL& url) {
  DCHECK(CalledOnValidThread());
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->contents_->MatchesURL(url, NULL))
      return it->contents_;
  }
  // Entry not found.
  return NULL;
}

void PrerenderManager::DoShutdown() {
  DestroyAllContents(FINAL_STATUS_MANAGER_SHUTDOWN);
  STLDeleteElements(&prerender_conditions_);
  profile_ = NULL;
}

bool PrerenderManager::DoesRateLimitAllowPrerender() const {
  DCHECK(CalledOnValidThread());
  base::TimeDelta elapsed_time =
      GetCurrentTimeTicks() - last_prerender_start_time_;
  histograms_->RecordTimeBetweenPrerenderRequests(elapsed_time);
  if (!config_.rate_limit_enabled)
    return true;
  return elapsed_time >
      base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs);
}

void PrerenderManager::StartSchedulingPeriodicCleanups() {
  DCHECK(CalledOnValidThread());
  if (repeating_timer_.IsRunning())
    return;
  repeating_timer_.Start(FROM_HERE,
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

bool PrerenderManager::IsOldRenderViewHost(
    const RenderViewHost* render_view_host) const {
  for (std::list<TabContentsWrapper*>::const_iterator it =
           old_tab_contents_list_.begin();
       it != old_tab_contents_list_.end();
       ++it) {
    if ((*it)->tab_contents()->render_view_host() == render_view_host)
      return true;
  }
  return false;
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
    DCHECK(it->contents_);
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
      base::Bind(&PrerenderManager::PeriodicCleanup,
                 weak_factory_.GetWeakPtr()));
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

bool PrerenderManager::HasRecentlyBeenNavigatedTo(const GURL& url) {
  DCHECK(CalledOnValidThread());

  CleanUpOldNavigations();
  for (std::list<NavigationRecord>::const_iterator it = navigations_.begin();
       it != navigations_.end();
       ++it) {
    if (it->url_ == url)
      return true;
  }

  return false;
}

void PrerenderManager::CleanUpOldNavigations() {
  DCHECK(CalledOnValidThread());

  // Cutoff.  Navigations before this cutoff can be discarded.
  base::TimeTicks cutoff = GetCurrentTimeTicks() -
      base::TimeDelta::FromMilliseconds(kNavigationRecordWindowMs);
  while (!navigations_.empty()) {
    if (navigations_.front().time_ > cutoff)
      break;
    navigations_.pop_front();
  }
}

void PrerenderManager::ScheduleDeleteOldTabContents(
    TabContentsWrapper* tab,
    OnCloseTabContentsDeleter* deleter) {
  old_tab_contents_list_.push_back(tab);
  PostCleanupTask();

  if (deleter) {
    ScopedVector<OnCloseTabContentsDeleter>::iterator i = std::find(
        on_close_tab_contents_deleters_.begin(),
        on_close_tab_contents_deleters_.end(),
        deleter);
    DCHECK(i != on_close_tab_contents_deleters_.end());
    on_close_tab_contents_deleters_.erase(i);
  }
}

DictionaryValue* PrerenderManager::GetAsValue() const {
  DCHECK(CalledOnValidThread());
  DictionaryValue* dict_value = new DictionaryValue();
  dict_value->Set("history", prerender_history_->GetEntriesAsValue());
  dict_value->Set("active", GetActivePrerendersAsValue());
  dict_value->SetBoolean("enabled", enabled_);
  dict_value->SetBoolean("omnibox_enabled", IsOmniboxEnabled(profile_));
  if (IsOmniboxEnabled(profile_)) {
    switch (GetOmniboxHeuristicToUse()) {
      case OMNIBOX_HEURISTIC_ORIGINAL:
        dict_value->SetString("omnibox_heuristic", "(original)");
        break;
      case OMNIBOX_HEURISTIC_CONSERVATIVE:
        dict_value->SetString("omnibox_heuristic", "(conservative)");
        break;
      case OMNIBOX_HEURISTIC_EXACT:
        dict_value->SetString("omnibox_heuristic", "(exact)");
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  // If prerender is disabled via a flag this method is not even called.
  if (IsControlGroup())
    dict_value->SetString("disabled_reason", "(Disabled for testing)");
  if (IsNoUseGroup())
    dict_value->SetString("disabled_reason", "(Not using prerendered pages)");
  return dict_value;
}

void PrerenderManager::ClearData(int clear_flags) {
  DCHECK_GE(clear_flags, 0);
  DCHECK_LT(clear_flags, CLEAR_MAX);
  if (clear_flags & CLEAR_PRERENDER_CONTENTS)
    DestroyAllContents(FINAL_STATUS_CACHE_OR_HISTORY_CLEARED);
  // This has to be second, since destroying prerenders can add to the history.
  if (clear_flags & CLEAR_PRERENDER_HISTORY)
    prerender_history_->Clear();
}

void PrerenderManager::AddToHistory(PrerenderContents* contents) {
  PrerenderHistory::Entry entry(contents->prerender_url(),
                                contents->final_status(),
                                contents->origin(),
                                base::Time::Now());
  prerender_history_->AddEntry(entry);
}

void PrerenderManager::RecordNavigation(const GURL& url) {
  DCHECK(CalledOnValidThread());

  navigations_.push_back(NavigationRecord(url, GetCurrentTimeTicks()));
  CleanUpOldNavigations();
}

Value* PrerenderManager::GetActivePrerendersAsValue() const {
  ListValue* list_value = new ListValue();
  for (std::list<PrerenderContentsData>::const_iterator it =
           prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    Value* prerender_value = it->contents_->GetAsValue();
    if (!prerender_value)
      continue;
    list_value->Append(prerender_value);
  }
  return list_value;
}

void PrerenderManager::DestroyAllContents(FinalStatus final_status) {
  DeleteOldTabContents();
  while (!prerender_list_.empty()) {
    PrerenderContentsData data = prerender_list_.front();
    prerender_list_.pop_front();
    data.contents_->Destroy(final_status);
  }
  DeletePendingDeleteEntries();
}

void PrerenderManager::RecordFinalStatus(Origin origin,
                                         uint8 experiment_id,
                                         FinalStatus final_status) const {
  histograms_->RecordFinalStatus(origin, experiment_id, final_status);
}

PrerenderManager* FindPrerenderManagerUsingRenderProcessId(
    int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_id);
  // Each render process is guaranteed to only hold RenderViews owned by the
  // same BrowserContext. This is enforced by
  // RenderProcessHost::GetExistingProcessHost.
  if (!render_process_host || !render_process_host->GetBrowserContext())
    return NULL;
  Profile* profile = Profile::FromBrowserContext(
      render_process_host->GetBrowserContext());
  if (!profile)
    return NULL;
  return PrerenderManagerFactory::GetInstance()->GetForProfile(profile);
}

}  // namespace prerender
