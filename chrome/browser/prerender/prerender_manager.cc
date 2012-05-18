// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include <set>
#include <string>
#include <vector>

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
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/prerender/prerender_condition.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_history.h"
#include "chrome/browser/prerender/prerender_local_predictor.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/prerender_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::SessionStorageNamespace;
using content::WebContents;

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
// Traditionally, "Match" means that a prerendered page was actually visited &
// the prerender was used.  Our goal is to have "Match" cases line up in the
// control group & the experiment group, so that we can make meaningful
// comparisons of improvements.  However, in the control group, since we don't
// actually perform prerenders, many of the cancellation reasons cannot be
// detected.  Therefore, in the Prerender group, when we cancel for one of these
// reasons, we keep track of a dummy Prerender representing what we would
// have in the control group.  If that dummy prerender in the prerender group
// would then be swapped in (but isn't actually b/c it's a dummy), we record
// this as a MatchComplete.  This allows us to compare MatchComplete's
// across Prerender & Control group which ideally should be lining up.
// This ensures that there is no bias in terms of the page load times
// of the pages forming the difference between the two sets.

bool NeedMatchCompleteDummyForFinalStatus(FinalStatus final_status) {
  return final_status != FINAL_STATUS_USED &&
      final_status != FINAL_STATUS_TIMED_OUT &&
      final_status != FINAL_STATUS_EVICTED &&
      final_status != FINAL_STATUS_MANAGER_SHUTDOWN &&
      final_status != FINAL_STATUS_APP_TERMINATING &&
      final_status != FINAL_STATUS_WINDOW_OPENER &&
      final_status != FINAL_STATUS_FRAGMENT_MISMATCH &&
      final_status != FINAL_STATUS_CACHE_OR_HISTORY_CLEARED &&
      final_status != FINAL_STATUS_CANCELLED &&
      final_status != FINAL_STATUS_SESSION_STORAGE_NAMESPACE_MISMATCH &&
      final_status != FINAL_STATUS_DEVTOOLS_ATTACHED &&
      final_status != FINAL_STATUS_CROSS_SITE_NAVIGATION_PENDING;
}

}  // namespace

class PrerenderManager::OnCloseTabContentsDeleter
    : public content::WebContentsDelegate,
      public base::SupportsWeakPtr<
          PrerenderManager::OnCloseTabContentsDeleter> {
 public:
  OnCloseTabContentsDeleter(PrerenderManager* manager,
                            TabContentsWrapper* tab)
      : manager_(manager),
        tab_(tab) {
    tab_->web_contents()->SetDelegate(this);
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        base::Bind(&OnCloseTabContentsDeleter::ScheduleTabContentsForDeletion,
                   this->AsWeakPtr(), true),
        base::TimeDelta::FromSeconds(kDeleteWithExtremePrejudiceSeconds));
  }

  virtual void CloseContents(WebContents* source) OVERRIDE {
    DCHECK_EQ(tab_->web_contents(), source);
    ScheduleTabContentsForDeletion(false);
  }

  virtual void SwappedOut(WebContents* source) OVERRIDE {
    DCHECK_EQ(tab_->web_contents(), source);
    ScheduleTabContentsForDeletion(false);
  }

  virtual bool ShouldSuppressDialogs() OVERRIDE {
    return true;
  }

 private:
  static const int kDeleteWithExtremePrejudiceSeconds = 3;

  void ScheduleTabContentsForDeletion(bool timeout) {
    tab_->web_contents()->SetDelegate(NULL);
    manager_->ScheduleDeleteOldTabContents(tab_.release(), this);
    UMA_HISTOGRAM_BOOLEAN("Prerender.TabContentsDeleterTimeout", timeout);
  }

  PrerenderManager* manager_;
  scoped_ptr<TabContentsWrapper> tab_;

  DISALLOW_COPY_AND_ASSIGN(OnCloseTabContentsDeleter);
};

// static
bool PrerenderManager::is_prefetch_enabled_ = false;

// static
int PrerenderManager::prerenders_per_session_count_ = 0;

// static
PrerenderManager::PrerenderManagerMode PrerenderManager::mode_ =
    PRERENDER_MODE_ENABLED;

struct PrerenderManager::PrerenderContentsData {
  PrerenderContents* contents_;
  base::Time start_time_;
  int active_count_;
  PrerenderContentsData(PrerenderContents* contents, base::Time start_time)
      : contents_(contents),
        start_time_(start_time),
        active_count_(1) {
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

bool PrerenderManager::AddPrerenderFromLinkRelPrerender(
    int process_id,
    int route_id,
    const GURL& url,
    const content::Referrer& referrer,
    const gfx::Size& size) {
#if defined(OS_ANDROID)
  // TODO(jcivelli): http://crbug.com/113322 We should have an option to disable
  //                link-prerender and enable omnibox-prerender only.
  return false;
#else
  std::pair<int, int> child_route_id_pair(process_id, route_id);
  PrerenderContentsDataList::iterator it =
      FindPrerenderContentsForChildRouteIdPair(child_route_id_pair);
  if (it != prerender_list_.end()) {
    // Instead of prerendering from inside of a running prerender, we will defer
    // this request until its launcher is made visible.
    it->contents_->AddPendingPrerender(url, referrer, size);
    return true;
  }

  // Unit tests pass in a process_id == -1.
  RenderViewHost* source_render_view_host = NULL;
  SessionStorageNamespace* session_storage_namespace = NULL;
  if (process_id != -1) {
    source_render_view_host =
        RenderViewHost::FromID(process_id, route_id);
    if (!source_render_view_host || !source_render_view_host->GetView())
      return false;
    session_storage_namespace =
        source_render_view_host->GetSessionStorageNamespace();
  }

  return AddPrerender(ORIGIN_LINK_REL_PRERENDER,
                      process_id, url, referrer, size,
                      session_storage_namespace);
#endif
}

bool PrerenderManager::AddPrerenderFromOmnibox(
    const GURL& url,
    SessionStorageNamespace* session_storage_namespace) {
  if (!IsOmniboxEnabled(profile_))
    return false;
  return AddPrerender(ORIGIN_OMNIBOX, -1, url,
                      content::Referrer(), gfx::Size(),
                      session_storage_namespace);
}

void PrerenderManager::MaybeCancelPrerender(const GURL& url) {
  PrerenderContentsDataList::iterator it = FindPrerenderContentsForURL(url);
  if (it == prerender_list_.end())
    return;
  PrerenderContentsData& prerender_contents_data = *it;
  if (--prerender_contents_data.active_count_ == 0)
    prerender_contents_data.contents_->Destroy(FINAL_STATUS_CANCELLED);
}

void PrerenderManager::DestroyPrerenderForRenderView(
    int process_id, int view_id, FinalStatus final_status) {
  DCHECK(CalledOnValidThread());
  PrerenderContentsDataList::iterator it =
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

void PrerenderManager::CancelOmniboxPrerenders() {
  DCHECK(CalledOnValidThread());
  for (PrerenderContentsDataList::iterator it = prerender_list_.begin();
       it != prerender_list_.end(); ) {
    PrerenderContentsDataList::iterator cur = it++;
    if (cur->contents_->origin() == ORIGIN_OMNIBOX)
      cur->contents_->Destroy(FINAL_STATUS_CANCELLED);
  }
}

bool PrerenderManager::MaybeUsePrerenderedPage(WebContents* web_contents,
                                               const GURL& url) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsWebContentsPrerendering(web_contents));

  scoped_ptr<PrerenderContents> prerender_contents(
      GetEntryButNotSpecifiedWC(url, web_contents));
  if (prerender_contents.get() == NULL)
    return false;

  // Do not use the prerendered version if there is an opener object.
  if (web_contents->HasOpener()) {
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
  // that prerendering hasn't even started yet), record that |web_contents| now
  // would be showing a prerendered contents, but otherwise, don't do anything.
  if (!prerender_contents->prerendering_has_started()) {
    MarkWebContentsAsWouldBePrerendered(web_contents);
    prerender_contents.release()->Destroy(FINAL_STATUS_WOULD_HAVE_BEEN_USED);
    return false;
  }

  // Don't use prerendered pages if debugger is attached to the tab.
  // See http://crbug.com/98541
  if (content::DevToolsAgentHostRegistry::IsDebuggerAttached(web_contents)) {
    DestroyAndMarkMatchCompleteAsUsed(prerender_contents.release(),
                                      FINAL_STATUS_DEVTOOLS_ATTACHED);
    return false;
  }

  // If the prerendered page is in the middle of a cross-site navigation,
  // don't swap it in because there isn't a good way to merge histories.
  if (prerender_contents->IsCrossSiteNavigationPending()) {
    DestroyAndMarkMatchCompleteAsUsed(
        prerender_contents.release(),
        FINAL_STATUS_CROSS_SITE_NAVIGATION_PENDING);
    return false;
  }

  // If the session storage namespaces don't match, cancel the prerender.
  RenderViewHost* old_render_view_host = web_contents->GetRenderViewHost();
  RenderViewHost* new_render_view_host =
      prerender_contents->prerender_contents()->web_contents()->
          GetRenderViewHost();
  DCHECK(old_render_view_host);
  DCHECK(new_render_view_host);
  if (old_render_view_host->GetSessionStorageNamespace() !=
      new_render_view_host->GetSessionStorageNamespace()) {
    DestroyAndMarkMatchCompleteAsUsed(
        prerender_contents.release(),
        FINAL_STATUS_SESSION_STORAGE_NAMESPACE_MISMATCH);
    return false;
  }

  // If we don't want to use prerenders at all, we are done.
  // For bookkeeping purposes, we need to mark this WebContents to
  // reflect that it would have been prerendered.
  if (GetMode() == PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP) {
    MarkWebContentsAsWouldBePrerendered(web_contents);
    prerender_contents.release()->Destroy(FINAL_STATUS_WOULD_HAVE_BEEN_USED);
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
                                     GetMaxAge());
  }

  histograms_->RecordPerSessionCount(++prerenders_per_session_count_);
  histograms_->RecordUsedPrerender(prerender_contents->origin());
  prerender_contents->set_final_status(FINAL_STATUS_USED);

  new_render_view_host->Send(
      new PrerenderMsg_SetIsPrerendering(new_render_view_host->GetRoutingID(),
                                         false));

  TabContentsWrapper* new_tab_contents =
      prerender_contents->ReleasePrerenderContents();
  TabContentsWrapper* old_tab_contents =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents);
  DCHECK(new_tab_contents);
  DCHECK(old_tab_contents);

  MarkWebContentsAsPrerendered(new_tab_contents->web_contents());

  // Merge the browsing history.
  new_tab_contents->web_contents()->GetController().CopyStateFromAndPrune(
      &old_tab_contents->web_contents()->GetController());
  old_tab_contents->core_tab_helper()->delegate()->
      SwapTabContents(old_tab_contents, new_tab_contents);
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

  if (old_tab_contents->web_contents()->NeedToFireBeforeUnload()) {
    // Schedule the delete to occur after the tab has run its unload handlers.
    on_close_tab_contents_deleters_.push_back(
        new OnCloseTabContentsDeleter(this, old_tab_contents));
    old_tab_contents->web_contents()->GetRenderViewHost()->
        FirePageBeforeUnload(false);
  } else {
    // No unload handler to run, so delete asap.
    ScheduleDeleteOldTabContents(old_tab_contents, NULL);
  }

  // TODO(cbentzel): Should prerender_contents move to the pending delete
  //                 list, instead of deleting directly here?
  AddToHistory(prerender_contents.get());
  RecordNavigation(url);
  return true;
}

void PrerenderManager::MoveEntryToPendingDelete(PrerenderContents* entry,
                                                FinalStatus final_status) {
  DCHECK(CalledOnValidThread());
  DCHECK(entry);
  // Confirm this entry has not already been moved to the pending delete list.
  DCHECK_EQ(0, std::count(pending_delete_list_.begin(),
                          pending_delete_list_.end(), entry));

  for (PrerenderContentsDataList::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->contents_ == entry) {
      bool swapped_in_dummy_replacement = false;

      // If this PrerenderContents is being deleted due to a cancellation,
      // we need to create a dummy replacement for PPLT accounting purposes
      // for the Match Complete group.
      // This is the case if the cancellation is for any reason that would not
      // occur in the control group case.
      if (entry->match_complete_status() ==
          PrerenderContents::MATCH_COMPLETE_DEFAULT &&
          NeedMatchCompleteDummyForFinalStatus(final_status)) {
        // TODO(tburkard): I'd like to DCHECK that we are actually prerendering.
        // However, what if new conditions are added and
        // NeedMatchCompleteDummyForFinalStatus, is not being updated.  Not sure
        // what's the best thing to do here.  For now, I will just check whether
        // we are actually prerendering.
        if (ActuallyPrerendering()) {
          entry->set_match_complete_status(
              PrerenderContents::MATCH_COMPLETE_REPLACED);
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
            dummy_replacement_prerender_contents->set_match_complete_status(
                PrerenderContents::MATCH_COMPLETE_REPLACEMENT);
            it->contents_ = dummy_replacement_prerender_contents;
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

  // Destroy the old WebContents relatively promptly to reduce resource usage,
  // and in the case of HTML5 media, reduce the change of playing any sound.
  PostCleanupTask();
}

// static
void PrerenderManager::RecordPerceivedPageLoadTime(
    base::TimeDelta perceived_page_load_time,
    double fraction_plt_elapsed_at_swap_in,
    WebContents* web_contents,
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!prerender_manager)
    return;
  if (!prerender_manager->IsEnabled())
    return;
  bool was_prerender =
      prerender_manager->IsWebContentsPrerendered(web_contents);
  bool was_complete_prerender = was_prerender ||
      prerender_manager->WouldWebContentsBePrerendered(web_contents);
  if (prerender_manager->IsWebContentsPrerendering(web_contents)) {
    prerender_manager->histograms_->RecordPageLoadTimeNotSwappedIn(
        perceived_page_load_time, url);
  } else {
    prerender_manager->histograms_->RecordPerceivedPageLoadTime(
        perceived_page_load_time, was_prerender, was_complete_prerender, url);
    prerender_manager->histograms_->RecordPercentLoadDoneAtSwapin(
        fraction_plt_elapsed_at_swap_in);
  }
}

void PrerenderManager::set_enabled(bool enabled) {
  DCHECK(CalledOnValidThread());
  enabled_ = enabled;
}

// static
bool PrerenderManager::IsPrefetchEnabled() {
  return is_prefetch_enabled_;
}

// static
void PrerenderManager::SetIsPrefetchEnabled(bool value) {
  is_prefetch_enabled_ = value;
}

// static
PrerenderManager::PrerenderManagerMode PrerenderManager::GetMode() {
  return mode_;
}

// static
void PrerenderManager::SetMode(PrerenderManagerMode mode) {
  mode_ = mode;
}

// static
const char* PrerenderManager::GetModeString() {
  switch (mode_) {
    case PRERENDER_MODE_DISABLED:
      return "_Disabled";
    case PRERENDER_MODE_ENABLED:
    case PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP:
      return "_Enabled";
    case PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP:
      return "_Control";
    case PRERENDER_MODE_EXPERIMENT_5MIN_TTL_GROUP:
      return "_5MinTTL";
    case PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP:
      return "_NoUse";
    case PRERENDER_MODE_MAX:
      NOTREACHED() << "Invalid PrerenderManager mode.";
      break;
  };
  return "";
}

// static
bool PrerenderManager::IsPrerenderingPossible() {
  return GetMode() != PRERENDER_MODE_DISABLED;
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

bool PrerenderManager::IsWebContentsPrerendering(
    WebContents* web_contents) const {
  DCHECK(CalledOnValidThread());
  for (PrerenderContentsDataList::const_iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    TabContentsWrapper* prerender_tab_contents_wrapper =
        it->contents_->prerender_contents();
    if (prerender_tab_contents_wrapper &&
        prerender_tab_contents_wrapper->web_contents() == web_contents) {
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
        prerender_tab_contents_wrapper->web_contents() == web_contents)
      return true;
  }

  return false;
}

bool PrerenderManager::DidPrerenderFinishLoading(const GURL& url) const {
  DCHECK(CalledOnValidThread());
  PrerenderContents* contents = FindEntry(url);
  return contents ? contents->has_finished_loading() : false;
}

void PrerenderManager::MarkWebContentsAsPrerendered(WebContents* web_contents) {
  DCHECK(CalledOnValidThread());
  prerendered_tab_contents_set_.insert(web_contents);
}

void PrerenderManager::MarkWebContentsAsWouldBePrerendered(
    WebContents* web_contents) {
  DCHECK(CalledOnValidThread());
  would_be_prerendered_tab_contents_set_.insert(web_contents);
}

void PrerenderManager::MarkWebContentsAsNotPrerendered(
    WebContents* web_contents) {
  DCHECK(CalledOnValidThread());
  prerendered_tab_contents_set_.erase(web_contents);
  would_be_prerendered_tab_contents_set_.erase(web_contents);
}

bool PrerenderManager::IsWebContentsPrerendered(
    content::WebContents* web_contents) const {
  DCHECK(CalledOnValidThread());
  return prerendered_tab_contents_set_.count(web_contents) > 0;
}

bool PrerenderManager::WouldWebContentsBePrerendered(
    WebContents* web_contents) const {
  DCHECK(CalledOnValidThread());
  return would_be_prerendered_tab_contents_set_.count(web_contents) > 0;
}

bool PrerenderManager::HasRecentlyBeenNavigatedTo(const GURL& url) {
  DCHECK(CalledOnValidThread());

  CleanUpOldNavigations();
  std::list<NavigationRecord>::const_reverse_iterator end = navigations_.rend();
  for (std::list<NavigationRecord>::const_reverse_iterator it =
           navigations_.rbegin();
       it != end;
       ++it) {
    if (it->url_ == url) {
      base::TimeDelta delta = GetCurrentTimeTicks() - it->time_;
      histograms_->RecordTimeSinceLastRecentVisit(delta);
      return true;
    }
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

DictionaryValue* PrerenderManager::GetAsValue() const {
  DCHECK(CalledOnValidThread());
  DictionaryValue* dict_value = new DictionaryValue();
  dict_value->Set("history", prerender_history_->GetEntriesAsValue());
  dict_value->Set("active", GetActivePrerendersAsValue());
  dict_value->SetBoolean("enabled", enabled_);
  dict_value->SetBoolean("omnibox_enabled", IsOmniboxEnabled(profile_));
  // If prerender is disabled via a flag this method is not even called.
  std::string enabled_note;
  if (IsControlGroup())
    enabled_note += "(Control group: Not actually prerendering) ";
  if (IsNoUseGroup())
    enabled_note += "(No-use group: Not swapping in prerendered pages) ";
  if (GetMode() == PRERENDER_MODE_EXPERIMENT_5MIN_TTL_GROUP)
    enabled_note += "(5 min TTL group: Extended prerender eviction to 5 mins) ";
  dict_value->SetString("enabled_note", enabled_note);
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

void PrerenderManager::RecordFinalStatusWithMatchCompleteStatus(
    Origin origin,
    uint8 experiment_id,
    PrerenderContents::MatchCompleteStatus mc_status,
    FinalStatus final_status) const {
  histograms_->RecordFinalStatus(origin,
                                 experiment_id,
                                 mc_status,
                                 final_status);
}

void PrerenderManager::AddCondition(const PrerenderCondition* condition) {
  prerender_conditions_.push_back(condition);
}

bool PrerenderManager::IsTopSite(const GURL& url) {
  if (!most_visited_.get())
    most_visited_.reset(new MostVisitedSites(profile_));
  return most_visited_->IsTopSite(url);
}

bool PrerenderManager::IsPendingEntry(const GURL& url) const {
  DCHECK(CalledOnValidThread());
  for (PrerenderContentsDataList::const_iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->contents_->IsPendingEntry(url))
      return true;
  }
  return false;
}

bool PrerenderManager::IsPrerendering(const GURL& url) const {
  DCHECK(CalledOnValidThread());
  return (FindEntry(url) != NULL);
}

void PrerenderManager::RecordNavigation(const GURL& url) {
  DCHECK(CalledOnValidThread());

  navigations_.push_back(NavigationRecord(url, GetCurrentTimeTicks()));
  CleanUpOldNavigations();
}

// protected
void PrerenderManager::SetPrerenderContentsFactory(
    PrerenderContents::Factory* prerender_contents_factory) {
  DCHECK(CalledOnValidThread());
  prerender_contents_factory_.reset(prerender_contents_factory);
}

void PrerenderManager::DoShutdown() {
  DestroyAllContents(FINAL_STATUS_MANAGER_SHUTDOWN);
  STLDeleteElements(&prerender_conditions_);
  on_close_tab_contents_deleters_.reset();
  profile_ = NULL;
}

// private
bool PrerenderManager::AddPrerender(
    Origin origin,
    int process_id,
    const GURL& url_arg,
    const content::Referrer& referrer,
    const gfx::Size& size,
    SessionStorageNamespace* session_storage_namespace) {
  DCHECK(CalledOnValidThread());

  if (!IsEnabled())
    return false;

  if (origin == ORIGIN_LINK_REL_PRERENDER &&
      IsGoogleSearchResultURL(referrer.url)) {
    origin = ORIGIN_GWS_PRERENDER;
  }

  DeleteOldEntries();
  DeletePendingDeleteEntries();

  GURL url = url_arg;
  GURL alias_url;
  if (IsControlGroup() && MaybeGetQueryStringBasedAliasURL(url, &alias_url))
    url = alias_url;

  // From here on, we will record a FinalStatus so we need to register with the
  // histogram tracking.
  histograms_->RecordPrerender(origin, url_arg);

  uint8 experiment = GetQueryStringBasedExperiment(url_arg);

  if (PrerenderContentsData* prerender_contents_data = FindEntryData(url)) {
    ++prerender_contents_data->active_count_;
    RecordFinalStatus(origin, experiment, FINAL_STATUS_DUPLICATE);
    return true;
  }

  // Do not prerender if there are too many render processes, and we would
  // have to use an existing one.  We do not want prerendering to happen in
  // a shared process, so that we can always reliably lower the CPU
  // priority for prerendering.
  // In single-process mode, ShouldTryToUseExistingProcessHost() always returns
  // true, so that case needs to be explicitly checked for.
  // TODO(tburkard): Figure out how to cancel prerendering in the opposite
  // case, when a new tab is added to a process used for prerendering.
  // On Android we do reuse processes as we have a limited number of them and we
  // still want the benefits of prerendering even when several tabs are open.
#if !defined(OS_ANDROID)
  if (content::RenderProcessHost::ShouldTryToUseExistingProcessHost(
          profile_, url) &&
      !content::RenderProcessHost::run_renderer_in_process()) {
    RecordFinalStatus(origin, experiment, FINAL_STATUS_TOO_MANY_PROCESSES);
    return false;
  }
#endif

  // Check if enough time has passed since the last prerender.
  if (!DoesRateLimitAllowPrerender()) {
    // Cancel the prerender. We could add it to the pending prerender list but
    // this doesn't make sense as the next prerender request will be triggered
    // by a navigation and is unlikely to be the same site.
    RecordFinalStatus(origin, experiment, FINAL_STATUS_RATE_LIMIT_EXCEEDED);
    return false;
  }

  PrerenderContents* prerender_contents = CreatePrerenderContents(
      url, referrer, origin, experiment);
  if (!prerender_contents || !prerender_contents->Init())
    return false;

  histograms_->RecordPrerenderStarted(origin);

  // TODO(cbentzel): Move invalid checks here instead of PrerenderContents?
  PrerenderContentsData data(prerender_contents, GetCurrentTime());

  prerender_list_.push_back(data);

  last_prerender_start_time_ = GetCurrentTimeTicks();

  if (!IsControlGroup()) {
    data.contents_->StartPrerendering(process_id,
                                      size, session_storage_namespace);
  }
  while (prerender_list_.size() > config_.max_elements) {
    data = prerender_list_.front();
    prerender_list_.pop_front();
    data.contents_->Destroy(FINAL_STATUS_EVICTED);
  }
  StartSchedulingPeriodicCleanups();
  return true;
}

PrerenderContents* PrerenderManager::GetEntry(const GURL& url) {
  return GetEntryButNotSpecifiedWC(url, NULL);
}

PrerenderContents* PrerenderManager::GetEntryButNotSpecifiedWC(
    const GURL& url,
    WebContents* wc) {
  DCHECK(CalledOnValidThread());
  DeleteOldEntries();
  DeletePendingDeleteEntries();
  for (PrerenderContentsDataList::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    PrerenderContents* prerender_contents = it->contents_;
    if (prerender_contents->MatchesURL(url, NULL) &&
        !IsNoSwapInExperiment(prerender_contents->experiment_id())) {
      if (!prerender_contents->prerender_contents() ||
          !wc ||
          prerender_contents->prerender_contents()->web_contents() != wc) {
        prerender_list_.erase(it);
        return prerender_contents;
      }
    }
  }
  // Entry not found.
  return NULL;
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

void PrerenderManager::PeriodicCleanup() {
  DCHECK(CalledOnValidThread());
  DeleteOldTabContents();
  DeleteOldEntries();

  // Grab a copy of the current PrerenderContents pointers, so that we
  // will not interfere with potential deletions of the list.
  std::vector<PrerenderContents*> prerender_contents;
  for (PrerenderContentsDataList::iterator it = prerender_list_.begin();
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

base::TimeDelta PrerenderManager::GetMaxAge() const {
  return (GetMode() == PRERENDER_MODE_EXPERIMENT_5MIN_TTL_GROUP ?
      base::TimeDelta::FromSeconds(300) : config_.max_age);
}

bool PrerenderManager::IsPrerenderElementFresh(const base::Time start) const {
  DCHECK(CalledOnValidThread());
  base::Time now = GetCurrentTime();
  return (now - start < GetMaxAge());
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

base::Time PrerenderManager::GetCurrentTime() const {
  return base::Time::Now();
}

base::TimeTicks PrerenderManager::GetCurrentTimeTicks() const {
  return base::TimeTicks::Now();
}

PrerenderContents* PrerenderManager::CreatePrerenderContents(
    const GURL& url,
    const content::Referrer& referrer,
    Origin origin,
    uint8 experiment_id) {
  DCHECK(CalledOnValidThread());
  return prerender_contents_factory_->CreatePrerenderContents(
      this, prerender_tracker_, profile_, url,
      referrer, origin, experiment_id);
}

void PrerenderManager::DeletePendingDeleteEntries() {
  while (!pending_delete_list_.empty()) {
    PrerenderContents* contents = pending_delete_list_.front();
    pending_delete_list_.pop_front();
    delete contents;
  }
}

PrerenderManager::PrerenderContentsData* PrerenderManager::FindEntryData(
    const GURL& url) {
  DCHECK(CalledOnValidThread());
  PrerenderContentsDataList::iterator it = FindPrerenderContentsForURL(url);
  if (it == prerender_list_.end())
    return NULL;
  PrerenderContentsData& prerender_contents_data = *it;
  return &prerender_contents_data;
}

PrerenderContents* PrerenderManager::FindEntry(const GURL& url) const {
  DCHECK(CalledOnValidThread());
  for (PrerenderContentsDataList::const_iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->contents_->MatchesURL(url, NULL))
      return it->contents_;
  }
  // Entry not found.
  return NULL;
}

PrerenderManager::PrerenderContentsDataList::iterator
    PrerenderManager::FindPrerenderContentsForChildRouteIdPair(
        const std::pair<int, int>& child_route_id_pair) {
  PrerenderContentsDataList::iterator it = prerender_list_.begin();
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

PrerenderManager::PrerenderContentsDataList::iterator
    PrerenderManager::FindPrerenderContentsForURL(const GURL& url) {
  for (PrerenderContentsDataList::iterator it = prerender_list_.begin();
       it != prerender_list_.end(); ++it) {
    if (it->contents_->MatchesURL(url, NULL))
      return it;
  }
  return prerender_list_.end();
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

void PrerenderManager::DeleteOldTabContents() {
  while (!old_tab_contents_list_.empty()) {
    TabContentsWrapper* tab_contents = old_tab_contents_list_.front();
    old_tab_contents_list_.pop_front();
    // TODO(dominich): should we use Instant Unload Handler here?
    delete tab_contents;
  }
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

void PrerenderManager::AddToHistory(PrerenderContents* contents) {
  PrerenderHistory::Entry entry(contents->prerender_url(),
                                contents->final_status(),
                                contents->origin(),
                                base::Time::Now());
  prerender_history_->AddEntry(entry);
}

Value* PrerenderManager::GetActivePrerendersAsValue() const {
  ListValue* list_value = new ListValue();
  for (PrerenderContentsDataList::const_iterator it = prerender_list_.begin();
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

void PrerenderManager::DestroyAndMarkMatchCompleteAsUsed(
    PrerenderContents* prerender_contents,
    FinalStatus final_status) {
  prerender_contents->set_match_complete_status(
      PrerenderContents::MATCH_COMPLETE_REPLACED);
  histograms_->RecordFinalStatus(prerender_contents->origin(),
                                 prerender_contents->experiment_id(),
                                 PrerenderContents::MATCH_COMPLETE_REPLACEMENT,
                                 FINAL_STATUS_WOULD_HAVE_BEEN_USED);
  prerender_contents->Destroy(final_status);
}

void PrerenderManager::RecordFinalStatus(Origin origin,
                                         uint8 experiment_id,
                                         FinalStatus final_status) const {
  RecordFinalStatusWithMatchCompleteStatus(
      origin, experiment_id,
      PrerenderContents::MATCH_COMPLETE_DEFAULT,
      final_status);
}

bool PrerenderManager::IsEnabled() const {
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
