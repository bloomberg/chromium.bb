// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/prerender/prerender_condition.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_handle.h"
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
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/prerender_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/favicon_url.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::SessionStorageNamespace;
using content::WebContents;

namespace prerender {

namespace {

// Time interval at which periodic cleanups are performed.
const int kPeriodicCleanupIntervalMs = 1000;

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
      final_status != FINAL_STATUS_MANAGER_SHUTDOWN &&
      final_status != FINAL_STATUS_APP_TERMINATING &&
      final_status != FINAL_STATUS_WINDOW_OPENER &&
      final_status != FINAL_STATUS_CACHE_OR_HISTORY_CLEARED &&
      final_status != FINAL_STATUS_CANCELLED &&
      final_status != FINAL_STATUS_DEVTOOLS_ATTACHED &&
      final_status != FINAL_STATUS_CROSS_SITE_NAVIGATION_PENDING;
}

}  // namespace

class PrerenderManager::OnCloseWebContentsDeleter
    : public content::WebContentsDelegate,
      public base::SupportsWeakPtr<
          PrerenderManager::OnCloseWebContentsDeleter> {
 public:
  OnCloseWebContentsDeleter(PrerenderManager* manager,
                            WebContents* tab)
      : manager_(manager),
        tab_(tab) {
    tab_->SetDelegate(this);
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        base::Bind(&OnCloseWebContentsDeleter::ScheduleWebContentsForDeletion,
                   this->AsWeakPtr(), true),
        base::TimeDelta::FromSeconds(kDeleteWithExtremePrejudiceSeconds));
  }

  virtual void CloseContents(WebContents* source) OVERRIDE {
    DCHECK_EQ(tab_, source);
    ScheduleWebContentsForDeletion(false);
  }

  virtual void SwappedOut(WebContents* source) OVERRIDE {
    DCHECK_EQ(tab_, source);
    ScheduleWebContentsForDeletion(false);
  }

  virtual bool ShouldSuppressDialogs() OVERRIDE {
    return true;
  }

 private:
  static const int kDeleteWithExtremePrejudiceSeconds = 3;

  void ScheduleWebContentsForDeletion(bool timeout) {
    tab_->SetDelegate(NULL);
    manager_->ScheduleDeleteOldWebContents(tab_.release(), this);
    UMA_HISTOGRAM_BOOLEAN("Prerender.TabContentsDeleterTimeout", timeout);
  }

  PrerenderManager* manager_;
  scoped_ptr<WebContents> tab_;

  DISALLOW_COPY_AND_ASSIGN(OnCloseWebContentsDeleter);
};

// static
bool PrerenderManager::is_prefetch_enabled_ = false;

// static
int PrerenderManager::prerenders_per_session_count_ = 0;

// static
PrerenderManager::PrerenderManagerMode PrerenderManager::mode_ =
    PRERENDER_MODE_ENABLED;

struct PrerenderManager::NavigationRecord {
  NavigationRecord(const GURL& url, base::TimeTicks time)
      : url(url),
        time(time) {
  }

  GURL url;
  base::TimeTicks time;
};

PrerenderManager::PrerenderedWebContentsData::
PrerenderedWebContentsData(Origin origin) : origin(origin) {
}

PrerenderManager::WouldBePrerenderedWebContentsData::
WouldBePrerenderedWebContentsData(Origin origin)
    : origin(origin),
      state(WAITING_FOR_PROVISIONAL_LOAD) {
}

PrerenderManager::PrerenderManager(Profile* profile,
                                   PrerenderTracker* prerender_tracker)
    : enabled_(profile && profile->GetPrefs() &&
          profile->GetPrefs()->GetBoolean(prefs::kNetworkPredictionEnabled)),
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

  if (IsLocalPredictorEnabled())
    local_predictor_.reset(new PrerenderLocalPredictor(this));

  // Certain experiments override our default config_ values.
  switch (PrerenderManager::GetMode()) {
    case PrerenderManager::PRERENDER_MODE_EXPERIMENT_MULTI_PRERENDER_GROUP:
      config_.max_link_concurrency = 4;
      config_.max_link_concurrency_per_launcher = 2;
      break;
    case PrerenderManager::PRERENDER_MODE_EXPERIMENT_15MIN_TTL_GROUP:
      config_.time_to_live = base::TimeDelta::FromMinutes(15);
      break;
    default:
      break;
  }
}

PrerenderManager::~PrerenderManager() {
  // The earlier call to ProfileKeyedService::Shutdown() should have emptied
  // these vectors already.
  DCHECK(active_prerenders_.empty());
  DCHECK(to_delete_prerenders_.empty());
}

void PrerenderManager::Shutdown() {
  DestroyAllContents(FINAL_STATUS_MANAGER_SHUTDOWN);
  STLDeleteElements(&prerender_conditions_);
  on_close_web_contents_deleters_.clear();
  // Must happen before |profile_| is set to NULL as
  // |local_predictor_| accesses it.
  if (local_predictor_)
    local_predictor_->Shutdown();
  profile_ = NULL;

  DCHECK(active_prerenders_.empty());
}

PrerenderHandle* PrerenderManager::AddPrerenderFromLinkRelPrerender(
    int process_id,
    int route_id,
    const GURL& url,
    const content::Referrer& referrer,
    const gfx::Size& size) {
#if defined(OS_ANDROID)
  // TODO(jcivelli): http://crbug.com/113322 We should have an option to disable
  //                link-prerender and enable omnibox-prerender only.
  return NULL;
#else
  DCHECK(!size.IsEmpty());
  Origin origin = ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN;
  SessionStorageNamespace* session_storage_namespace = NULL;
  // Unit tests pass in a process_id == -1.
  if (process_id != -1) {
    RenderViewHost* source_render_view_host =
        RenderViewHost::FromID(process_id, route_id);
    if (!source_render_view_host)
      return NULL;
    WebContents* source_web_contents =
        WebContents::FromRenderViewHost(source_render_view_host);
    if (!source_web_contents)
      return NULL;
    if (source_web_contents->GetURL().host() == url.host())
      origin = ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN;
    // TODO(ajwong): This does not correctly handle storage for isolated apps.
    session_storage_namespace =
        source_web_contents->GetController()
            .GetDefaultSessionStorageNamespace();
  }

  // If the prerender request comes from a recently cancelled prerender that
  // |this| still owns, then abort the prerender.
  for (ScopedVector<PrerenderData>::iterator it = to_delete_prerenders_.begin();
       it != to_delete_prerenders_.end(); ++it) {
    PrerenderContents* prerender_contents = (*it)->contents();
    int contents_child_id;
    int contents_route_id;
    if (prerender_contents->GetChildId(&contents_child_id) &&
        prerender_contents->GetRouteId(&contents_route_id) &&
        contents_child_id == process_id && contents_route_id == route_id) {
      return NULL;
    }
  }

  if (PrerenderData* parent_prerender_data =
          FindPrerenderDataForChildAndRoute(process_id, route_id)) {
    // Instead of prerendering from inside of a running prerender, we will defer
    // this request until its launcher is made visible.
    if (PrerenderContents* contents = parent_prerender_data->contents()) {
      PrerenderHandle* prerender_handle =
          new PrerenderHandle(static_cast<PrerenderData*>(NULL));
      scoped_ptr<PrerenderContents::PendingPrerenderInfo>
          pending_prerender_info(new PrerenderContents::PendingPrerenderInfo(
              prerender_handle->weak_ptr_factory_.GetWeakPtr(),
              origin, url, referrer, size));

      contents->AddPendingPrerender(pending_prerender_info.Pass());
      return prerender_handle;
    }
  }

  return AddPrerender(origin, process_id, url, referrer, size,
                      session_storage_namespace);
#endif
}

PrerenderHandle* PrerenderManager::AddPrerenderFromOmnibox(
    const GURL& url,
    SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size) {
  if (!IsOmniboxEnabled(profile_))
    return NULL;
  return AddPrerender(ORIGIN_OMNIBOX, -1, url, content::Referrer(), size,
                      session_storage_namespace);
}

void PrerenderManager::DestroyPrerenderForRenderView(
    int process_id, int view_id, FinalStatus final_status) {
  DCHECK(CalledOnValidThread());
  if (PrerenderData* prerender_data =
          FindPrerenderDataForChildAndRoute(process_id, view_id)) {
    prerender_data->contents()->Destroy(final_status);
  }
}

void PrerenderManager::CancelAllPrerenders() {
  DCHECK(CalledOnValidThread());
  while (!active_prerenders_.empty()) {
    PrerenderContents* prerender_contents =
        active_prerenders_.front()->contents();
    prerender_contents->Destroy(FINAL_STATUS_CANCELLED);
  }
}

bool PrerenderManager::MaybeUsePrerenderedPage(WebContents* web_contents,
                                               const GURL& url) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsWebContentsPrerendering(web_contents, NULL));

  DeleteOldEntries();
  to_delete_prerenders_.clear();
  // TODO(ajwong): This doesn't handle isolated apps correctly.
  PrerenderData* prerender_data = FindPrerenderData(
          url,
          web_contents->GetController().GetDefaultSessionStorageNamespace());
  if (!prerender_data)
    return false;
  DCHECK(prerender_data->contents());
  if (IsNoSwapInExperiment(prerender_data->contents()->experiment_id()))
    return false;

  if (WebContents* new_web_contents =
      prerender_data->contents()->prerender_contents()) {
    if (web_contents == new_web_contents)
      return false;  // Do not swap in to ourself.
  }

  // Do not use the prerendered version if there is an opener object.
  if (web_contents->HasOpener()) {
    prerender_data->contents()->Destroy(FINAL_STATUS_WINDOW_OPENER);
    return false;
  }

  // If we are just in the control group (which can be detected by noticing
  // that prerendering hasn't even started yet), record that |web_contents| now
  // would be showing a prerendered contents, but otherwise, don't do anything.
  if (!prerender_data->contents()->prerendering_has_started()) {
    MarkWebContentsAsWouldBePrerendered(web_contents,
                                        prerender_data->contents()->origin());
    prerender_data->contents()->Destroy(FINAL_STATUS_WOULD_HAVE_BEEN_USED);
    return false;
  }

  // Don't use prerendered pages if debugger is attached to the tab.
  // See http://crbug.com/98541
  if (content::DevToolsAgentHost::IsDebuggerAttached(web_contents)) {
    DestroyAndMarkMatchCompleteAsUsed(prerender_data->contents(),
                                      FINAL_STATUS_DEVTOOLS_ATTACHED);
    return false;
  }

  // If the prerendered page is in the middle of a cross-site navigation,
  // don't swap it in because there isn't a good way to merge histories.
  if (prerender_data->contents()->IsCrossSiteNavigationPending()) {
    DestroyAndMarkMatchCompleteAsUsed(
        prerender_data->contents(),
        FINAL_STATUS_CROSS_SITE_NAVIGATION_PENDING);
    return false;
  }

  // For bookkeeping purposes, we need to mark this WebContents to
  // reflect that it would have been prerendered.
  if (GetMode() == PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP) {
    MarkWebContentsAsWouldBePrerendered(web_contents,
                                        prerender_data->contents()->origin());
    prerender_data->contents()->Destroy(FINAL_STATUS_WOULD_HAVE_BEEN_USED);
    return false;
  }

  int child_id, route_id;
  CHECK(prerender_data->contents()->GetChildId(&child_id));
  CHECK(prerender_data->contents()->GetRouteId(&route_id));

  // Try to set the prerendered page as used, so any subsequent attempts to
  // cancel on other threads will fail.  If this fails because the prerender
  // was already cancelled, possibly on another thread, fail.
  if (!prerender_tracker_->TryUse(child_id, route_id))
    return false;

  // At this point, we've determined that we will use the prerender.
  ScopedVector<PrerenderData>::iterator to_erase =
      FindIteratorForPrerenderContents(prerender_data->contents());
  DCHECK(active_prerenders_.end() != to_erase);
  DCHECK_EQ(prerender_data, *to_erase);
  scoped_ptr<PrerenderContents>
      prerender_contents(prerender_data->ReleaseContents());
  active_prerenders_.erase(to_erase);

  if (!prerender_contents->load_start_time().is_null()) {
    histograms_->RecordTimeUntilUsed(
        prerender_contents->origin(),
        GetCurrentTimeTicks() - prerender_contents->load_start_time());
  }

  histograms_->RecordPerSessionCount(prerender_contents->origin(),
                                     ++prerenders_per_session_count_);
  histograms_->RecordUsedPrerender(prerender_contents->origin());
  prerender_contents->SetFinalStatus(FINAL_STATUS_USED);

  RenderViewHost* new_render_view_host =
      prerender_contents->prerender_contents()->GetRenderViewHost();
  new_render_view_host->Send(
      new PrerenderMsg_SetIsPrerendering(new_render_view_host->GetRoutingID(),
                                         false));

  // Start pending prerender requests from the PrerenderContents, if there are
  // any.
  prerender_contents->PrepareForUse();

  WebContents* new_web_contents =
      prerender_contents->ReleasePrerenderContents();
  WebContents* old_web_contents = web_contents;
  DCHECK(new_web_contents);
  DCHECK(old_web_contents);

  MarkWebContentsAsPrerendered(new_web_contents, prerender_contents->origin());

  // Merge the browsing history.
  new_web_contents->GetController().CopyStateFromAndPrune(
      &old_web_contents->GetController());
  CoreTabHelper::FromWebContents(old_web_contents)->delegate()->
      SwapTabContents(old_web_contents, new_web_contents);
  prerender_contents->CommitHistory(new_web_contents);

  GURL icon_url = prerender_contents->icon_url();
  if (!icon_url.is_empty()) {
    std::vector<content::FaviconURL> urls;
    urls.push_back(content::FaviconURL(icon_url, content::FaviconURL::FAVICON));
    FaviconTabHelper::FromWebContents(new_web_contents)->
        DidUpdateFaviconURL(prerender_contents->page_id(), urls);
  }

  // Update PPLT metrics:
  // If the tab has finished loading, record a PPLT of 0.
  // If the tab is still loading, reset its start time to the current time.
  PrerenderTabHelper* prerender_tab_helper =
      PrerenderTabHelper::FromWebContents(new_web_contents);
  DCHECK(prerender_tab_helper != NULL);
  prerender_tab_helper->PrerenderSwappedIn();

  if (old_web_contents->NeedToFireBeforeUnload()) {
    // Schedule the delete to occur after the tab has run its unload handlers.
    on_close_web_contents_deleters_.push_back(
        new OnCloseWebContentsDeleter(this, old_web_contents));
    old_web_contents->GetRenderViewHost()->
        FirePageBeforeUnload(false);
  } else {
    // No unload handler to run, so delete asap.
    ScheduleDeleteOldWebContents(old_web_contents, NULL);
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

  ScopedVector<PrerenderData>::iterator it =
      FindIteratorForPrerenderContents(entry);
  DCHECK(it != active_prerenders_.end());

  // If this PrerenderContents is being deleted due to a cancellation any time
  // after the prerender has started then we need to create a dummy replacement
  // for PPLT accounting purposes for the Match Complete group. This is the case
  // if the cancellation is for any reason that would not occur in the control
  // group case.
  if (entry->prerendering_has_started() &&
      entry->match_complete_status() ==
          PrerenderContents::MATCH_COMPLETE_DEFAULT &&
      NeedMatchCompleteDummyForFinalStatus(final_status) &&
      ActuallyPrerendering()) {
    // TODO(tburkard): I'd like to DCHECK that we are actually prerendering.
    // However, what if new conditions are added and
    // NeedMatchCompleteDummyForFinalStatus is not being updated.  Not sure
    // what's the best thing to do here.  For now, I will just check whether
    // we are actually prerendering.
    (*it)->MakeIntoMatchCompleteReplacement();
  } else {
    to_delete_prerenders_.push_back(*it);
    active_prerenders_.weak_erase(it);
  }

  // Destroy the old WebContents relatively promptly to reduce resource usage,
  // and in the case of HTML5 media, reduce the chance of playing any sound.
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

  Origin prerender_origin = ORIGIN_NONE;
  if (prerender_manager->IsWebContentsPrerendering(web_contents,
                                                   &prerender_origin)) {
    prerender_manager->histograms_->RecordPageLoadTimeNotSwappedIn(
        prerender_origin, perceived_page_load_time, url);
    return;
  }

  bool was_prerender = prerender_manager->IsWebContentsPrerendered(
      web_contents, &prerender_origin);
  bool was_complete_prerender = was_prerender ||
      prerender_manager->WouldWebContentsBePrerendered(web_contents,
                                                       &prerender_origin);
  prerender_manager->histograms_->RecordPerceivedPageLoadTime(
      prerender_origin, perceived_page_load_time, was_prerender,
      was_complete_prerender, url);

  if (was_prerender) {
    prerender_manager->histograms_->RecordPercentLoadDoneAtSwapin(
        prerender_origin, fraction_plt_elapsed_at_swap_in);
  }
  if (prerender_manager->local_predictor_.get()) {
    prerender_manager->local_predictor_->
        OnPLTEventForURL(url, perceived_page_load_time);
  }
}

void PrerenderManager::RecordFractionPixelsFinalAtSwapin(
    content::WebContents* web_contents,
    double fraction) {
  Origin origin = ORIGIN_NONE;
  bool is_prerendered = IsWebContentsPrerendered(web_contents, &origin);
  DCHECK(is_prerendered);
  histograms_->RecordFractionPixelsFinalAtSwapin(origin, fraction);
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
    case PRERENDER_MODE_EXPERIMENT_MULTI_PRERENDER_GROUP:
      return "_Multi";
    case PRERENDER_MODE_EXPERIMENT_15MIN_TTL_GROUP:
      return "_15MinTTL";
    case PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP:
      return "_NoUse";
    case PRERENDER_MODE_MAX:
    default:
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
  return IsPrerenderingPossible() && !IsControlGroup(kNoExperiment);
}

// static
bool PrerenderManager::IsControlGroup(uint8 experiment_id) {
  return GetMode() == PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP ||
      IsControlGroupExperiment(experiment_id);
}

// static
bool PrerenderManager::IsNoUseGroup() {
  return GetMode() == PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP;
}

bool PrerenderManager::IsWebContentsPrerendering(
    WebContents* web_contents,
    Origin* origin) const {
  DCHECK(CalledOnValidThread());
  if (PrerenderContents* prerender_contents =
          GetPrerenderContents(web_contents)) {
    if (origin)
      *origin = prerender_contents->origin();
    return true;
  }

  // Also look through the pending-deletion list.
  for (ScopedVector<PrerenderData>::const_iterator it =
           to_delete_prerenders_.begin();
       it != to_delete_prerenders_.end();
       ++it) {
    if (PrerenderContents* prerender_contents = (*it)->contents()) {
      WebContents* prerender_web_contents =
          prerender_contents->prerender_contents();
      if (prerender_web_contents == web_contents) {
        if (origin)
          *origin = prerender_contents->origin();
        return true;
      }
    }
  }

  return false;
}

PrerenderContents* PrerenderManager::GetPrerenderContents(
    content::WebContents* web_contents) const {
  DCHECK(CalledOnValidThread());
  for (ScopedVector<PrerenderData>::const_iterator it =
           active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    WebContents* prerender_web_contents =
        (*it)->contents()->prerender_contents();
    if (prerender_web_contents == web_contents) {
      return (*it)->contents();
    }
  }
  return NULL;
}

const std::vector<WebContents*>
PrerenderManager::GetAllPrerenderingContents() const {
  DCHECK(CalledOnValidThread());
  std::vector<WebContents*> result;

  for (ScopedVector<PrerenderData>::const_iterator it =
           active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    if (WebContents* contents = (*it)->contents()->prerender_contents())
      result.push_back(contents);
  }

  return result;
}

void PrerenderManager::MarkWebContentsAsPrerendered(WebContents* web_contents,
                                                    Origin origin) {
  DCHECK(CalledOnValidThread());
  prerendered_web_contents_data_.insert(
      base::hash_map<content::WebContents*,
                     PrerenderedWebContentsData>::value_type(
                         web_contents, PrerenderedWebContentsData(origin)));
}

void PrerenderManager::MarkWebContentsAsWouldBePrerendered(
    WebContents* web_contents,
    Origin origin) {
  DCHECK(CalledOnValidThread());
  would_be_prerendered_map_.insert(
      base::hash_map<content::WebContents*,
                     WouldBePrerenderedWebContentsData>::value_type(
                         web_contents,
                         WouldBePrerenderedWebContentsData(origin)));
}

void PrerenderManager::MarkWebContentsAsNotPrerendered(
    WebContents* web_contents) {
  DCHECK(CalledOnValidThread());
  prerendered_web_contents_data_.erase(web_contents);
  base::hash_map<content::WebContents*, WouldBePrerenderedWebContentsData>::
      iterator it = would_be_prerendered_map_.find(web_contents);
  if (it != would_be_prerendered_map_.end()) {
    if (it->second.state ==
            WouldBePrerenderedWebContentsData::WAITING_FOR_PROVISIONAL_LOAD) {
      it->second.state =
          WouldBePrerenderedWebContentsData::SEEN_PROVISIONAL_LOAD;
    } else {
      would_be_prerendered_map_.erase(it);
    }
  }
}

bool PrerenderManager::IsWebContentsPrerendered(
    content::WebContents* web_contents,
    Origin* origin) const {
  DCHECK(CalledOnValidThread());
  base::hash_map<content::WebContents*, PrerenderedWebContentsData>::
      const_iterator it = prerendered_web_contents_data_.find(web_contents);
  if (it == prerendered_web_contents_data_.end())
    return false;
  if (origin)
    *origin = it->second.origin;
  return true;
}

bool PrerenderManager::WouldWebContentsBePrerendered(
    WebContents* web_contents,
    Origin* origin) const {
  DCHECK(CalledOnValidThread());
  base::hash_map<content::WebContents*, WouldBePrerenderedWebContentsData>::
      const_iterator it = would_be_prerendered_map_.find(web_contents);
  if (it == would_be_prerendered_map_.end())
    return false;
  if (origin)
    *origin = it->second.origin;
  return true;
}

bool PrerenderManager::HasRecentlyBeenNavigatedTo(Origin origin,
                                                  const GURL& url) {
  DCHECK(CalledOnValidThread());

  CleanUpOldNavigations();
  std::list<NavigationRecord>::const_reverse_iterator end = navigations_.rend();
  for (std::list<NavigationRecord>::const_reverse_iterator it =
           navigations_.rbegin();
       it != end;
       ++it) {
    if (it->url == url) {
      base::TimeDelta delta = GetCurrentTimeTicks() - it->time;
      histograms_->RecordTimeSinceLastRecentVisit(origin, delta);
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
  if (IsControlGroup(kNoExperiment))
    enabled_note += "(Control group: Not actually prerendering) ";
  if (IsNoUseGroup())
    enabled_note += "(No-use group: Not swapping in prerendered pages) ";
  if (GetMode() == PRERENDER_MODE_EXPERIMENT_15MIN_TTL_GROUP)
    enabled_note +=
        "(15 min TTL group: Extended prerender eviction to 15 mins) ";
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

void PrerenderManager::RecordNavigation(const GURL& url) {
  DCHECK(CalledOnValidThread());

  navigations_.push_back(NavigationRecord(url, GetCurrentTimeTicks()));
  CleanUpOldNavigations();
}

// protected
struct PrerenderManager::PrerenderData::OrderByExpiryTime {
  bool operator()(const PrerenderData* a, const PrerenderData* b) const {
    return a->expiry_time() < b->expiry_time();
  }
};

PrerenderManager::PrerenderData::PrerenderData(PrerenderManager* manager,
                                               PrerenderContents* contents,
                                               base::TimeTicks expiry_time)
    : manager_(manager),
      contents_(contents),
      handle_count_(0),
      expiry_time_(expiry_time) {
  DCHECK_NE(static_cast<PrerenderContents*>(NULL), contents_);
}

PrerenderManager::PrerenderData::~PrerenderData() {
}

void PrerenderManager::PrerenderData::MakeIntoMatchCompleteReplacement() {
  DCHECK(contents_);
  contents_->set_match_complete_status(
      PrerenderContents::MATCH_COMPLETE_REPLACED);
  PrerenderData* to_delete = new PrerenderData(manager_, contents_.release(),
                                               expiry_time_);
  contents_.reset(to_delete->contents_->CreateMatchCompleteReplacement());
  manager_->to_delete_prerenders_.push_back(to_delete);
}

void PrerenderManager::PrerenderData::OnHandleCreated(PrerenderHandle* handle) {
  DCHECK_NE(static_cast<PrerenderContents*>(NULL), contents_);
  ++handle_count_;
  contents_->AddObserver(handle);
}

void PrerenderManager::PrerenderData::OnHandleNavigatedAway(
    PrerenderHandle* handle) {
  DCHECK_LT(0, handle_count_);
  DCHECK_NE(static_cast<PrerenderContents*>(NULL), contents_);
  // We intentionally don't decrement the handle count here, so that the
  // prerender won't be canceled until it times out.
  manager_->SourceNavigatedAway(this);
}

void PrerenderManager::PrerenderData::OnHandleCanceled(
    PrerenderHandle* handle) {
  DCHECK_LT(0, handle_count_);
  DCHECK_NE(static_cast<PrerenderContents*>(NULL), contents_);

  if (--handle_count_ == 0) {
    // This will eventually remove this object from active_prerenders_.
    contents_->Destroy(FINAL_STATUS_CANCELLED);
  }
}

PrerenderContents* PrerenderManager::PrerenderData::ReleaseContents() {
  return contents_.release();
}

void PrerenderManager::SetPrerenderContentsFactory(
    PrerenderContents::Factory* prerender_contents_factory) {
  DCHECK(CalledOnValidThread());
  prerender_contents_factory_.reset(prerender_contents_factory);
}

void PrerenderManager::StartPendingPrerenders(
    const int process_id,
    ScopedVector<PrerenderContents::PendingPrerenderInfo>* pending_prerenders,
    content::SessionStorageNamespace* session_storage_namespace) {
  for (ScopedVector<PrerenderContents::PendingPrerenderInfo>::iterator
           it = pending_prerenders->begin();
       it != pending_prerenders->end(); ++it) {
    PrerenderContents::PendingPrerenderInfo* info = *it;
    PrerenderHandle* existing_prerender_handle =
        info->weak_prerender_handle.get();
    if (!existing_prerender_handle)
      continue;

    DCHECK(!existing_prerender_handle->IsPrerendering());
    DCHECK(process_id == -1 || session_storage_namespace);

    scoped_ptr<PrerenderHandle> new_prerender_handle(AddPrerender(
        info->origin, process_id,
        info->url, info->referrer, info->size,
        session_storage_namespace));
    if (new_prerender_handle) {
      // AddPrerender has returned a new prerender handle to us. We want to make
      // |existing_prerender_handle| active, so move the underlying
      // PrerenderData to our new handle.
      existing_prerender_handle->AdoptPrerenderDataFrom(
          new_prerender_handle.get());
      continue;
    }
  }
}

void PrerenderManager::SourceNavigatedAway(PrerenderData* prerender_data) {
  // The expiry time of our prerender data will likely change because of
  // this navigation. This requires a resort of active_prerenders_.
  ScopedVector<PrerenderData>::iterator it =
      std::find(active_prerenders_.begin(), active_prerenders_.end(),
                prerender_data);
  if (it == active_prerenders_.end())
    return;

  (*it)->set_expiry_time(
      std::min((*it)->expiry_time(),
               GetExpiryTimeForNavigatedAwayPrerender()));
  SortActivePrerenders();
}

// private
PrerenderHandle* PrerenderManager::AddPrerender(
    Origin origin,
    int process_id,
    const GURL& url_arg,
    const content::Referrer& referrer,
    const gfx::Size& size,
    SessionStorageNamespace* session_storage_namespace) {
  DCHECK(CalledOnValidThread());

  if (!IsEnabled())
    return NULL;

  if ((origin == ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN ||
       origin == ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN) &&
      IsGoogleSearchResultURL(referrer.url)) {
    origin = ORIGIN_GWS_PRERENDER;
  }

  GURL url = url_arg;
  GURL alias_url;
  uint8 experiment = GetQueryStringBasedExperiment(url_arg);
  if (IsControlGroup(experiment) &&
      MaybeGetQueryStringBasedAliasURL(url, &alias_url)) {
    url = alias_url;
  }

  // From here on, we will record a FinalStatus so we need to register with the
  // histogram tracking.
  histograms_->RecordPrerender(origin, url_arg);

  if (PrerenderData* preexisting_prerender_data =
          FindPrerenderData(url, session_storage_namespace)) {
    RecordFinalStatus(origin, experiment, FINAL_STATUS_DUPLICATE);
    return new PrerenderHandle(preexisting_prerender_data);
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
    return NULL;
  }
#endif

  // Check if enough time has passed since the last prerender.
  if (!DoesRateLimitAllowPrerender(origin)) {
    // Cancel the prerender. We could add it to the pending prerender list but
    // this doesn't make sense as the next prerender request will be triggered
    // by a navigation and is unlikely to be the same site.
    RecordFinalStatus(origin, experiment, FINAL_STATUS_RATE_LIMIT_EXCEEDED);
    return NULL;
  }

  PrerenderContents* prerender_contents = CreatePrerenderContents(
      url, referrer, origin, experiment);
  DCHECK(prerender_contents);
  active_prerenders_.push_back(
      new PrerenderData(this, prerender_contents,
                        GetExpiryTimeForNewPrerender()));
  if (!prerender_contents->Init()) {
    DCHECK(active_prerenders_.end() ==
           FindIteratorForPrerenderContents(prerender_contents));
    return NULL;
  }

  histograms_->RecordPrerenderStarted(origin);
  DCHECK(!prerender_contents->prerendering_has_started());

  PrerenderHandle* prerender_handle =
      new PrerenderHandle(active_prerenders_.back());
  SortActivePrerenders();

  last_prerender_start_time_ = GetCurrentTimeTicks();

  gfx::Size contents_size =
      size.IsEmpty() ? config_.default_tab_bounds.size() : size;

  prerender_contents->StartPrerendering(process_id, contents_size,
                                        session_storage_namespace);

  DCHECK(IsControlGroup(experiment) ||
         prerender_contents->prerendering_has_started());

  if (GetMode() == PRERENDER_MODE_EXPERIMENT_MULTI_PRERENDER_GROUP)
    histograms_->RecordConcurrency(active_prerenders_.size());

  StartSchedulingPeriodicCleanups();
  return prerender_handle;
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

void PrerenderManager::StopSchedulingPeriodicCleanups() {
  DCHECK(CalledOnValidThread());
  repeating_timer_.Stop();
}

void PrerenderManager::PeriodicCleanup() {
  DCHECK(CalledOnValidThread());
  DeleteOldWebContents();
  DeleteOldEntries();
  if (active_prerenders_.empty())
    StopSchedulingPeriodicCleanups();

  // Grab a copy of the current PrerenderContents pointers, so that we
  // will not interfere with potential deletions of the list.
  std::vector<PrerenderContents*>
      prerender_contents(active_prerenders_.size());
  std::transform(active_prerenders_.begin(), active_prerenders_.end(),
                 prerender_contents.begin(),
                 std::mem_fun(&PrerenderData::contents));

  // And now check for prerenders using too much memory.
  std::for_each(prerender_contents.begin(), prerender_contents.end(),
                std::mem_fun(
                    &PrerenderContents::DestroyWhenUsingTooManyResources));

  to_delete_prerenders_.clear();
}

void PrerenderManager::PostCleanupTask() {
  DCHECK(CalledOnValidThread());
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PrerenderManager::PeriodicCleanup,
                 weak_factory_.GetWeakPtr()));
}

base::TimeTicks PrerenderManager::GetExpiryTimeForNewPrerender() const {
  return GetCurrentTimeTicks() + config_.time_to_live;
}

base::TimeTicks PrerenderManager::GetExpiryTimeForNavigatedAwayPrerender()
    const {
  return GetCurrentTimeTicks() + config_.abandon_time_to_live;
}

void PrerenderManager::DeleteOldEntries() {
  DCHECK(CalledOnValidThread());
  while (!active_prerenders_.empty()) {
    PrerenderData* prerender_data = active_prerenders_.front();
    DCHECK(prerender_data);
    DCHECK(prerender_data->contents());

    if (prerender_data->expiry_time() > GetCurrentTimeTicks())
      return;
    prerender_data->contents()->Destroy(FINAL_STATUS_TIMED_OUT);
  }
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
      this, profile_, url, referrer, origin, experiment_id);
}

void PrerenderManager::SortActivePrerenders() {
  std::sort(active_prerenders_.begin(), active_prerenders_.end(),
            PrerenderData::OrderByExpiryTime());
}

PrerenderManager::PrerenderData* PrerenderManager::FindPrerenderData(
    const GURL& url,
    const SessionStorageNamespace* session_storage_namespace) {
  for (ScopedVector<PrerenderData>::iterator it = active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    if ((*it)->contents()->Matches(url, session_storage_namespace))
      return *it;
  }
  return NULL;
}

PrerenderManager::PrerenderData*
PrerenderManager::FindPrerenderDataForChildAndRoute(
    const int child_id, const int route_id) {
  for (ScopedVector<PrerenderData>::iterator it = active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    PrerenderContents* prerender_contents = (*it)->contents();

    int contents_child_id;
    if (!prerender_contents->GetChildId(&contents_child_id))
      continue;
    int contents_route_id;
    if (!prerender_contents->GetRouteId(&contents_route_id))
      continue;

    if (contents_child_id == child_id && contents_route_id == route_id)
      return *it;
  }
  return NULL;
}

ScopedVector<PrerenderManager::PrerenderData>::iterator
PrerenderManager::FindIteratorForPrerenderContents(
    PrerenderContents* prerender_contents) {
  for (ScopedVector<PrerenderData>::iterator it = active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    if (prerender_contents == (*it)->contents())
      return it;
  }
  return active_prerenders_.end();
}

bool PrerenderManager::DoesRateLimitAllowPrerender(Origin origin) const {
  DCHECK(CalledOnValidThread());
  base::TimeDelta elapsed_time =
      GetCurrentTimeTicks() - last_prerender_start_time_;
  histograms_->RecordTimeBetweenPrerenderRequests(origin, elapsed_time);
  if (!config_.rate_limit_enabled)
    return true;
  return elapsed_time >=
      base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs);
}

void PrerenderManager::DeleteOldWebContents() {
  while (!old_web_contents_list_.empty()) {
    WebContents* web_contents = old_web_contents_list_.front();
    old_web_contents_list_.pop_front();
    // TODO(dominich): should we use Instant Unload Handler here?
    delete web_contents;
  }
}

void PrerenderManager::CleanUpOldNavigations() {
  DCHECK(CalledOnValidThread());

  // Cutoff.  Navigations before this cutoff can be discarded.
  base::TimeTicks cutoff = GetCurrentTimeTicks() -
      base::TimeDelta::FromMilliseconds(kNavigationRecordWindowMs);
  while (!navigations_.empty()) {
    if (navigations_.front().time > cutoff)
      break;
    navigations_.pop_front();
  }
}

void PrerenderManager::ScheduleDeleteOldWebContents(
    WebContents* tab,
    OnCloseWebContentsDeleter* deleter) {
  old_web_contents_list_.push_back(tab);
  PostCleanupTask();

  if (deleter) {
    ScopedVector<OnCloseWebContentsDeleter>::iterator i = std::find(
        on_close_web_contents_deleters_.begin(),
        on_close_web_contents_deleters_.end(),
        deleter);
    DCHECK(i != on_close_web_contents_deleters_.end());
    on_close_web_contents_deleters_.erase(i);
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
  for (ScopedVector<PrerenderData>::const_iterator it =
           active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    if (Value* prerender_value = (*it)->contents()->GetAsValue())
      list_value->Append(prerender_value);
  }
  return list_value;
}

void PrerenderManager::DestroyAllContents(FinalStatus final_status) {
  DeleteOldWebContents();
  while (!active_prerenders_.empty()) {
    PrerenderContents* contents = active_prerenders_.front()->contents();
    contents->Destroy(final_status);
  }
  to_delete_prerenders_.clear();
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
